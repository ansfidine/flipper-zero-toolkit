/*
 * Module: subghz_ui.c
 * Purpose: ViewPort draw callback + input handling + screen state machine
 * Screen: 128×64 monochrome. All text uses 1px spacing to avoid overlap.
 */

#include "subghz_easy.h"

/* ── Draw main screen ─────────────────────────────────────────────────── &/── 
 *  y=8:   Title "SubGHz CN"
 *  y=10:  Separator line
 *  y=20:  Frequency (FontSecondary)
 *  y=30:  RSSI
 *  y=40:  Signal indicator
 *  y=50:  Status
 *  y=62:  Help text
 */
static void draw_main(Canvas* canvas, SubGhzEasyApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, "SubGHz CN");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    canvas_set_font(canvas, FontSecondary);
    char buf[32];

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint32_t freq = app->frequency;
    float rssi = app->rssi;
    bool has = app->has_signal;
    char status[48];
    strncpy(status, app->status, sizeof(status));
    furi_mutex_release(app->mutex);

    snprintf(buf, sizeof(buf), "Freq: %lu.00 MHz", freq / 1000000);
    canvas_draw_str(canvas, 2, 20, buf);

    snprintf(buf, sizeof(buf), "RSSI: %.1f dBm", (double)rssi);
    canvas_draw_str(canvas, 2, 30, buf);

    if(has) {
        canvas_draw_str(canvas, 2, 40, "SIGNAL!");
    } else {
        canvas_draw_str(canvas, 2, 40, "No signal");
    }

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 50, status);
    canvas_draw_str(canvas, 2, 62, "L/R=Freq | OK=Cap");
}

/* ── Draw capture screen ───────────────────────────────────────────────── */
static void draw_capture(Canvas* canvas, SubGhzEasyApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, "Capture");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    canvas_set_font(canvas, FontSecondary);
    char buf[64];

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint32_t cap_freq = app->capture_freq;
    uint16_t count = app->signal_count;
    uint16_t cap_max = app->signal_capacity;
    bool capturing = app->capturing;
    char status[48];
    strncpy(status, app->status, sizeof(status));
    furi_mutex_release(app->mutex);

    snprintf(buf, sizeof(buf), "Freq: %lu.00", cap_freq / 1000000);
    canvas_draw_str(canvas, 2, 20, buf);
    snprintf(buf, sizeof(buf), "Samples: %d/%d", count, cap_max);
    canvas_draw_str(canvas, 2, 30, buf);

    if(capturing) {
        canvas_draw_str(canvas, 2, 42, "RECORDING...");
    } else if(count > 0) {
        canvas_draw_str(canvas, 2, 42, "Ready");
    } else {
        canvas_draw_str(canvas, 2, 42, "Standby");
    }

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 54, status);
    canvas_draw_str(canvas, 2, 62, "OK=REC | DN=Save→Replay");
}

/* ── Draw replay screen ───────────────────────────────────────────────── */
static void draw_replay(Canvas* canvas, SubGhzEasyApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 8, "Replay");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    canvas_set_font(canvas, FontSecondary);
    char buf[64];

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint16_t count = app->signal_count;
    uint32_t cap_freq = app->capture_freq;
    bool replaying = app->replaying;
    furi_mutex_release(app->mutex);

    if(count > 0) {
        snprintf(buf, sizeof(buf), "Timings: %d", count);
        canvas_draw_str(canvas, 2, 20, buf);
        snprintf(buf, sizeof(buf), "Freq: %lu.00", cap_freq / 1000000);
        canvas_draw_str(canvas, 2, 30, buf);
        canvas_draw_str(canvas, 2, 42, replaying ? "PLAYING..." : "Ready");
    } else {
        canvas_draw_str(canvas, 2, 20, "No capture");
        canvas_draw_str(canvas, 2, 30, "Capture first!");
    }

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 62, "OK=Play | Back");
}

/* ── ViewPort draw callback ────────────────────────────────────────────── */
void ui_draw(Canvas* canvas, void* ctx) {
    SubGhzEasyApp* app = ctx;
    furi_assert(app);

    canvas_clear(canvas);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    Screen screen = app->screen;
    furi_mutex_release(app->mutex);

    switch(screen) {
    case Screen_Main:
        draw_main(canvas, app);
        break;
    case Screen_Capture:
        draw_capture(canvas, app);
        break;
    case Screen_Replay:
        draw_replay(canvas, app);
        break;
    }
}

/* ── ViewPort input callback ────────────────────────────────────────────── */
void ui_input(InputEvent* event, void* ctx) {
    SubGhzEasyApp* app = ctx;
    furi_assert(app);
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

/* ── Toggle frequency (315 ↔ 433.92) with mutex ─────────────────────────── */
void frequency_toggle(SubGhzEasyApp* app) {
    furi_assert(app);

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
