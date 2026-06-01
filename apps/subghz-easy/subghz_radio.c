/*
 * Module: subghz_radio.c
 * Purpose: CC1101 radio control with SPI safety + expansion guard
 *
 * Boss: Ansfidine Youssouf
 */

#include "subghz_easy.h"

/* ── SPI macros to keep code clean and prevent unbalanced acquire ──────── */
#define SPI_ACQUIRE()  furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz)
#define SPI_RELEASE()  furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz)

/* ── Disable expansion and put radio into safe idle state ──────────────── */
/** radio_init
 *  Entry code. Prevents GPIO 15/16 conflict before any radio access.
 *  Sequence: disable expansion → acquire SPI → reset → idle → release. */
void radio_init(SubGhzEasyApp* app) {
    furi_assert(app);

    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion); // free GPIO 15/16

    SPI_ACQUIRE();
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    SPI_RELEASE();
}

/* ── Put radio to sleep, re-enable expansion ─────────────────────────── */
/** radio_shutdown
 *  Exit code. Re-enable expansion protocol after radio done. */
void radio_shutdown(SubGhzEasyApp* app) {
    furi_assert(app);

    SPI_ACQUIRE();
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    SPI_RELEASE();

    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);
    app->expansion = NULL;
}

/* ── Single-shot RSSI scan with full SPI lock discipline ─────────────── */
/** scan_frequency_rssi
 *  Full SPI sequence:
 *    acquire → idle → set_freq → rx → release
 *    delay 2ms (PLL settle)
 *    get_rssi()
 *    acquire → idle → release
 *
 *  Mutex: caller must hold mutex before reading/writing app state.
 *  Returns: clamped RSSI in dBm (range [-127.0, 0.0]). */
float scan_frequency_rssi(SubGhzEasyApp* app) {
    furi_assert(app);

    float rssi = -127.0f;

    SPI_ACQUIRE();
    furi_hal_subghz_idle();
    furi_hal_subghz_set_frequency_and_path(app->frequency);
    furi_hal_subghz_rx();

    furi_delay_ms(PLL_SETTLE_MS);

    rssi = furi_hal_subghz_get_rssi();
    if(rssi > 0.0f) rssi = 0.0f;
    if(rssi < -127.0f) rssi = -127.0f;

    furi_hal_subghz_idle();
    SPI_RELEASE();

    return rssi;
}
