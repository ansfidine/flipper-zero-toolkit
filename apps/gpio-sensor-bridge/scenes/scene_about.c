#include "scene_about.h"
#include "../gpio_sensor_bridge.h"

#include <furi.h>
#include <gui/modules/widget.h>

void gpio_sensor_bridge_scene_about_on_enter(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "GPIO Sensor Bridge");
    widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, "v0.1");
    widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "I2C sensor interface");
    widget_add_string_element(app->widget, 0, 34, AlignLeft, AlignTop, FontSecondary, "for Flipper Zero");
    widget_add_string_element(app->widget, 0, 56, AlignLeft, AlignTop, FontSecondary, "Press BACK to return");

    view_dispatcher_switch_to_view(app->view_dispatcher, GpioSensorBridgeViewWidget);
}

bool gpio_sensor_bridge_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gpio_sensor_bridge_scene_about_on_exit(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    widget_reset(app->widget);
}