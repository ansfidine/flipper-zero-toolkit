/*
 * Module: subghz_replay.c
 * Purpose: Async signal replay from internal buffer
 */

#include "subghz_easy.h"

#define SPI_ACQUIRE()  furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz)
#define SPI_RELEASE()  furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz)

/* ── Stop replay ──────────────────────────────────────────────────────── */
/** replay_stop
 *  Preconditions: replaying == true
 *  Sequence: acquire → stop_async_tx → idle → release
 *  Postconditions: replaying = false, status = "Done" */
void replay_stop(SubGhzEasyApp* app) {
    furi_assert(app);
    if(!app->replaying) return;

    app->replaying = false;

    SPI_ACQUIRE();
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    SPI_RELEASE();

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "Done");
    furi_mutex_release(app->mutex);
}

/* ── ISR callback — feeds next LevelDuration to radio ──────────────────── */
/** replay_isr_cb
 *  Interrupt context. Reads signals[] + replay_idx (no lock — no concurrent access).
 *  End-of-buffer: returns level_duration_reset() which halts async TX. */
LevelDuration replay_isr_cb(void* context) {
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

/* ── Start replay ──────────────────────────────────────────────────────── */
/** replay_start
 *  Precondition: !replaying, signal_count > 0
 *  Sequence: acquire → reset → idle → set_freq → start_async_tx → release
 *  ISR registered: replay_isr_cb. */
void replay_start(SubGhzEasyApp* app) {
    furi_assert(app);
    if(app->replaying || app->signal_count == 0) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        strcpy(app->status, "No signal!");
        furi_mutex_release(app->mutex);
        return;
    }

    app->replaying = true;
    app->replay_idx = 0;

    SPI_ACQUIRE();
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(app->capture_freq);
    furi_hal_subghz_start_async_tx(replay_isr_cb, app);
    SPI_RELEASE();

    notification_message(app->notifications, &sequence_blink_blue_100);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "TX...");
    furi_mutex_release(app->mutex);
}
