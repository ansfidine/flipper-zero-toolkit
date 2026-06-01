#include "scene_signal_detail.h"
#include "../subghz_pentest.h"

#include <furi.h>
#include <gui/modules/widget.h>

/* Signal detail — M1: show best signal from last scan
 * M2: parse .sub file, show protocol + data + replay option
 * v0.4.0: Uses float RSSI for precision
 */

void subghz_pentest_scene_signal_detail_on_enter(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Signal Detail");

    if(app->best_freq > 0) {
        char line_buf[64];

        snprintf(
            line_buf,
            sizeof(line_buf),
            "Freq: %lu.%02lu MHz",
            (uint32_t)(app->best_freq / 1000000),
            (uint32_t)((app->best_freq % 1000000) / 10000));
        widget_add_string_element(app->widget, 0, 13, AlignLeft, AlignTop, FontSecondary, line_buf);

        snprintf(line_buf, sizeof(line_buf), "RSSI: %.1f dBm", (double)app->best_rssi);
        widget_add_string_element(app->widget, 0, 23, AlignLeft, AlignTop, FontSecondary, line_buf);

        const SubGhzPentestSweepConfig* cfg = &app->sweep_configs[app->active_category];
        widget_add_string_element(
            app->widget, 0, 33, AlignLeft, AlignTop, FontSecondary, cfg->protocols);

        widget_add_string_element(
            app->widget, 0, 43, AlignLeft, AlignTop, FontSecondary, "OK=Emulate  BACK=Back");
    } else {
        widget_add_string_element(
            app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, "No signal data available");
        widget_add_string_element(
            app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Run a scan first");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzPentestViewWidget);
}

bool subghz_pentest_scene_signal_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    /* M2: handle emulate event */
    return false;
}

void subghz_pentest_scene_signal_detail_on_exit(void* context) {
    furi_assert(context);
    SubGhzPentestApp* app = context;
    widget_reset(app->widget);
}