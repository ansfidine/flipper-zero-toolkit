#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_subghz.h>
#include <furi_hal_spi.h>
#include <storage/storage.h>
#include <lib/toolbox/level_duration.h>
#include <expansion/expansion.h>

#define TAG "SubGHz_CNEasy"
#define SIGNAL_SAMPLES_MAX 2048
#define SUBGHZ_FOLDER EXT_PATH("apps_data/subghz_cn")

// Only two Chinese ISM bands as Boss specified
#define FREQ_315    315000000
#define FREQ_433    433920000

// PLL settle delay after freq change (official: 2ms)
#define CC1101_PLL_SETTLE_MS 2

typedef enum {
    Screen_Main,
    Screen_Capture,
    Screen_Replay
} Screen;

// Worker commands
typedef enum {
    WorkerCmd_Idle,
    WorkerCmd_Scan,
    WorkerCmd_Stop
} WorkerCmd;

typedef struct {
    bool level;
    uint32_t duration;
} SignalTiming;

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    Screen screen;
    Storage* storage;

    // Radio state — protected by mutex
    uint32_t frequency;
    float rssi;
    bool has_signal;

    // Signal capture
    SignalTiming* signals;
    uint16_t signal_count;
    uint16_t signal_capacity;
    bool capturing;
    uint32_t capture_freq;
    FuriTimer* capture_timer;

    // Replay
    bool replaying;
    uint16_t replay_idx;

    // Thread safety + expansion
    FuriMutex* mutex;
    FuriThread* worker;
    WorkerCmd worker_cmd;
    Expansion* expansion;

    char status[48];
} SubGhzEasyApp;

/* ── SPI-safe + expansion-safe radio helpers ───────────────────────────── */

__attribute__((unused)) static void radio_setup_unused(void) {
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
}

__attribute__((unused)) static void radio_sleep_unused(void) {
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}

/* ── Capture / Replay ────────────────────────────────────────────────── */

static void stop_capture(SubGhzEasyApp* app) {
    if(!app->capturing) return;
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_subghz_stop_async_rx();
    furi_hal_subghz_idle();
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    app->capturing = false;
    if(app->capture_timer) {
        furi_timer_stop(app->capture_timer);
    }
    notification_message(app->notifications, &sequence_success);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(app->status, sizeof(app->status), "Done: %d", app->signal_count);
    furi_mutex_release(app->mutex);
}

static void capture_timeout_callback(void* context) {
    SubGhzEasyApp* app = context;
    stop_capture(app);
}

static void capture_callback(bool level, uint32_t duration, void* context) {
    SubGhzEasyApp* app = context;
    if(!app->capturing) return;
    if(app->signal_count >= app->signal_capacity) {
        stop_capture(app);
        return;
    }
    app->signals[app->signal_count].level = level;
    app->signals[app->signal_count].duration = duration;
    app->signal_count++;
}

static void start_capture(SubGhzEasyApp* app) {
    if(app->capturing) return;
    app->signal_count = 0;
    app->capturing = true;
    app->capture_freq = app->frequency;

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    uint32_t actual = furi_hal_subghz_set_frequency_and_path(app->capture_freq);
    UNUSED(actual);
    furi_hal_subghz_start_async_rx(capture_callback, app);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    furi_timer_start(app->capture_timer, 5000);
    notification_message(app->notifications, &sequence_blink_red_100);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "REC...");
    furi_mutex_release(app->mutex);
}

static LevelDuration replay_callback(void* context) {
    SubGhzEasyApp* app = context;
    if(!app->replaying) return level_duration_reset();
    if(app->replay_idx >= app->signal_count) {
        app->replaying = false;
        return level_duration_reset();
    }
    SignalTiming* st = &app->signals[app->replay_idx];
    app->replay_idx++;
    return level_duration_make(st->level, st->duration);
}

static void start_replay(SubGhzEasyApp* app) {
    if(app->replaying) return;
    if(app->signal_count == 0) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        strcpy(app->status, "No signal!");
        furi_mutex_release(app->mutex);
        return;
    }
    app->replaying = true;
    app->replay_idx = 0;

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(app->capture_freq);
    furi_hal_subghz_start_async_tx(replay_callback, app);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    notification_message(app->notifications, &sequence_blink_blue_100);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "TX...");
    furi_mutex_release(app->mutex);
}

static void stop_replay(SubGhzEasyApp* app) {
    if(!app->replaying) return;
    app->replaying = false;
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "Done");
    furi_mutex_release(app->mutex);
}

/* ── File save ───────────────────────────────────────────────────────── */

