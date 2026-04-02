# ESP32-S3 Microphone Test Example

## Overview

This example demonstrates microphone capture, AFE processing, and playback on the ONX3248G035 development board (ESP32-S3). The LVGL UI provides MIC (record), release-to-play, and STOP controls.

## Purchase Links

- Product: [Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)
- Recommended Microphone Accessory: [Nextion Dual MIC Board](https://itead.cc/product/nextion-dual-mic-board/)
- Recommended Audio Accessory: [Nextion BOX Speaker](https://itead.cc/product/nextion-box-speaker/)

## Features

- **Microphone capture**: PDM mic input.
- **AFE processing**: NS/VAD/AGC/SE (depending on model availability).
- **Playback**: Record to RAM and loop playback.
- **Touch UI**: LVGL UI for record/play/stop.

## Hardware Requirements

- **MCU**: ESP32-S3 Development Board
- **LCD**: 3.5-inch ST7796U SPI TFT LCD (320x480)
- **Touch**: CST826 I2C capacitive touch controller
- **IO Expander**: PCF8574 I2C IO expander
- **Microphone**: External  PDM mic
- **Speaker**: External  speaker / amplifier
- **PSRAM**: Recommended (recording buffer)

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

> Mic and speaker pins are configured in `board_mic` / `board_speaker` components.

## How to Use

1. **Connect hardware**: ensure mic/speaker/LCD are ready.
2. **Build and flash**.
3. **Record**: long-press the MIC button.
4. **Playback**: release MIC to loop playback.
5. **Stop**: tap STOP to stop recording/playback.

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

- This example depends on AFE models (`esp_afe_sr_models`) stored in the `model` partition.
- Insufficient PSRAM may prevent recording buffer allocation or AFE init.

