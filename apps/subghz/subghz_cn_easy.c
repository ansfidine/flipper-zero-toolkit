#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_subghz.h>
#include <storage/storage.h>
#include <lib/toolbox/level_duration.h>

#define TAG "SubGHzEasy"

#define SIGNAL_SAMPLES_MAX 4096
#define SUBGHZ_FOLDER EXT_PATH("apps_data/subghz_cn")

// Chinese ISM bands only
#define FREQ_315    315000000
#define FREQ_433    433920000

typedef enum { Screen_Main, Screen_Capture, Screen_Replay } Screen;

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

    uint32_t frequency;
    float rssi;
    bool scanning;
    bool has_signal;

    // Signal capture
    SignalTiming* signals;
    uint16_t signal_count;
    uint16_t signal_capacity;
    bool capturing;
    uint32_t capture_freq;
    FuriTimer* capture_timer;

    // File list for replay
    char** files;
    uint8_t file_count;
    uint8_t selected_file_idx;
    bool replaying;
    uint16_t replay_idx;

    char status[64];
} SubGhzEasyApp;

static void stop_capture(SubGhzEasyApp* app) {
    if(!app->capturing) return;
    furi_hal_subghz_stop_async_rx();
    app->capturing = false;
    if(app->capture_timer) {
        furi_timer_stop(app->capture_timer);
    }
    furi_hal_subghz_idle();
    notification_message(app->notifications, &sequence_success);
    snprintf(app->status, sizeof(app->status), "Captured %d timings", app->signal_count);
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

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    uint32_t actual = furi_hal_subghz_set_frequency_and_path(app->capture_freq);
    UNUSED(actual);

    furi_hal_subghz_start_async_rx(capture_callback, app);
    furi_timer_start(app->capture_timer, 5000); // 5 second auto-stop
    notification_message(app->notifications, &sequence_blink_red_100);
    strcpy(app->status, "Capturing...");
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
        strcpy(app->status, "No signal!");
        return;
    }
    app->replaying = true;
    app->replay_idx = 0;

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(app->capture_freq);
    furi_hal_subghz_start_async_tx(replay_callback, app);
    notification_message(app->notifications, &sequence_blink_blue_100);
    strcpy(app->status, "Replaying...");
}

static void stop_replay(SubGhzEasyApp* app) {
    if(!app->replaying) return;
    app->replaying = false;
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    strcpy(app->status, "Replay done");
}

static void save_signal(SubGhzEasyApp* app) {
    if(app->signal_count == 0) {
        strcpy(app->status, "Nothing to save!");
        return;
    }

    // Ensure folder exists
    if(!storage_dir_exists(app->storage, SUBGHZ_FOLDER)) {
        storage_simply_mkdir(app->storage, SUBGHZ_FOLDER);
    }

    char path[128];
    int idx = 1;
    do {
        snprintf(path, sizeof(path), "%s/signal_%lu_%03d.sub", SUBGHZ_FOLDER, app->capture_freq, idx);
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
        // Break line every ~15 values to keep format clean
        if((i+1) % 15 == 0 && i < app->signal_count - 1) {
            furi_string_cat_printf(buf, "\nRAW_Data: ");
        }
    }
    furi_string_cat_printf(buf, "\n");

    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        strcpy(app->status, "Save failed!");
        storage_file_free(file);
        furi_string_free(buf);
        return;
    }

    const char* data = furi_string_get_cstr(buf);
    storage_file_write(file, data, strlen(data));
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(buf);

    snprintf(app->status, sizeof(app->status), "Saved!");
}

static void scan_rssi(SubGhzEasyApp* app) {
    if(app->capturing || app->replaying) return;
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(app->frequency);
    furi_hal_subghz_rx();
    app->rssi = furi_hal_subghz_get_rssi();
    app->has_signal = app->rssi > -67.0f;
    furi_hal_subghz_idle();
}

