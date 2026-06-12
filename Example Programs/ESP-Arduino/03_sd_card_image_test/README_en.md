# ESP32-S3 Arduino SD Card Image Test Example

## Overview

This is the ESP-Arduino version of `03_sd_card_image_test`. It tests SD-card image-file scanning and full-screen image browsing.

Libraries used:

- LCD: `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- SD card: `SD_MMC` from the ESP32 Arduino core
- UI: `lvgl`
- JPG/JPEG decoding: Espressif JPEG decoder bundled with the ESP32 Arduino core
- PNG decoding: LVGL bundled `lodepng`

The sketch scans `/images` for image files. When images are found, it opens a full-screen image viewer. Swipe left/right to switch to the previous or next image.

## Notes

This version does not implement a custom image decoder. JPG/JPEG files are decoded with `esp_jpeg_decode()` from the ESP32 Arduino core, and PNG files are decoded with LVGL's bundled `lodepng`. Images are fully decoded into a PSRAM frame buffer before the LCD is updated in bands, which avoids visible top-to-bottom decode refresh on the panel.

Images use a fixed width-fit policy: regardless of the source orientation, the image is scaled to the 320-pixel screen width and vertically centered. Other extensions are scanned for compatibility with the ESP-IDF directory rules, but the current Arduino version directly renders JPG/JPEG/PNG only; unsupported formats show a decode-failed message.

`build_opt.h` enables `LV_USE_PNG` and `LV_MEM_CUSTOM` so LVGL builds PNG support and the PNG decoder can allocate from the configured external-memory path.

## SD Card Layout

Create this folder in the SD card root:

```text
/images
```

Then copy image files into it.

Recommended formats:

```text
.jpg
.jpeg
.png
```

## Arduino IDE Settings

When opening this example in Arduino IDE, use:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`
