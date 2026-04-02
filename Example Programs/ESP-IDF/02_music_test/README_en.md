# ESP32-S3 Music Playback Test Example

## Overview

This example demonstrates playing audio from an SD card on the ONX3248G035 development board (ESP32-S3), with a simple LVGL touch UI and onboard speaker output.

## Purchase Links

- Product: [Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)
- Recommended Audio Accessory: [Nextion BOX Speaker](https://itead.cc/product/nextion-box-speaker/)

## Features

- **SD card music scan**: Reads music files from the SD card.
- **Audio playback**: Uses I2S + amplifier to output audio.
- **Touch UI**: LVGL-based UI to select and control playback.

## Hardware Requirements

- **MCU**: ESP32-S3 Development Board
- **LCD**: 3.5-inch ST7796U SPI TFT LCD (320x480)
- **Touch**: CST826 I2C capacitive touch controller
- **IO Expander**: PCF8574 I2C IO expander
- **Speaker**: External  speaker / amplifier
- **SD Card**: microSD card formatted as FAT32

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
| **SD Card (SDMMC 1-bit)** | | |
| SD CMD | GPIO_NUM_10 | SD Command |
| SD D0 | GPIO_NUM_9 | SD Data 0 |
| SD CLK | GPIO_NUM_11 | SD Clock |
| **Other** | | |
| LCD BL | GPIO_NUM_6 | Backlight (PWM) |
| LCD RST | - | Controlled via PCF8574 |

## How to Use

1. **Prepare SD card**: format as FAT32.
2. **Create folder**: add `music` folder in the SD root.
3. **Copy audio files**: put music files into `music`.
4. **Build and flash**.
5. **Use the touch UI** to select and play tracks.

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

- This example requires PSRAM for LVGL buffers and media loading.
- If SD card or `music` folder is missing, the UI will show a hint message.

