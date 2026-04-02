# ESP32-S3 SD 卡图片测试示例

## 概述

本项目是一个基于 ONX3248G035 开发板 (ESP32-S3) 的 SD 卡图像显示测试示例。它使用 SPI 接口与 ST7796U LCD 屏幕和 SD 卡进行通信。LVGL 图形库用于渲染图像和显示导航按钮。

该示例会扫描 SD 卡根目录中 images 文件夹内扩展名为 `.jpg`、`.png`、`.bmp` 和 `.bin` 的图片文件。用户可以通过向左划和向右划来切换想要显示的图片。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## 功能

- **SD 卡集成**：以 SPI 模式挂载 SD 卡并从中读取文件。
- **图片解码**：支持解码和显示多种图片格式（JPG、PNG、BMP、BIN）。
- **LVGL UI**：提供一个简单的用户界面，带有用于浏览图片的按钮。
- **实时显示**：在 ST7796U LCD 屏幕上显示图片。
- **触摸支持**：通过向左滑动和向右滑动来导航图片。

## 硬件要求

- **MCU**：ESP32-S3 开发板
- **LCD 屏幕**：3.5 英寸 ST7796U SPI TFT LCD（分辨率：320x480）
- **触摸控制器**：CST826 I2C 电容式触摸控制器
- **SD 卡**：一张格式化为 FAT32 的 microSD 卡。
- **IO 扩展器**：PCF8574 I2C IO 扩展器（用于 LCD 复位）。

## 引脚连接

| 功能          | ESP32-S3 引脚 | 描述                                      |
| :------------ | :------------ | :---------------------------------------- |
| **I2C**       |               |                                           |
| I2C SCL       | GPIO_NUM_7    | I2C 时钟线                                |
| I2C SDA       | GPIO_NUM_8    | I2C 数据线                                |
| **SPI**       |               |                                           |
| SPI SCLK      | GPIO_NUM_5    | SPI 时钟线                                |
| SPI MOSI      | GPIO_NUM_1    | SPI 主设备输出从设备输入数据线            |
| LCD DC        | GPIO_NUM_3    | LCD 数据/命令选择                         |
| LCD CS        | GPIO_NUM_2    | LCD 片选                                  |
| **SD 卡**     |               |                                           |
| SD CMD        | GPIO_NUM_10   | SD 卡命令线                               |
| SD D0         | GPIO_NUM_9    | SD 卡数据线 0                             |
| SD CLK        | GPIO_NUM_11   | SD 卡时钟线                               |
| **其他**      |               |                                           |
| LCD BL        | GPIO_NUM_6    | LCD 背光控制 (PWM)                        |
| LCD RST       | -             | 通过 PCF8574 控制                         |

## 如何使用

1.  **准备 SD 卡**：将一张 microSD 卡格式化为 FAT32 文件系统。将一些图片文件（`.jpg`、`.png`、`.bmp`、`.bin`）复制到 SD 卡的根目录下的 images 文件夹中 。
2.  **连接硬件**：根据引脚连接表将 LCD 屏幕、触摸控制器和 SD 卡连接到 ESP32-S3 开发板。
3.  **编译和烧录**：编译项目并将其烧录到 ESP32-S3 开发板。

## 编译和烧录

### 环境设置

请确保您已安装并配置了 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html) 开发环境。

### 编译步骤

1.  **设置目标芯片**：
    ```bash
    idf.py set-target esp32s3
    ```

2.  **构建项目**：
    ```bash
    idf.py build
    ```

3.  **烧录和监视**：
    ```bash
    idf.py flash monitor
    ```