# ESP32-S3 Battery Test Example

## Overview

This example shows how to estimate battery level using ADC sampling and display the percentage and charging state on the LCD of the ONX3248G035 development board (ESP32-S3).

## Purchase Links

- Product: [Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## Features

- **Voltage sampling**: ADC reads battery voltage via a resistor divider.
- **Battery estimation**: maps voltage to an estimated percentage.
- **Charging status**: reads charge status pin and shows icon.
- **Touch UI**: LVGL renders battery state.

## Hardware Requirements

- **MCU**: ESP32-S3 Development Board
- **LCD**: 3.5-inch ST7796U SPI TFT LCD (320x480)
- **Touch**: CST826 I2C capacitive touch controller
- **IO Expander**: PCF8574 I2C IO expander
- **Battery divider**: resistor divider per schematic

## Pin Connections (aligned with code)

| Function | ESP32-S3 Pin | Description |
| :--- | :--- | :--- |
| **I2C** | | |
| I2C SCL | GPIO_NUM_7 | I2C Clock |
| I2C SDA | GPIO_NUM_8 | I2C Data |
| **SPI (LCD)** | | |
| SPI SCLK | GPIO_NUM_5 | SPI Clock |
| SPI MOSI | GPIO_NUM_1 | SPI Data |
| LCD DC | GPIO_NUM_3 | LCD Data/Command |
| LCD CS | GPIO_NUM_2 | LCD Chip Select |
| **Battery ADC** | | |
| ADC Channel | ADC_CHANNEL_3 (GPIO4) | Battery divider input |
| **Other** | | |
| LCD BL | GPIO_NUM_6 | Backlight (PWM) |
| LCD RST | - | Controlled via PCF8574 |

## Key Parameters

- Divider resistors (refer to schematic, values can be adjusted appropriately):
  - Upper: 1.05M (`TEST_RESISTOR_UPPER`)
  - Lower: 2.15M (`TEST_RESISTOR_LOWER`)

## How to Use

1. **Connect hardware**: ensure ADC divider is correct.
2. **Build and flash**.
3. **View UI**: battery icon/percentage in the top-right.

## Build and Flash

1. Set target:
   ```bash
   idf.py set-target esp32s3
   ```
2. Build:
   ```bash
   idf.py build
   ```
3. Flash and monitor:
   ```bash
   idf.py -p <PORT> flash monitor
   ```

## Notes

- The code assumes `CHG_N` is high when charging (read via PCF8574).

