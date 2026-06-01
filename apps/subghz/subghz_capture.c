/*
 * Module: subghz_capture.c
 * Purpose: Async signal capture with buffer management + auto-stop timer
 */

#include "subghz_easy.h"

#define SPI_ACQUIRE()  furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz)
#define SPI_RELEASE()  furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz)

/* ── Stop capture (called from timer OR user input) ───────────────────── */
/** capture_stop
 *  Preconditions: capturing == true
 *  Sequence: SPI acquire → stop_async_rx → idle → release → kill timer
 *  Postconditions: capturing = false, status = "Done: N" */
void capture_stop(SubGhzEasyApp* app) {
    furi_assert(app);

    SPI_ACQUIRE();
    furi_hal_subghz_stop_async_rx();
    furi_hal_subghz_idle();
    SPI_RELEASE();

    app->capturing = false;
    if(app->capture_timer) {
        furi_timer_stop(app->capture_timer);
    }

    notification_message(app->notifications, &sequence_success);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(app->status, sizeof(app->status), "Done: %d", app->signal_count);
    furi_mutex_release(app->mutex);
}

/* ── Timer callback (SYS context) ──────────────────────────────────────── */
/** capture_timeout_cb
 *  Called when 5000ms timer expires.
 *  Marshals to capture_stop via context pointer. */
void capture_timeout_cb(void* context) {
    capture_stop((SubGhzEasyApp*)context);
}

/* ── ISR callback — appends sample to ring-like buffer ─────────────────── */
/** capture_isr_cb
 *  Interrupt context. NO locks, NO allocation.
 *  Only writer to signals[] during active capture.
 *  Overflow: sets app->capturing = false to signal end, main thread cleans up. */
void capture_isr_cb(bool level, uint32_t duration, void* context) {
    SubGhzEasyApp* app = context;
    if(!app->capturing) return;
    if(app->signal_count >= app->signal_capacity) {
        app->capturing = false; /* signal overflow — main thread will stop hardware */
        return;
    }
    app->signals[app->signal_count].level = level;
    app->signals[app->signal_count].duration = duration;
    app->signal_count++;
}

/* ── Start capture ──────────────────────────────────────────────────────── */
/** capture_start
 *  Sequence: SPI acquire → reset → idle → set_freq → start_async_rx → release → arm timer
 *  ISR registered: capture_isr_cb.
 *  Precondition: !capturing && !replaying. */
void capture_start(SubGhzEasyApp* app) {
    furi_assert(app);
    if(app->capturing || app->replaying) return;

    app->signal_count = 0;
    app->capturing = true;
    app->capture_freq = app->frequency;

    SPI_ACQUIRE();
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    uint32_t actual = furi_hal_subghz_set_frequency_and_path(app->capture_freq);
    UNUSED(actual);
    furi_hal_subghz_start_async_rx(capture_isr_cb, app);
    SPI_RELEASE();

    furi_timer_start(app->capture_timer, CAPTURE_TIMEOUT_MS);
    notification_message(app->notifications, &sequence_blink_red_100);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strcpy(app->status, "REC...");
    furi_mutex_release(app->mutex);
}
