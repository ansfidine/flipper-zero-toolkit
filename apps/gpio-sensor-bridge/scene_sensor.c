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

/* Timeout for BME280 register reads (ms).
 * 50ms is safe for sensor I2C reads — BME280 can take up to ~12ms
 * for a forced measurement, and slower bus conditions add overhead. */
#define BME280_I2C_TIMEOUT_MS 50

static bool bme280_read_chip_id(const FuriHalI2cBusHandle* i2c, uint8_t addr, uint8_t* id_out) {
    return furi_hal_i2c_read_reg_8(i2c, (addr << 1), BME280_CHIP_ID_ADDR, id_out, BME280_I2C_TIMEOUT_MS);
}

static void scene_sensor_update_widget(GpioSensorBridgeApp* app) {
    const FuriHalI2cBusHandle* i2c = &furi_hal_i2c_handle_external;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "BME280 Sensor");

    furi_hal_i2c_acquire(i2c);

    /* Always check both addresses — a non-BME280 device at 0x76
     * should not prevent us from finding a BME280 at 0x77. */
    uint8_t chip_id_primary = 0;
    uint8_t chip_id_secondary = 0;
    bool found_primary = bme280_read_chip_id(i2c, BME280_I2C_ADDR_PRIMARY, &chip_id_primary);
    bool found_secondary = bme280_read_chip_id(i2c, BME280_I2C_ADDR_SECONDARY, &chip_id_secondary);

    furi_hal_i2c_release(i2c);

    char line_buf[32];
    bool bme280_found = false;
    int y = 14;

    /* Check primary address (0x76) */
    if(found_primary) {
        if(chip_id_primary == BME280_CHIP_ID_VAL) {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X OK", BME280_I2C_ADDR_PRIMARY, chip_id_primary);
            widget_add_string_element(app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, line_buf);
            y += 10;
            bme280_found = true;
        } else {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X", BME280_I2C_ADDR_PRIMARY, chip_id_primary);
            widget_add_string_element(app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, line_buf);
            y += 10;
        }
    }

    /* Check secondary address (0x77) */
    if(found_secondary) {
        if(chip_id_secondary == BME280_CHIP_ID_VAL) {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X OK", BME280_I2C_ADDR_SECONDARY, chip_id_secondary);
            widget_add_string_element(app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, line_buf);
            y += 10;
            bme280_found = true;
        } else {
            snprintf(line_buf, sizeof(line_buf), "0x%02X: ID=0x%02X", BME280_I2C_ADDR_SECONDARY, chip_id_secondary);
            widget_add_string_element(app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, line_buf);
            y += 10;
        }
    }

    /* Show appropriate message if no BME280 was found on either address */
    if(!bme280_found) {
        if(found_primary || found_secondary) {
            widget_add_string_element(app->widget, 0, y, AlignLeft, AlignTop, FontSecondary, "Not a BME280!");
        } else {
            widget_add_string_element(app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, "No BME280 found");
            widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Check wiring!");
        }
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