static void save_signal(SubGhzEasyApp* app) {
    if(app->signal_count == 0) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        strcpy(app->status, "Nothing!");
        furi_mutex_release(app->mutex);
        return;
    }

    if(!storage_dir_exists(app->storage, SUBGHZ_FOLDER)) {
        storage_simply_mkdir(app->storage, SUBGHZ_FOLDER);
    }

    char path[128];
    int idx = 1;
    do {
        snprintf(path, sizeof(path), "%s/sig_%lu_%03d.sub", SUBGHZ_FOLDER, app->capture_freq / 1000000, idx);
        idx++;
    } while(storage_file_exists(app->storage, path));

    FuriString* buf = furi_string_alloc();
    furi_string_printf(buf, "Filetype: Flipper SubGhz Key File\n");
    furi_string_cat_printf(buf, "Version: 1\n");
    furi_string_cat_printf(buf, "Frequency: %lu\n", app->capture_freq);
    furi_string_cat_printf(buf, "Preset: FuriHalSubGhzPresetOok650Async\n");
    furi_string_cat_printf(buf, "Protocol: RAW\n");
    furi_string_cat_printf(buf, "RAW_Data: ");

    for(uint16_t i = 0; i < app->signal_count; i++) {
        int32_t us = app->signals[i].duration;
        if(!app->signals[i].level) us = -us;
        furi_string_cat_printf(buf, "%ld ", us);
        if((i + 1) % 12 == 0 && i < app->signal_count - 1) {
            furi_string_cat_printf(buf, "\nRAW_Data: ");
        }
    }
    furi_string_cat_printf(buf, "\n");

    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        strcpy(app->status, "Save err");
        furi_mutex_release(app->mutex);
        storage_file_free(file);
        furi_string_free(buf);
        return;
    }

    const char* data = furi_string_get_cstr(buf);
    storage_file_write(file, data, strlen(data));
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(buf);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "Saved!");
    furi_mutex_release(app->mutex);
}

/* ── Background worker thread for RSSI ────────────────────────────── */

static void worker_update_rssi(SubGhzEasyApp* app) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(app->frequency);
    furi_hal_subghz_rx();

    furi_delay_ms(CC1101_PLL_SETTLE_MS);

    float rssi = furi_hal_subghz_get_rssi();
    if(rssi > 0.0f) rssi = 0.0f;
    if(rssi < -127.0f) rssi = -127.0f;

    furi_hal_subghz_idle();
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->rssi = rssi;
    app->has_signal = (rssi > -67.0f);
    furi_mutex_release(app->mutex);
}

static int32_t subghz_easy_worker_thread(void* context) {
    SubGhzEasyApp* app = context;
    while(true) {
        WorkerCmd cmd;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        cmd = app->worker_cmd;
        furi_mutex_release(app->mutex);

        if(cmd == WorkerCmd_Stop) break;
        if(cmd == WorkerCmd_Scan && !app->capturing && !app->replaying) {
            worker_update_rssi(app);
        }
        furi_delay_ms(500); // 2Hz update rate
    }
    return 0;
}

/* ── UI ────────────────────────────────────────────────────────────── */

static void draw_callback(Canvas* canvas, void* ctx) {
    SubGhzEasyApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint32_t freq = app->frequency;
    float rssi = app->rssi;
    bool has_signal = app->has_signal;
    Screen screen = app->screen;
    uint16_t signal_count = app->signal_count;
    uint16_t signal_capacity = app->signal_capacity;
    bool capturing = app->capturing;
    bool replaying = app->replaying;
    uint32_t capture_freq = app->capture_freq;
    char status[48];
    strncpy(status, app->status, sizeof(status));
    furi_mutex_release(app->mutex);

    if(screen == Screen_Main) {
        canvas_draw_str(canvas, 2, 8, "SubGHz CN");
        canvas_draw_line(canvas, 0, 10, 128, 10);

        canvas_set_font(canvas, FontSecondary);
        char freq_str[32];
        snprintf(freq_str, sizeof(freq_str), "Freq: %lu.00 MHz", freq / 1000000);
        canvas_draw_str(canvas, 2, 20, freq_str);

        snprintf(freq_str, sizeof(freq_str), "RSSI: %.1f dBm", (double)rssi);
        canvas_draw_str(canvas, 2, 30, freq_str);

        if(has_signal) {
            canvas_draw_str(canvas, 2, 40, "SIGNAL!");
        } else {
            canvas_draw_str(canvas, 2, 40, "No signal");
        }

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 50, status);
        canvas_draw_str(canvas, 2, 62, "L/R=Freq | OK=Cap");
    } else if(screen == Screen_Capture) {
        canvas_draw_str(canvas, 2, 8, "Capture");
        canvas_draw_line(canvas, 0, 10, 128, 10);
        canvas_set_font(canvas, FontSecondary);

        char buf[64];
        snprintf(buf, sizeof(buf), "Freq: %lu.00", capture_freq / 1000000);
        canvas_draw_str(canvas, 2, 20, buf);
        snprintf(buf, sizeof(buf), "Samples: %d/%d", signal_count, signal_capacity);
        canvas_draw_str(canvas, 2, 30, buf);

        if(capturing) {
            canvas_draw_str(canvas, 2, 42, "RECORDING...");
        } else if(signal_count > 0) {
            canvas_draw_str(canvas, 2, 42, "Ready");
        } else {
            canvas_draw_str(canvas, 2, 42, "Standby");
        }

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 54, status);
        canvas_draw_str(canvas, 2, 62, "OK=REC | DN=Save");
    } else if(screen == Screen_Replay) {
        canvas_draw_str(canvas, 2, 8, "Replay");
        canvas_draw_line(canvas, 0, 10, 128, 10);
        canvas_set_font(canvas, FontSecondary);

        if(signal_count > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Timings: %d", signal_count);
            canvas_draw_str(canvas, 2, 20, buf);
            snprintf(buf, sizeof(buf), "Freq: %lu.00", capture_freq / 1000000);
            canvas_draw_str(canvas, 2, 30, buf);
            canvas_draw_str(canvas, 2, 42, replaying ? "PLAYING..." : "Ready");
        } else {
            canvas_draw_str(canvas, 2, 20, "No capture");
            canvas_draw_str(canvas, 2, 30, "Capture first!");
        }

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 62, "OK=Play | Back");
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    SubGhzEasyApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

