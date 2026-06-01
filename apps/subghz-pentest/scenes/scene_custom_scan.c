#include "scene_custom_scan.h"
#include "../subghz_pentest.h"

#include <furi.h>
#include <furi_hal_subghz.h>
#include <gui/modules/widget.h>

/* Custom scan: tune to user-specified frequency and monitor RSSI.
 * v0.4.0: Uses worker thread in Fixed mode — no radio ops on UI thread.
 */

/* Preset frequency values for cycling */
static const uint32_t custom_freq_values[] = {
    315000000,
    330000000,
    390000000,
    418000000,
    433920000,
};

#define CUSTOM_FREQ_COUNT 5

static void custom_scan_update_widget(SubGhzPentestApp* app) {
    char line_buf[64];

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Custom Scan");

    snprintf(
        line_buf,
        sizeof(line_buf),
        "Freq: %lu.%02lu MHz",
        (uint32_t)(app->current_freq / 1000000),
        (uint32_t)((app->current_freq % 1000000) / 10000));
    widget_add_string_element(app->widget, 0, 13, AlignLeft, AlignTop, FontSecondary, line_buf);

    snprintf(line_buf, sizeof(line_buf), "RSSI: %.0f dBm", (double)app->current_rssi);
    widget_add_string_element(app->widget, 0, 23, AlignLeft, AlignTop, FontSecondary, line_buf);

    /* Signal strength bar */
    float strength_pct = (app->current_rssi + 127.0f) * 100.0f / 157.0f;
    if(strength_pct < 0.0f) strength_pct = 0.0f;
    if(strength_pct > 100.0f) strength_pct = 100.0f;
    snprintf(line_buf, sizeof(line_buf), "Str: %.0f%%", (double)strength_pct);
    widget_add_string_element(app->widget, 0, 33, AlignLeft, AlignTop, FontSecondary, line_buf);

    if(app->scanner_running) {
        widget_add_string_element(app->widget, 0, 43, AlignLeft, AlignTop, FontSecondary, "MONITORING");
    } else {
        widget_add_string_element(app->widget, 0, 43, AlignLeft, AlignTop, FontSecondary, "STOPPED");
    }

    widget_add_string_element(
        app->widget, 0, 55, AlignLeft, AlignTop, FontSecondary, "L/R=Freq OK=Go BACK=Stop");
}

void subghz_pentest_scene_custom_scan_on_enter(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    /* Default to 433.92 MHz for custom scan */
    app->current_freq = 433920000;

    /* Validate frequency */
    if(!furi_hal_subghz_is_frequency_valid(app->current_freq)) {
        app->current_freq = 315000000; /* fallback to 315 MHz */
    }

    /* Start worker in fixed frequency mode */
    subghz_pentest_worker_start_fixed(app->worker, app->current_freq);
    app->scanner_running = true;

    custom_scan_update_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPentestViewWidget);
}

bool subghz_pentest_scene_custom_scan_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzPentestScannerEventStart:
            /* Restart worker at current freq */
            subghz_pentest_worker_stop(app->worker);
            subghz_pentest_worker_start_fixed(app->worker, app->current_freq);
            app->scanner_running = true;
            custom_scan_update_widget(app);
            return true;
        case SubGhzPentestScannerEventStop:
            subghz_pentest_worker_stop(app->worker);
            app->scanner_running = false;
            app->current_rssi = -127.0f;
            custom_scan_update_widget(app);
            return true;
        case SubGhzPentestScannerEventNextFreq: {
            /* Stop current worker, change freq, restart */
            subghz_pentest_worker_stop(app->worker);

            /* Cycle through preset frequencies */
            uint8_t i;
            for(i = 0; i < CUSTOM_FREQ_COUNT; i++) {
                if(app->current_freq == custom_freq_values[i]) {
                    i = (i + 1) % CUSTOM_FREQ_COUNT;
                    app->current_freq = custom_freq_values[i];
                    break;
                }
            }
            if(i == CUSTOM_FREQ_COUNT) {
                app->current_freq = custom_freq_values[0];
            }
            /* Validate frequency before tuning */
            if(!furi_hal_subghz_is_frequency_valid(app->current_freq)) {
                app->current_freq = 433920000; /* fallback */
            }
            /* Restart worker at new freq */
            subghz_pentest_worker_start_fixed(app->worker, app->current_freq);
            custom_scan_update_widget(app);
            return true;
        }
        default:
            break;
        }
    }

    /* On tick: read worker state, update widget */
    if(event.type == SceneManagerEventTypeTick && app->scanner_running) {
        app->current_rssi = app->worker->fixed_rssi;
        app->current_freq = app->worker->fixed_freq;
        custom_scan_update_widget(app);
        return true;
    }

    return false;
}

void subghz_pentest_scene_custom_scan_on_exit(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;
    if(app->scanner_running) {
        subghz_pentest_worker_stop(app->worker);
        app->scanner_running = false;
    }
    widget_reset(app->widget);
}