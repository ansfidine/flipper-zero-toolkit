#include "scene_captured.h"
#include "../subghz_pentest.h"

#include <furi.h>
#include <gui/modules/widget.h>

/* Captured signals list — M1: placeholder showing count and status
 * M2: Browse saved .sub files from /ext/subghz_pentest/
 */

void subghz_pentest_scene_captured_on_enter(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Captured Signals");

    char line_buf[32];
    snprintf(line_buf, sizeof(line_buf), "Saved: %lu signals", app->capture_count);
    widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, line_buf);

    if(app->capture_count > 0) {
        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "OK=View details");
        widget_add_string_element(app->widget, 0, 36, AlignLeft, AlignTop, FontSecondary, "Path: /ext/subghz/");
    } else {
        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "No signals yet");
        widget_add_string_element(app->widget, 0, 36, AlignLeft, AlignTop, FontSecondary, "Use a scan mode first");
    }

    widget_add_string_element(app->widget, 0, 55, AlignLeft, AlignTop, FontSecondary, "BACK=Return");
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPentestViewWidget);
}

bool subghz_pentest_scene_captured_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    /* M2: navigate to signal detail on select */
    if(event.type == SceneManagerEventTypeCustom && app->capture_count > 0) {
        scene_manager_next_scene(app->scene_manager, SubGhzPentestSceneSignalDetail);
        return true;
    }

    return false;
}

void subghz_pentest_scene_captured_on_exit(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;
    widget_reset(app->widget);
}