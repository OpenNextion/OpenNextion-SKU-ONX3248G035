# ESP32-S3 SD Card Test Example

## Overview

This project is an example of testing SD card functionality on the ONX3248G035 development board (ESP32-S3). It demonstrates how to mount an SD card, write a file to it, read the file back, and check the card's information, such as card type and capacity.

The example uses the SDMMC host driver in 1-line SD mode. The results of the operations (file write, read, and card info) are printed to the serial monitor.

## Purchase Links

- Product: [Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## Features

- **SD Card Initialization**: Mounts an SD card using the SDMMC host driver.
- **File I/O**: Demonstrates writing to and reading from a file on the SD card.
- **Card Information**: Retrieves and displays the SD card's name, type, capacity, and other details.
- **Error Handling**: Includes checks for common errors during SD card operations.

## Hardware Requirements

- **MCU**: ESP32-S3 Development Board
- **SD Card**: A microSD card, formatted with FAT32.

## Pin Connections

| Function      | ESP32-S3 Pin | Description                               |
| :------------ | :----------- | :---------------------------------------- |
| **SD Card**   |              |                                           |
| SD CMD        | GPIO_NUM_10  | SD Card Command Line                      |
| SD D0         | GPIO_NUM_9   | SD Card Data Line 0                       |
| SD CLK        | GPIO_NUM_11  | SD Card Clock Line                        |

## How to Use

1.  **Prepare the SD Card**: Format a microSD card with a FAT32 file system.
2.  **Connect Hardware**: Connect the SD card to the ESP32-S3 board according to the pin connections table.
3.  **Compile and Flash**: Compile and flash the project to the ESP32-S3 board.
4.  **Monitor Output**: Open a serial monitor to view the output of the SD card test.

## Compilation and Flashing

### Environment Setup

Please ensure you have installed and configured the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) development environment.

### Compilation Steps

1.  **Set Target Chip**:
    ```bash
    idf.py set-target esp32s3
    ```

2.  **Build the Project**:
    ```bash
    idf.py build
    ```

3.  **Flash and Monitor**:
    ```bash
    idf.py flash monitor
    ```