static void toggle_frequency(SubGhzEasyApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->capturing || app->replaying) {
        furi_mutex_release(app->mutex);
        return;
    }
    if(app->frequency == FREQ_315) {
        app->frequency = FREQ_433;
    } else {
        app->frequency = FREQ_315;
    }
    furi_mutex_release(app->mutex);
}

/* ── Main ────────────────────────────────────────────────────────────── */

int32_t subghz_cn_easy_app(void* p) {
    UNUSED(p);

    SubGhzEasyApp* app = malloc(sizeof(SubGhzEasyApp));
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->screen = Screen_Main;
    app->frequency = FREQ_433;
    app->rssi = -96.0f;
    app->has_signal = false;
    app->signal_count = 0;
    app->signal_capacity = SIGNAL_SAMPLES_MAX;
    app->signals = malloc(sizeof(SignalTiming) * SIGNAL_SAMPLES_MAX);
    if(app->signals == NULL) {
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_STORAGE);
        furi_timer_free(app->capture_timer);
        furi_message_queue_free(app->input_queue);
        view_port_free(app->view_port);
        free(app);
        return -1;
    }
    app->capturing = false;
    app->replaying = false;
    app->capture_timer = furi_timer_alloc(capture_timeout_callback, FuriTimerTypeOnce, app);
    strcpy(app->status, "Ready");

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->worker_cmd = WorkerCmd_Scan;
    app->worker = furi_thread_alloc();
    furi_thread_set_name(app->worker, "SubGhzEasyWorker");
    furi_thread_set_stack_size(app->worker, 4096);
    furi_thread_set_context(app->worker, app);
    furi_thread_set_callback(app->worker, subghz_easy_worker_thread);

    // Disable expansion protocol to free GPIO 15/16 for CC1101
    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Initial radio setup with SPI safety
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    furi_thread_start(app->worker);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort) {
                switch(event.key) {
                case InputKeyUp:
                    break;
                case InputKeyDown:
                    if(app->screen == Screen_Capture && !app->capturing) {
                        save_signal(app);
                    }
                    break;
                case InputKeyLeft:
                    if(app->screen == Screen_Main) toggle_frequency(app);
                    break;
                case InputKeyRight:
                    if(app->screen == Screen_Main) toggle_frequency(app);
                    break;
                case InputKeyOk:
                    if(app->screen == Screen_Main) {
                        app->screen = Screen_Capture;
                    } else if(app->screen == Screen_Capture) {
                        if(app->capturing) stop_capture(app);
                        else start_capture(app);
                    } else if(app->screen == Screen_Replay) {
                        if(app->replaying) stop_replay(app);
                        else start_replay(app);
                    }
                    break;
                case InputKeyBack:
                    if(app->screen == Screen_Main) {
                        running = false;
                    } else if(app->screen == Screen_Capture) {
                        if(app->capturing) stop_capture(app);
                        app->screen = Screen_Main;
                    } else if(app->screen == Screen_Replay) {
                        if(app->replaying) stop_replay(app);
                        app->screen = Screen_Main;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    // Shutdown
    if(app->capturing) stop_capture(app);
    if(app->replaying) stop_replay(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->worker_cmd = WorkerCmd_Stop;
    furi_mutex_release(app->mutex);

    furi_thread_join(app->worker);
    furi_thread_free(app->worker);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);

    // Re-enable expansion protocol
    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    free(app->signals);
    free(app);
    return 0;
}
