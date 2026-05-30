#include "scene_sensor.h"
#include "../gpio_sensor_bridge.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <gui/modules/widget.h>

/* BME280 chip ID register and expected value */
#define BME280_CHIP_ID_ADDR 0xD0
#define BME280_CHIP_ID_VAL 0x60

/* BME280 I2C addresses (configurable via solder bridge) */
#define BME280_I2C_ADDR_PRIMARY 0x76
#define BME280_I2C_ADDR_SECONDARY 0x77

static bool bme280_read_chip_id(const FuriHalI2cBusHandle* i2c, uint8_t addr, uint8_t* id_out) {
    return furi_hal_i2c_read_reg_8(i2c, (addr << 1), BME280_CHIP_ID_ADDR, id_out, 5);
}

static void scene_sensor_update_widget(GpioSensorBridgeApp* app) {
    const FuriHalI2cBusHandle* i2c = &furi_hal_i2c_handle_external;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "BME280 Sensor");

    furi_hal_i2c_acquire(i2c);

    uint8_t chip_id = 0;
    bool detected_primary = bme280_read_chip_id(i2c, BME280_I2C_ADDR_PRIMARY, &chip_id);
    bool detected_secondary = false;
    uint8_t chip_id_secondary = 0;

    if(!detected_primary) {
        detected_secondary = bme280_read_chip_id(i2c, BME280_I2C_ADDR_SECONDARY, &chip_id_secondary);
    }

    furi_hal_i2c_release(i2c);

    char line_buf[32];

    if(detected_primary) {
        if(chip_id == BME280_CHIP_ID_VAL) {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X OK", BME280_I2C_ADDR_PRIMARY, chip_id);
            widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, line_buf);
        } else {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X ???", BME280_I2C_ADDR_PRIMARY, chip_id);
            widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, line_buf);
            widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Not a BME280!");
        }
    } else if(detected_secondary) {
        if(chip_id_secondary == BME280_CHIP_ID_VAL) {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X OK", BME280_I2C_ADDR_SECONDARY, chip_id_secondary);
            widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, line_buf);
        } else {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X ???", BME280_I2C_ADDR_SECONDARY, chip_id_secondary);
            widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, line_buf);
            widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Not a BME280!");
        }
    } else {
        widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, "No BME280 found");
        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Check wiring!");
    }

    widget_add_string_element(app->widget, 0, 56, AlignLeft, AlignTop, FontSecondary, "Press BACK to return");
}

void gpio_sensor_bridge_scene_sensor_on_enter(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    scene_sensor_update_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GpioSensorBridgeViewWidget);
}

bool gpio_sensor_bridge_scene_sensor_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gpio_sensor_bridge_scene_sensor_on_exit(void* context) {
    furi_assert(context);
    GpioSensorBridgeApp* app = context;
    widget_reset(app->widget);
}