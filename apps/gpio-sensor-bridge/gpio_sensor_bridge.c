#include "gpio_sensor_bridge.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>

/* ── Scene handlers table ──────────────────────────────────────────────── */

static const AppSceneOnEnterCallback scene_on_enter[] = {
    [GpioSensorBridgeSceneMenu] = gpio_sensor_bridge_scene_menu_on_enter,
    [GpioSensorBridgeSceneI2cScan] = gpio_sensor_bridge_scene_i2c_scan_on_enter,
    [GpioSensorBridgeSceneSensor] = gpio_sensor_bridge_scene_sensor_on_enter,
    [GpioSensorBridgeSceneAbout] = gpio_sensor_bridge_scene_about_on_enter,
};

static const AppSceneOnEventCallback scene_on_event[] = {
    [GpioSensorBridgeSceneMenu] = gpio_sensor_bridge_scene_menu_on_event,
    [GpioSensorBridgeSceneI2cScan] = gpio_sensor_bridge_scene_i2c_scan_on_event,
    [GpioSensorBridgeSceneSensor] = gpio_sensor_bridge_scene_sensor_on_event,
    [GpioSensorBridgeSceneAbout] = gpio_sensor_bridge_scene_about_on_event,
};

static const AppSceneOnExitCallback scene_on_exit[] = {
    [GpioSensorBridgeSceneMenu] = gpio_sensor_bridge_scene_menu_on_exit,
    [GpioSensorBridgeSceneI2cScan] = gpio_sensor_bridge_scene_i2c_scan_on_exit,
    [GpioSensorBridgeSceneSensor] = gpio_sensor_bridge_scene_sensor_on_exit,
    [GpioSensorBridgeSceneAbout] = gpio_sensor_bridge_scene_about_on_exit,
};

static const SceneManagerHandlers scene_manager_handlers = {
    .scene_num = GpioSensorBridgeSceneCount,
    .on_enter_handlers = scene_on_enter,
    .on_event_handlers = scene_on_event,
    .on_exit_handlers = scene_on_exit,
};

/* ── ViewDispatcher callbacks ─────────────────────────────────────────── */

static bool gpio_sensor_bridge_view_dispatcher_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool gpio_sensor_bridge_view_dispatcher_navigation_event_callback(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    return scene_manager_previous_scene(app->scene_manager);
}

/* ── Scene: Menu (embedded in main .c for M1) ──────────────────────────── */

void gpio_sensor_bridge_scene_menu_on_enter(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "GPIO Sensor Bridge");
    submenu_add_item(app->submenu, "I2C Scan", GpioSensorBridgeCustomEventMenuI2cScan, NULL, NULL);
    submenu_add_item(app->submenu, "Read BME280", GpioSensorBridgeCustomEventMenuSensor, NULL, NULL);
    submenu_add_item(app->submenu, "About", GpioSensorBridgeCustomEventMenuAbout, NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, GpioSensorBridgeViewSubmenu);
}

bool gpio_sensor_bridge_scene_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GpioSensorBridgeCustomEventMenuI2cScan:
            scene_manager_next_scene(app->scene_manager, GpioSensorBridgeSceneI2cScan);
            return true;
        case GpioSensorBridgeCustomEventMenuSensor:
            scene_manager_next_scene(app->scene_manager, GpioSensorBridgeSceneSensor);
            return true;
        case GpioSensorBridgeCustomEventMenuAbout:
            scene_manager_next_scene(app->scene_manager, GpioSensorBridgeSceneAbout);
            return true;
        default:
            break;
        }
    }
    return false;
}

void gpio_sensor_bridge_scene_menu_on_exit(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    submenu_reset(app->submenu);
}

/* ── App lifecycle ──────────────────────────────────────────────────────── */

static GpioSensorBridgeApp* gpio_sensor_bridge_app_alloc(void) {
    GpioSensorBridgeApp* app = malloc(sizeof(GpioSensorBridgeApp));

    app->scene_manager = scene_manager_alloc(&scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    app->submenu = submenu_alloc();
    app->widget = widget_alloc();

    view_dispatcher_add_view(app->view_dispatcher, GpioSensorBridgeViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, GpioSensorBridgeViewWidget, widget_get_view(app->widget));

    view_dispatcher_set_custom_event_callback(app->view_dispatcher, gpio_sensor_bridge_view_dispatcher_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, gpio_sensor_bridge_view_dispatcher_navigation_event_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    return app;
}

static void gpio_sensor_bridge_app_free(GpioSensorBridgeApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, GpioSensorBridgeViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, GpioSensorBridgeViewSubmenu);

    widget_free(app->widget);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    free(app);
}

int32_t gpio_sensor_bridge_app(void* p) {
    UNUSED(p);

    GpioSensorBridgeApp* app = gpio_sensor_bridge_app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, GpioSensorBridgeSceneMenu);

    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    gpio_sensor_bridge_app_free(app);
    return 0;
}