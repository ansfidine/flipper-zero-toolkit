#include "scene_about.h"
#include "../subghz_pentest.h"

#include <furi.h>
#include <gui/modules/widget.h>

void subghz_pentest_scene_about_on_enter(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "SubGHz Pentest");
    widget_add_string_element(app->widget, 0, 13, AlignLeft, AlignTop, FontSecondary, "v0.4.0 — China Ed");
    widget_add_string_element(app->widget, 0, 23, AlignLeft, AlignTop, FontSecondary, "Worker thread + SPI lock");
    widget_add_string_element(app->widget, 0, 33, AlignLeft, AlignTop, FontSecondary, "Exported APIs only (FAP)");
    widget_add_string_element(app->widget, 0, 43, AlignLeft, AlignTop, FontSecondary, "by Ansfidine Youssouf");
    widget_add_string_element(app->widget, 0, 55, AlignLeft, AlignTop, FontSecondary, "Official firmware pattern");

    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPentestViewWidget);
}

bool subghz_pentest_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void subghz_pentest_scene_about_on_exit(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;
    widget_reset(app->widget);
}