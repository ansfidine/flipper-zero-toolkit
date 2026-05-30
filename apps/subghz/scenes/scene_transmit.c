#include "scene_transmit.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/widget.h>
#include <gui/modules/widget_elements/widget_element.h>

#define TAG "SubGhzPentestTransmit"

/* ── Transmit scene: replay a captured signal ──────────────────────────── */

static void transmit_update_widget(SubGhzPentestApp* app, bool transmitting) {
    widget_reset(app->widget);

    char buf[128];
    SubGhzPentestCapture* cap = &app->capture_mgr->captures[app->selected_capture];

    if(transmitting) {
        widget_add_string_element(
            app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "TRANSMITTING");
        snprintf(buf, sizeof(buf), "Protocol: %s", cap->protocol_name);
        widget_add_string_element(
            app->widget, 64, 28, AlignCenter, AlignTop, FontSecondary, buf);
        snprintf(buf, sizeof(buf), "%lu.%03lu MHz",
            cap->frequency / 1000000,
            (cap->frequency % 1000000) / 1000);
        widget_add_string_element(
            app->widget, 64, 42, AlignCenter, AlignTop, FontSecondary, buf);
    } else {
        widget_add_string_element(
            app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "Replay Signal");

        subghz_pentest_capture_get_info(app->capture_mgr, app->selected_capture, buf, sizeof(buf));

        /* Display info line by line */
        FuriString* info_str = furi_string_alloc_set(buf);
        size_t pos = 0;
        int line = 0;
        while(line < 5) {
            size_t newline = furi_string_search_char(info_str, '\n', pos);
            if(newline == FURI_STRING_FAILURE) {
                const char* remaining = furi_string_get_cstr(info_str) + pos;
                widget_add_string_element(
                    app->widget, 64, 24 + line * 12, AlignCenter, AlignTop, FontSecondary, remaining);
                break;
            }
            char line_buf[64];
            size_t len = newline - pos;
            if(len >= sizeof(line_buf)) len = sizeof(line_buf) - 1;
            memcpy(line_buf, furi_string_get_cstr(info_str) + pos, len);
            line_buf[len] = '\0';
            widget_add_string_element(
                app->widget, 64, 24 + line * 12, AlignCenter, AlignTop, FontSecondary, line_buf);
            pos = newline + 1;
            line++;
        }
        furi_string_free(info_str);

        widget_add_string_element(
            app->widget, 64, 54, AlignCenter, AlignTop, FontSecondary, "OK=Send  Back=Cancel");
    }
}

void subghz_pentest_scene_transmit_on_enter(void* context) {
    SubGhzPentestApp* app = context;

    /* Make sure signal is saved before replay */
    if(!app->capture_mgr->captures[app->selected_capture].is_saved) {
        subghz_pentest_capture_save(app->capture_mgr, app->selected_capture);
    }

    transmit_update_widget(app, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPentestViewWidget);
}

bool subghz_pentest_scene_transmit_on_event(void* context, SceneManagerEvent event) {
    SubGhzPentestApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzPentestCustomEventReplaySignal:
            /* Transmit the selected signal */
            transmit_update_widget(app, true);

            bool success = subghz_pentest_capture_replay(app->capture_mgr, app->selected_capture);

            transmit_update_widget(app, false);
            if(!success) {
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "TX Failed!");
                widget_add_string_element(
                    app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Region locked or error");
            }
            return true;

        default:
            break;
        }
    }

    return false;
}

void subghz_pentest_scene_transmit_on_exit(void* context) {
    SubGhzPentestApp* app = context;
    widget_reset(app->widget);
}