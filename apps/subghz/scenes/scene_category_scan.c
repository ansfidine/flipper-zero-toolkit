#include "scene_category_scan.h"
#include "../subghz_pentest.h"

#include <furi.h>
#include <furi_hal_subghz.h>
#include <gui/modules/widget.h>
#include <input/input.h>

/* ── Category scan: auto-sweep range via WORKER THREAD (v0.4.0) ──────── */
/* All radio ops now happen on SubGhzPentestWorker thread, not UI thread.
 * The UI tick only reads shared state (current_freq, current_rssi) and
 * updates the widget. No blocking SPI calls on UI thread anymore.
 *
 * v0.5.0: OK button now enters Capture Mode on best frequency.
 * Button label changes based on scanner state. */

/* ── OK button callback: enter capture mode ─────────────────────────────── */
static void category_scan_ok_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    UNUSED(type);
    SubGhzPentestApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzPentestCustomEventCaptureSignal);
}

static void category_scan_update_widget(SubGhzPentestApp* app) {
    const SubGhzPentestSweepConfig* cfg = &app->sweep_configs[app->active_category];
    char line_buf[64];

    widget_reset(app->widget);

    /* Header: category name */
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, cfg->label);

    /* Current sweep frequency — read from worker (shared state) */
    snprintf(
        line_buf,
        sizeof(line_buf),
        "Scan: %lu.%02lu MHz",
        (uint32_t)(app->current_freq / 1000000),
        (uint32_t)((app->current_freq % 1000000) / 10000));
    widget_add_string_element(app->widget, 0, 13, AlignLeft, AlignTop, FontSecondary, line_buf);

    /* Current RSSI — float, display as integer for readability */
    snprintf(line_buf, sizeof(line_buf), "RSSI: %.0f dBm", (double)app->current_rssi);
    widget_add_string_element(app->widget, 0, 23, AlignLeft, AlignTop, FontSecondary, line_buf);

    /* Best signal found so far */
    if(app->best_freq > 0) {
        snprintf(
            line_buf,
            sizeof(line_buf),
            "Best: %lu.%02lu MHz %.0f dBm",
            (uint32_t)(app->best_freq / 1000000),
            (uint32_t)((app->best_freq % 1000000) / 10000),
            (double)app->best_rssi);
        widget_add_string_element(app->widget, 0, 33, AlignLeft, AlignTop, FontSecondary, line_buf);
    } else {
        widget_add_string_element(app->widget, 0, 33, AlignLeft, AlignTop, FontSecondary, "No signal yet...");
    }

    /* Hit counter */
    snprintf(line_buf, sizeof(line_buf), "Hits: %lu", app->sweep_hits);
    widget_add_string_element(app->widget, 64, 33, AlignLeft, AlignTop, FontSecondary, line_buf);

    /* Worker stage indicator */
    const char* stage_str = "IDLE";
    if(app->worker) {
        switch(app->worker->stage) {
        case SubGhzPentestWorkerStageCoarse: stage_str = "COARSE"; break;
        case SubGhzPentestWorkerStageFine:   stage_str = "FINE"; break;
        case SubGhzPentestWorkerStageFixed:  stage_str = "FIXED"; break;
        default: break;
        }
    }

    if(app->scanner_running) {
        char status[16];
        snprintf(status, sizeof(status), "SCAN:%s", stage_str);
        widget_add_string_element(app->widget, 0, 43, AlignLeft, AlignTop, FontSecondary, status);
    } else {
        widget_add_string_element(app->widget, 0, 43, AlignLeft, AlignTop, FontSecondary, "STOPPED");
    }

    /* Controls: OK button enters capture mode */
    if(app->scanner_running) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Capture", category_scan_ok_callback, app);
    } else if(app->best_freq > 0) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Capture", category_scan_ok_callback, app);
    } else {
        widget_add_string_element(
            app->widget, 0, 55, AlignLeft, AlignTop, FontSecondary, "Scanning...");
    }
}

/* Worker callback: called from worker thread to notify UI of new data.
 * This runs on the worker thread — we only set a flag, UI tick reads it. */
static void category_scan_worker_callback(void* context) {
    UNUSED(context);
    /* The UI tick reads worker state directly. No action needed here.
     * This callback exists so the worker can notify us if we add
     * furi_thread_flags_set() or similar in the future. */
}

void subghz_pentest_scene_category_scan_on_enter(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    const SubGhzPentestSweepConfig* cfg = &app->sweep_configs[app->active_category];

    /* Initialize sweep state from config */
    app->sweep_start = cfg->freq_start;
    app->sweep_end = cfg->freq_end;
    app->sweep_step = cfg->freq_step;
    app->best_freq = 0;
    app->best_rssi = -127.0f;
    app->sweep_hits = 0;

    /* Set worker callback */
    subghz_pentest_worker_set_callback(app->worker, category_scan_worker_callback, app);

    /* Start worker thread — all radio ops happen there */
    subghz_pentest_worker_start(app->worker, cfg->freq_start, cfg->freq_end, cfg->freq_step);
    app->scanner_running = true;

    category_scan_update_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPentestViewWidget);
}

bool subghz_pentest_scene_category_scan_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzPentestCustomEventCaptureSignal:
            /* If scanning, stop scanner and enter capture mode on best frequency */
            if(app->scanner_running) {
                subghz_pentest_worker_stop(app->worker);
                app->scanner_running = false;
            }
            /* Enter capture mode on best frequency or current frequency */
            if(app->best_freq > 0) {
                app->current_freq = app->best_freq;
            }
            scene_manager_next_scene(app->scene_manager, SubGhzPentestSceneCaptureMode);
            return true;
        default:
            break;
        }
    }

    /* On tick: read worker state and update widget. NO radio ops here! */
    if(event.type == SceneManagerEventTypeTick) {
        /* Read shared state from worker — lock-free, single direction */
        app->current_freq = app->worker->current_freq;
        app->current_rssi = app->worker->current_rssi;
        app->best_freq = app->worker->best_freq;
        app->best_rssi = app->worker->best_rssi;
        app->sweep_hits = app->worker->sweep_hits;

        category_scan_update_widget(app);
        return true;
    }

    return false;
}

void subghz_pentest_scene_category_scan_on_exit(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    /* Stop worker thread on scene exit */
    if(app->scanner_running) {
        subghz_pentest_worker_stop(app->worker);
        app->scanner_running = false;
    }
    widget_reset(app->widget);
}