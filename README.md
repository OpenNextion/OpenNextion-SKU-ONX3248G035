[**English**](./README.md) | [简体中文](./README.zh-CN.md)

# ONX3248G035

![ONX3248G035](<Product Images/ONX3248G035-1.jpg>)

## Overview

`ONX3248G035` is a 3.5-inch Open Nextion HMI development board based on `ESP32-S3R8`. It integrates a capacitive touch display, `2.4 GHz Wi-Fi`, `Bluetooth 5 LE`, `MicroSD`, camera, microphone, speaker, battery management, and multiple expansion interfaces, making it suitable for HMI panels, IoT terminals, multimedia interaction, and rapid prototyping.

This repository collects product images, mechanical drawings, schematics, PCB files, certification documents, datasheets, USB-to-serial drivers, and example projects for this model. Both `ESP-IDF` and `ESP-Arduino` examples are provided so users can choose the development workflow that best fits their project.

## Purchase Links

- Product: [Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)
- Optional Audio Accessory: [Nextion BOX Speaker](https://itead.cc/product/nextion-box-speaker/)
- Optional Microphone Accessory: [Nextion Dual MIC Board](https://itead.cc/product/nextion-dual-mic-board/)
- Optional Expansion Accessory: [Nextion IO Adapter V2](https://itead.cc/product/nextion-io-adapter-v2/)

## Hardware Specifications

The following specifications are compiled from the official Wiki and the files included in this repository:

| Item | Specification |
| :--- | :--- |
| Model | `ONX3248G035` |
| MCU | `ESP32-S3R8`, Xtensa LX7 dual-core, up to `240 MHz` |
| Flash | `16 MB` |
| PSRAM | `8 MB` |
| Display Size | `3.5"` |
| Resolution | `320 x 480` |
| Colors | `262K` |
| Brightness | `300 nit` |
| Touch Type | Capacitive touch |
| LCD Driver | `ST7796` |
| Touch Driver | `CST826` |
| Wireless | `2.4 GHz Wi-Fi` + `Bluetooth 5 LE` |
| Power Input | `DC 5V/1A` |
| Battery Support | `3.7V` Li-ion battery interface with charging management, plus `3V` RTC backup battery support |
| Main Interfaces | `USB-C`, Grove `I2C`, Grove `UART1`, `MicroSD`, camera interface, PDM microphone interface, speaker interface, 24-pin GPIO, external antenna connector |
| Operating Temperature | `-20 to 70 C` |
| Storage Temperature | `-30 to 80 C` |
| Weight | Net weight about `61 g`, gross weight about `115 g` |

## Key Features

- Built on `ESP32-S3` for connected applications with graphical user interfaces.
- Onboard `3.5"` capacitive touch display, suitable for control panels, dashboards, and interactive terminals.
- Supports `MicroSD`, camera, `PDM` microphone, and speaker expansion for multimedia and voice-interaction projects.
- `USB-C` is used for power, firmware flashing, and serial logs, and supports automatic download mode.
- Includes an onboard antenna and a reserved external antenna connector for wireless applications.
- Supports Li-ion battery power and RTC backup battery, useful for portable devices and offline-clock scenarios.

## Repository Contents

| Folder | Description |
| :--- | :--- |
| `Certifications/` | Certification and compliance documents |
| `Datasheets/` | Datasheets for key components such as `ESP32-S3`, `ST7796`, `CST826`, `PCF8574`, and `IP4054V` |
| `Drawings/` | Mechanical dimension file, `2D` drawing, and `3D` model |
| `Example Programs/` | Example projects for both `ESP-IDF` and `ESP-Arduino` |
| `LTA Announcement/` | Official announcement document |
| `Product Images/` | Product photos |
| `Schematic & Layout /` | Schematics and PCB layout files |
| `USB-to-Serial Driver/` | USB-to-serial drivers for Windows, macOS, and Linux |

## Example Projects

`Example Programs/` contains two sets of ready-to-use examples:

- `ESP-IDF/`: native ESP-IDF projects, recommended for production firmware, component-based development, and full access to ESP-IDF features.
- `ESP-Arduino/`: Arduino sketches, recommended for quick prototyping, Arduino IDE users, and developers who prefer the ESP32 Arduino core.

Most example folders include both Chinese and English documentation.

### ESP-IDF Examples

| Example Folder | Description |
| :--- | :--- |
| `01_touch_test` | Touch test for LCD and touch verification |
| `02_music_test` | Play audio from `MicroSD` with touch interaction |
| `03_sd_card_image_test` | Read and display images from `MicroSD` |
| `04_sd_card_test` | `MicroSD` mount, read/write, and card info test |
| `05_wifi_test` | Wi-Fi scan, connection, and status display |
| `06_camera_test` | Connect an `OV2640` camera and preview the image on screen |
| `07_microphone_test` | PDM microphone capture, speech front-end processing, and playback |
| `08_battery_test` | Battery voltage, battery level estimation, and charge status display |
| `09_outofbox_demo` | Factory demo for quickly verifying LCD, touch, and LVGL UI |

### ESP-Arduino Examples

`Example Programs/ESP-Arduino/` contains Arduino versions of the board examples. Each folder can be opened directly in Arduino IDE through its `.ino` file.

| Example Folder | Description |
| :--- | :--- |
| `01_touch_test` | LCD, CST826 touch, PCF8574 reset, and LVGL touch-coordinate display test |
| `02_music_test` | Play `.mp3` files from `/music` on `MicroSD` with the LVGL music UI and speaker output |
| `03_sd_card_image_test` | Scan and display `JPG`, `JPEG`, and `PNG` images from `MicroSD` |
| `04_sd_card_test` | `MicroSD` mount, write, read, and verification test using `SD_MMC` |
| `05_wifi_test` | Wi-Fi scanning and connection UI using the ESP32 Arduino `WiFi` library |
| `06_camera_test` | `OV2640` camera capture and live LCD preview |
| `07_microphone_test` | PDM microphone recording and I2S speaker playback |
| `08_battery_test` | Battery voltage sampling, capacity estimation, and charging-state display |
| `09_outofbox_demo` | Arduino port of the factory LVGL widgets demo |

## Quick Start

### 1. Install the Driver

If your computer does not recognize the serial device, install the appropriate driver from `USB-to-Serial Driver/`:

- Windows: `CH340K-USB-to-UART-Driver.zip`
- macOS: `CH341SER_MAC.ZIP`
- Linux: `CH341SER_LINUX.zip`

### 2. Connect the Board

Use a `USB-C` cable to connect the board to your computer. The `USB-C` port can be used for:

- Power supply
- Firmware flashing
- Serial log output

### 3. Choose an Example

Go to `Example Programs/ESP-IDF/` or `Example Programs/ESP-Arduino/` and select the example you need. Check the included `README_zh.md` or `README_en.md` in each example folder and prepare the required peripherals, such as a `MicroSD` card, camera, microphone, or speaker.

### 4. Set Up the Development Environment

For `ESP-IDF`, the included examples indicate:

- Development version: `ESP-IDF 5.4.1`
- Recommended version: `ESP-IDF >= 5.4.0`

For `ESP-Arduino`, install the ESP32 Arduino core and the libraries required by the selected example. The common libraries are:

- `GFX Library for Arduino`
- `Adafruit CST8XX Library`
- `Adafruit PCF8574`
- `lvgl` 8.3.x

Some examples need additional libraries or built-in ESP32 Arduino features, such as `SD_MMC`, `WiFi`, `esp_camera`, ESP-IDF I2S drivers, or `ESP32-audioI2S-master`. See the example-specific README before compiling.

Recommended Arduino IDE settings for the larger examples, such as `02_music_test` and `09_outofbox_demo`:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

### 5. Build and Flash with ESP-IDF

Example using the out-of-box demo:

```bash
cd "Example Programs/ESP-IDF/09_outofbox_demo"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

If flashing fails on the first attempt, hold the `BOOT` button and press `RESET` once to enter download mode, then try again.

### 6. Build and Upload with Arduino

Open the target `.ino` file in Arduino IDE, select an ESP32-S3 board, choose the flash and PSRAM settings, then compile and upload. The same settings can also be supplied through `arduino-cli`. Example:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi "Example Programs/ESP-Arduino/09_outofbox_demo"
arduino-cli upload --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi -p <PORT> "Example Programs/ESP-Arduino/09_outofbox_demo"
```

## Notes

- `USB_UART` and `UART0` share the same `TX/RX` signals and cannot be used at the same time.
- The 24-pin GPIO expansion shares signals with `UART1`, `MIC_PDM`, and `CAMERA`, so check pin multiplexing before use.
- `USB-C` supports automatic download mode, while `UART0` does not.
- Wi-Fi supports `2.4 GHz` only.
- The speaker interface supports up to `2W`.
- If you need an external antenna, follow the official hardware instruction to switch the RF path by desoldering the corresponding resistor.
- The board itself is not waterproof. If needed, use a custom enclosure.
- For mechanical dimensions, see `Drawings/ONX3248G035-Dimension.pdf`.
- For detailed circuit design, see the files under `Schematic & Layout /`.

## References

- Official product page: [ONX3248G035 Wiki](https://nextion.tech/wiki/onx3248g035/)
- ESP-IDF getting started: [ESP32-S3 Getting Started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)

If you are starting development on this board, it is recommended to begin with `09_outofbox_demo` or `01_touch_test` to verify the screen, touch, and basic environment first, then move on to `Wi-Fi`, `MicroSD`, camera, or audio-related features. Use `ESP-IDF` when you need the complete Espressif framework, and use `ESP-Arduino` when you want a faster Arduino-style starting point.
