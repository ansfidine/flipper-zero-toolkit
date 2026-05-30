#pragma once

#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

/* View IDs for ViewDispatcher */
typedef enum {
    GpioSensorBridgeViewSubmenu,
    GpioSensorBridgeViewWidget,
} GpioSensorBridgeView;

/* Scene enumeration — every scene the app navigates through */
typedef enum {
    GpioSensorBridgeSceneMenu,
    GpioSensorBridgeSceneI2cScan,
    GpioSensorBridgeSceneSensor,
    GpioSensorBridgeSceneAbout,
    GpioSensorBridgeSceneCount,
} GpioSensorBridgeScene;

/** Main scene callback — custom event router */
typedef enum {
    GpioSensorBridgeCustomEventMenuI2cScan,
    GpioSensorBridgeCustomEventMenuSensor,
    GpioSensorBridgeCustomEventMenuAbout,
} GpioSensorBridgeCustomEvent;

/** Full app context — passed to every scene handler */
typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
} GpioSensorBridgeApp;

/* Menu scene — embedded in main .c for M1 */
void gpio_sensor_bridge_scene_menu_on_enter(void* context);
bool gpio_sensor_bridge_scene_menu_on_event(void* context, SceneManagerEvent event);
void gpio_sensor_bridge_scene_menu_on_exit(void* context);

/* Other scenes — headers in scenes/ */
#include "scenes/scene_i2c_scan.h"
#include "scenes/scene_sensor.h"
#include "scenes/scene_about.h"