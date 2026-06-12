# ESP32-S3 Arduino Microphone Record/Playback Example

## Overview

This is the ESP-Arduino version of `07_microphone_test`. It tests PDM microphone recording and speaker playback on the ONX3248G035 board.

The interaction matches the ESP-IDF example: press and hold `MIC` to record, release it to loop-play the audio stored in RAM, and tap `Stop` to stop recording or playback.

Libraries used:

- LCD: `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- UI: `lvgl`
- Microphone: ESP-IDF `driver/i2s_pdm.h` from the ESP32 Arduino core
- Speaker: ESP-IDF `driver/i2s_std.h` from the ESP32 Arduino core

## Runtime Flow

1. Initialize LCD, CST826 touch, PCF8574, PDM microphone input, and I2S speaker output.
2. Allocate a 30-second recording buffer from PSRAM.
3. Press and hold `MIC`: recording starts and the button turns red.
4. Release `MIC`: recording stops and loop playback starts; the button turns green.
5. Tap `Stop`: stop recording or playback and disable the speaker amplifier.

## Audio Configuration

- Sample rate: `16000 Hz`
- Bit depth: `16-bit`
- Recording format: PDM RX converted to PCM mono
- Playback format: I2S STD TX mono
- Maximum record time: `30 s`
- Record buffer: PSRAM first, internal RAM fallback

Note: the ESP-IDF version includes the ESP-SR/AFE processing chain. The Arduino version keeps the core record/playback behavior and uses the standard I2S drivers bundled with the ESP32 Arduino core instead of porting ESP-SR/AFE.

## Arduino IDE Settings

When opening this example in Arduino IDE, use:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## Files

- `07_microphone_test.ino`: Main Arduino sketch.
- `BoardConfig.h`: LCD, touch, PDM microphone, I2S speaker, and PCF8574 pin configuration.
- `build_opt.h`: LVGL build option; it currently skips the default `lv_conf.h`.
