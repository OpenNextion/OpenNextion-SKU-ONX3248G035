# ESP32-S3 Arduino Music Test Example

## Overview

This is the ESP-Arduino version of `02_music_test` for the ONX3248G035 board.

It uses standard Arduino/ESP32 libraries and third-party libraries:

- LCD: `Arduino_ST7796` from `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- SD card: `SD_MMC` from the ESP32 Arduino core
- UI: `lvgl`, with the Music Demo UI assets ported from the ESP-IDF `components/gui`
- Audio playback: `ESP32-audioI2S-master`

The sketch mounts the SD card, scans `/music` for `.mp3` files, and displays the same LVGL Music Demo style as the ESP-IDF example: cover art, spectrum, track metadata, playback controls, and the ALL TRACKS list. Tapping a track plays it through the onboard NS4168 speaker amplifier.

## Notes

This Arduino version matches the ESP-IDF UI and uses a proven third-party library for MP3 decoding and I2S playback. It does not implement a custom audio decoder.

## Audio And Pop Suppression

- I2S BCLK: GPIO14
- I2S LRCLK/WS: GPIO16
- I2S DOUT/SDATA: GPIO15
- NS4168 enable: PCF8574 `I2S_CTRL`

PCF8574 ports default high after power-up. If `I2S_CTRL` is enabled too early, the speaker may pop before I2S output is stable. This sketch keeps `I2S_CTRL=LOW` during initialization and enables the amplifier only after MP3 decoding and I2S output have started. It also disables the amplifier during pause and track switching.

## ESP32S3R8 + 16 MB Flash Settings

This sketch ports the complete LVGL Music UI images and fonts from the ESP-IDF example, so it is larger than Arduino's default `1.2 MB APP` partition. For ESP32S3R8 with external 16 MB flash, use these Arduino IDE settings:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

The project includes `sketch.yaml` as a record of the target board settings. With the current Arduino CLI, `--profile` may skip the locally installed library search path, so use the verified full FQBN:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi ESP-Arduino/02_music_test
arduino-cli upload --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi -p <your port> ESP-Arduino/02_music_test
```

## Files

- `02_music_test.ino`: Main Arduino sketch.
- `BoardConfig.h`: Board pins, resolution, I2C addresses, SD pins, and I2S pins.
- `build_opt.h`: LVGL build option.
- `sketch.yaml`: Target board setting record for ESP32S3R8 + 16 MB flash.
- `src/gui`: LVGL Music UI source and assets ported from the ESP-IDF example.
- `src/gui/gui_hal.cpp`: Arduino adapter for SD_MMC scanning, MP3 playback, volume mapping, and amplifier timing.

## SD Card Layout

Create this folder in the SD card root:

```text
/music
```

Then copy `.mp3` files into it.

## Dependencies

Install these libraries from Arduino IDE Library Manager:

- `GFX Library for Arduino`
- `Adafruit CST8XX Library`
- `Adafruit PCF8574`
- `lvgl` 8.3.x
- `ESP32-audioI2S-master`
