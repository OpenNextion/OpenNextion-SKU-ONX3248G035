# ESP32-S3 Out-of-Box Demo

## Overview

This example is an out-of-box demo for the ONX3248G035 development board (ESP32-S3). It runs the LVGL Widgets demo to quickly verify LCD, touch, and UI rendering.

## Purchase Links

- Product: [Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## Features

- **LVGL widgets demo**: showcases multiple UI widgets and interactions.
- **Touch interaction**: supports touch and scrolling.
- **LCD display**: ST7796U LCD output.

## Hardware Requirements

- **MCU**: ESP32-S3 Development Board
- **LCD**: 3.5-inch ST7796U SPI TFT LCD (320x480)
- **Touch**: CST826 I2C capacitive touch controller
- **IO Expander**: PCF8574 I2C IO expander

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
| **Other** | | |
| LCD BL | GPIO_NUM_6 | Backlight (PWM) |
| LCD RST | - | Controlled via PCF8574 |

## How to Use

1. **Connect hardware**: ensure LCD and touch are wired correctly.
2. **Build and flash**.
3. **View demo**: the widgets demo starts automatically after boot.

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

