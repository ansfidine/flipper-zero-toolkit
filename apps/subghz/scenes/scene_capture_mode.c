#include "scene_capture_mode.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/widget.h>
#include <gui/modules/widget_elements/widget_element.h>

#define TAG "SubGhzPentestCapture"

/* ── Capture mode: listen on a frequency and decode protocols ──────────── */

static void capture_mode_update_widget(SubGhzPentestApp* app) {
    widget_reset(app->widget);

    SubGhzPentestCaptureManager* mgr = app->capture_mgr;
    char buf[128];

    switch(mgr->state) {
    case CaptureStateIdle:
        widget_add_string_element(
            app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "Capture Mode");
        widget_add_string_element(
            app->widget, 64, 24, AlignCenter, AlignTop, FontSecondary, "Scanning...");
        snprintf(buf, sizeof(buf), "Freq: %lu.%03lu MHz",
            app->current_freq / 1000000,
            (app->current_freq % 1000000) / 1000);
        widget_add_string_element(
            app->widget, 64, 36, AlignCenter, AlignTop, FontSecondary, buf);
        break;

    case CaptureStateRecording:
        widget_add_string_element(
            app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "Capturing...");
        widget_add_string_element(
            app->widget, 64, 24, AlignCenter, AlignTop, FontSecondary, "Signal detected");
        snprintf(buf, sizeof(buf), "Freq: %lu.%03lu MHz",
            mgr->capture_freq / 1000000,
            (mgr->capture_freq % 1000000) / 1000);
        widget_add_string_element(
            app->widget, 64, 36, AlignCenter, AlignTop, FontSecondary, buf);
        snprintf(buf, sizeof(buf), "Captured: %d/%d", mgr->capture_count, SUBGHZ_PENTEST_MAX_CAPTURES);
        widget_add_string_element(
            app->widget, 64, 48, AlignCenter, AlignTop, FontSecondary, buf);
        break;

    case CaptureStateDecoded:
        widget_add_string_element(
            app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "Signal Decoded!");
        if(mgr->capture_count > 0) {
            SubGhzPentestCapture* cap = &mgr->captures[mgr->capture_count - 1];
            snprintf(buf, sizeof(buf), "Protocol: %s", cap->protocol_name);
            widget_add_string_element(
                app->widget, 64, 24, AlignCenter, AlignTop, FontSecondary, buf);
            snprintf(buf, sizeof(buf), "%u bits, Key: 0x%08lX", cap->bit_count, cap->serial_data);
            widget_add_string_element(
                app->widget, 64, 36, AlignCenter, AlignTop, FontSecondary, buf);
        }
        widget_add_string_element(
            app->widget, 64, 52, AlignCenter, AlignTop, FontSecondary, "OK=Save  Back=Stop");
        break;

    case CaptureStateError:
        widget_add_string_element(
            app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Capture Error");
        widget_add_string_element(
            app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Press Back to return");
        break;
    }
}

/* ── UI callback when new capture arrives ───────────────────────────────── */
static void capture_ui_callback(void* context, uint8_t index) {
    UNUSED(index);
    SubGhzPentestApp* app = context;
    capture_mode_update_widget(app);
}

void subghz_pentest_scene_capture_mode_on_enter(void* context) {
    SubGhzPentestApp* app = context;

    /* Start capture on current frequency */
    uint32_t freq = app->best_freq > 0 ? app->best_freq : app->current_freq;
    if(freq == 0) freq = app->sweep_configs[app->active_category].freq_default;

    app->capture_mgr->on_capture = capture_ui_callback;
    app->capture_mgr->callback_context = app;

    subghz_pentest_capture_start(app->capture_mgr, freq);
    capture_mode_update_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPentestViewWidget);
}

bool subghz_pentest_scene_capture_mode_on_event(void* context, SceneManagerEvent event) {
    SubGhzPentestApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzPentestCustomEventSaveSignal:
            /* Save last captured signal */
            if(app->capture_mgr->capture_count > 0) {
                subghz_pentest_capture_save(app->capture_mgr, app->capture_mgr->capture_count - 1);
                capture_mode_update_widget(app);
            }
            return true;

        case SubGhzPentestCustomEventReplaySignal:
            /* Replay last captured signal */
            if(app->capture_mgr->capture_count > 0) {
                app->selected_capture = app->capture_mgr->capture_count - 1;
                scene_manager_next_scene(app->scene_manager, SubGhzPentestSceneTransmit);
            }
            return true;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        /* Update display on each tick */
        capture_mode_update_widget(app);
    }

    return false;
}

void subghz_pentest_scene_capture_mode_on_exit(void* context) {
    SubGhzPentestApp* app = context;
    subghz_pentest_capture_stop(app->capture_mgr);
    widget_reset(app->widget);
}