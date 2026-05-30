# GPIO Sensor Bridge

Milestone M1 — I2C scanner + BME280 sensor ID reader for Flipper Zero.

## Features

- **I2C Scan**: Enumerates all devices on the external I2C bus (addresses 0x03–0x77)
- **BME280 Read**: Detects BME280 at 0x76/0x77, reads chip ID (expected: 0x60)
- **About**: Version and credits

## Deploy

1. Copy `gpio_sensor_bridge.fap` to Flipper SD card: `/apps/GPIO/`
2. On Flipper: Apps → GPIO → GPIO Sensor Bridge

## Build

```bash
cd apps/gpio-sensor-bridge && ufbt build
```

## Hardware

- Flipper Zero external I2C bus (pins 15/SCL, 16/SDA)
- BME280 breakout (optional) — 3.3V, GND, SDA, SCL
