#include "scene_i2c_scan.h"
#include "../gpio_sensor_bridge.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <gui/modules/widget.h>

/* I2C address scan range (skip reserved addresses) */
#define I2C_SCAN_ADDR_MIN 0x03
#define I2C_SCAN_ADDR_MAX 0x77

static void scene_i2c_scan_update_widget(GpioSensorBridgeApp* app) {
    const FuriHalI2cBusHandle* i2c = &furi_hal_i2c_handle_external;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "I2C Scan");

    furi_hal_i2c_acquire(i2c);

    char line_buf[32];
    int y = 12;
    uint8_t found = 0;

    for(uint8_t addr = I2C_SCAN_ADDR_MIN; addr <= I2C_SCAN_ADDR_MAX; addr++) {
        /* Probe: attempt a 1-byte read from the device address */
        if(furi_hal_i2c_is_device_ready(i2c, (addr << 1), 5)) {
            found++;
            snprintf(line_buf, sizeof(line_buf), "0x%02X", addr);
            widget_add_string_element(app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, line_buf);
            y += 10;
            /* Limit to fit on screen (128x64 display, ~6 entries visible) */
            if(y > 54) {
                widget_add_string_element(app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, "...");
                break;
            }
        }
    }

    furi_hal_i2c_release(i2c);

    if(found == 0) {
        widget_add_string_element(app->widget, 0, 20, AlignLeft, AlignTop, FontSecondary, "No devices found");
    }

    char footer[32];
    snprintf(footer, sizeof(footer), "Found: %d device(s)", found);
    widget_add_string_element(app->widget, 0, 56, AlignLeft, AlignTop, FontSecondary, footer);
}

void gpio_sensor_bridge_scene_i2c_scan_on_enter(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    scene_i2c_scan_update_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GpioSensorBridgeViewWidget);
}

bool gpio_sensor_bridge_scene_i2c_scan_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gpio_sensor_bridge_scene_i2c_scan_on_exit(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    widget_reset(app->widget);
}