static void draw_callback(Canvas* canvas, void* ctx) {
    SubGhzEasyApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(app->screen == Screen_Main) {
        canvas_draw_str(canvas, 2, 10, "Sub-GHz Easy");
        canvas_draw_line(canvas, 0, 12, 128, 12);

        canvas_set_font(canvas, FontSecondary);
        char freq_str[32];
        snprintf(freq_str, sizeof(freq_str), "Freq: %lu MHz", app->frequency / 1000000);
        canvas_draw_str(canvas, 2, 24, freq_str);

        snprintf(freq_str, sizeof(freq_str), "RSSI: %.1f dBm", (double)app->rssi);
        canvas_draw_str(canvas, 2, 36, freq_str);

        if(app->has_signal) {
            canvas_draw_str(canvas, 2, 48, "SIGNAL DETECTED!");
        } else {
            canvas_draw_str(canvas, 2, 48, "No signal");
        }

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 60, app->status);
        canvas_draw_str(canvas, 2, 63, "UP/DN=Freq | OK=Capture");
    } else if(app->screen == Screen_Capture) {
        canvas_draw_str(canvas, 2, 10, "Capture Signal");
        canvas_draw_line(canvas, 0, 12, 128, 12);
        canvas_set_font(canvas, FontSecondary);

        char buf[64];
        snprintf(buf, sizeof(buf), "Freq: %lu MHz", app->capture_freq / 1000000);
        canvas_draw_str(canvas, 2, 24, buf);
        snprintf(buf, sizeof(buf), "Samples: %d/%d", app->signal_count, SIGNAL_SAMPLES_MAX);
        canvas_draw_str(canvas, 2, 36, buf);

        if(app->capturing) {
            canvas_draw_str(canvas, 2, 48, "REC...");
        } else if(app->signal_count > 0) {
            canvas_draw_str(canvas, 2, 48, "Done!");
        } else {
            canvas_draw_str(canvas, 2, 48, "Ready");
        }

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 60, app->status);
        canvas_draw_str(canvas, 2, 63, "OK=Start | DN=Save | Back");
    } else if(app->screen == Screen_Replay) {
        canvas_draw_str(canvas, 2, 10, "Replay");
        canvas_draw_line(canvas, 0, 12, 128, 12);
        canvas_set_font(canvas, FontSecondary);

        if(app->signal_count > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Loaded: %d timings", app->signal_count);
            canvas_draw_str(canvas, 2, 24, buf);
            snprintf(buf, sizeof(buf), "Freq: %lu MHz", app->capture_freq / 1000000);
            canvas_draw_str(canvas, 2, 36, buf);
            canvas_draw_str(canvas, 2, 48, app->replaying ? "PLAYING..." : "Ready");
        } else {
            canvas_draw_str(canvas, 2, 24, "No signal loaded");
            canvas_draw_str(canvas, 2, 36, "Capture first!");
        }

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 63, "OK=Play | Back");
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    SubGhzEasyApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

static void frequency_adjust(SubGhzEasyApp* app, int delta) {
    if(app->capturing || app->replaying) return;
    int64_t new_freq = (int64_t)app->frequency + (delta * 1000000);
    if(new_freq < 314000000) new_freq = 435000000;
    if(new_freq > 435000000) new_freq = 314000000;
    app->frequency = (uint32_t)new_freq;
    scan_rssi(app);
}

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
    app->scanning = false;
    app->has_signal = false;
    app->signal_count = 0;
    app->signal_capacity = SIGNAL_SAMPLES_MAX;
    app->signals = malloc(sizeof(SignalTiming) * SIGNAL_SAMPLES_MAX);
    app->capturing = false;
    app->replaying = false;
    app->capture_timer = furi_timer_alloc(capture_timeout_callback, FuriTimerTypeOnce, app);
    strcpy(app->status, "Ready");

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    scan_rssi(app);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort) {
                switch(event.key) {
                case InputKeyUp:
                    if(app->screen == Screen_Main) frequency_adjust(app, +1);
                    break;
                case InputKeyDown:
                    if(app->screen == Screen_Main) frequency_adjust(app, -1);
                    else if(app->screen == Screen_Capture && !app->capturing) save_signal(app);
                    break;
                case InputKeyLeft:
                    break;
                case InputKeyRight:
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

    if(app->capturing) stop_capture(app);
    if(app->replaying) stop_replay(app);

    furi_hal_subghz_sleep();
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    free(app->signals);
    free(app);
    return 0;
}
