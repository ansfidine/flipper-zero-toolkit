/*
 * Module: subghz_main.c
 * Purpose: App entry point — alloc, event loop, graceful shutdown
 * Entry point: subghz_cn_easy_app (declared in .fam)
 */

#include "subghz_easy.h"

/* ── Screen transition helpers ──────────────────────────────────────────── */
static void screen_return_main(SubGhzEasyApp* app) {
    furi_assert(app);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->screen = Screen_Main;
    furi_mutex_release(app->mutex);
}

static void screen_goto_capture(SubGhzEasyApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->screen = Screen_Capture;
    furi_mutex_release(app->mutex);
}

static void screen_goto_replay(SubGhzEasyApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->screen = Screen_Replay;
    furi_mutex_release(app->mutex);
}
/** input_dispatch
 *  Handles all key inputs based on current screen + state.
 *  No blocking calls — radio ops delegate to worker or immediate SPI wrap. */
static void input_dispatch(SubGhzEasyApp* app, InputEvent* event) {
    if(event->type != InputTypeShort) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    Screen screen = app->screen;
    bool capturing = app->capturing;
    bool replaying = app->replaying;
    furi_mutex_release(app->mutex);

    switch(event->key) {
    case InputKeyLeft:
        if(screen == Screen_Main) frequency_toggle(app);
        break;
    case InputKeyRight:
        if(screen == Screen_Main) frequency_toggle(app);
        break;
    case InputKeyUp:
        break;
    case InputKeyDown:
        if(screen == Screen_Capture && !capturing) {
            file_save_signal(app);
            screen_goto_replay(app);
        }
        break;
    case InputKeyOk:
        switch(screen) {
        case Screen_Main:
            screen_goto_capture(app);
            break;
        case Screen_Capture:
            if(capturing) capture_stop(app);
            else capture_start(app);
            break;
        case Screen_Replay:
            if(replaying) replay_stop(app);
            else replay_start(app);
            break;
        }
        break;
    case InputKeyBack:
        switch(screen) {
        case Screen_Main:
            // Exit handled by caller loop
            break;
        case Screen_Capture:
            if(capturing) capture_stop(app);
            screen_return_main(app);
            break;
        case Screen_Replay:
            if(replaying) replay_stop(app);
            screen_return_main(app);
            break;
        }
        break;
    default:
        break;
    }
}

/* ── App entry point ──────────────────────────────────────────────────── */
int32_t subghz_cn_easy_app(void* p) {
    UNUSED(p);

    /* ── Allocate app context ─────────────────────────────────────────── */
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
        furi_message_queue_free(app->input_queue);
        view_port_free(app->view_port);
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_STORAGE);
        free(app);
        return -1;
    }
    app->capturing = false;
    app->replaying = false;
    app->capture_timer = furi_timer_alloc(capture_timeout_cb, FuriTimerTypeOnce, app);
    strcpy(app->status, "Ready");

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->worker_cmd = WorkerCmd_Scan;

    /* ── Register UI callbacks ────────────────────────────────────────── */
    view_port_draw_callback_set(app->view_port, ui_draw, app);
    view_port_input_callback_set(app->view_port, ui_input, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    /* ── Radio init (expansion disabled + CC1101 idle) ─────────────────── */
    radio_init(app);

    /* ── Start background worker ────────────────────────────────────────── */
    app->worker = worker_alloc(app);
    worker_start(app->worker);

    /* ── Main event loop ─────────────────────────────────────────────── */
    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if(app->screen == Screen_Main) running = false;
                furi_mutex_release(app->mutex);
            }
            if(running) input_dispatch(app, &event);
        }
    }

    /* ── Graceful shutdown ─────────────────────────────────────────────── */
    if(app->capturing) capture_stop(app);
    if(app->replaying) replay_stop(app);
    worker_stop(app);
    radio_shutdown(app);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_timer_free(app->capture_timer);
    free(app->signals);
    free(app);

    return 0;
}
