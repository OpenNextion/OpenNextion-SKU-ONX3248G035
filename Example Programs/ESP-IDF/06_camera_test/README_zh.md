# ESP32-S3 摄像头测试示例

## 概述

本项目是一个在 ONX3248G035 开发板 (ESP32-S3) 上演示摄像头功能的示例。它初始化一个 OV2640 摄像头传感器，将其配置为输出 RGB565 格式，并持续捕获帧并在 ST7796U LCD 上显示。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## 功能

- **摄像头集成**：初始化和配置 OV2640 摄像头传感器。
- **实时视频流**：从摄像头捕获帧并实时显示在 LCD 上。
- **LCD 显示**：使用 ST7796U LCD 渲染摄像头画面。
- **LVGL 集成**：利用 LVGL 库管理显示缓冲区和渲染。

## 硬件要求

- **MCU**：ESP32-S3 开发板
- **LCD 屏幕**：3.5 英寸 ST7796U SPI TFT LCD（分辨率：320x480）
- **摄像头**：OV2640 摄像头模块
- **IO 扩展器**：PCF8574 I2C IO 扩展器（用于 LCD 复位）。

## 引脚连接

| 功能          | ESP32-S3 引脚 | 描述                                      |
| :------------ | :------------ | :---------------------------------------- |
| **I2C**       |               |                                           |
| I2C SCL       | GPIO_NUM_7    | I2C 时钟线                                |
| I2C SDA       | GPIO_NUM_8    | I2C 数据线                                |
| **SPI**       |               |                                           |
| SPI SCLK      | GPIO_NUM_5    | SPI 时钟线                                |
| SPI MOSI      | GPIO_NUM_1    | SPI 主设备输出从设备输入数据线             |
| LCD DC        | GPIO_NUM_3    | LCD 数据/命令选择                         |
| LCD CS        | GPIO_NUM_2    | LCD 片选                                  |
| **摄像头**    |               |                                          |
| CAM PCLK      | GPIO_NUM_41   | 摄像头像素时钟                            |
| CAM XCLK      | GPIO_NUM_38   | 摄像头系统时钟                            |
| CAM HSYNC     | GPIO_NUM_18   | 摄像头水平同步                            |
| CAM VSYNC     | GPIO_NUM_17   | 摄像头垂直同步                            |
| CAM D0 (Y2)   | GPIO_NUM_45   | 摄像头数据线 0                            |
| CAM D1 (Y3)   | GPIO_NUM_47   | 摄像头数据线 1                            |
| CAM D2 (Y4)   | GPIO_NUM_48   | 摄像头数据线 2                            |
| CAM D3 (Y5)   | GPIO_NUM_46   | 摄像头数据线 3                            |
| CAM D4 (Y6)   | GPIO_NUM_42   | 摄像头数据线 4                            |
| CAM D5 (Y7)   | GPIO_NUM_40   | 摄像头数据线 5                            |
| CAM D6 (Y8)   | GPIO_NUM_39   | 摄像头数据线 6                            |
| CAM D7 (Y9)   | GPIO_NUM_21   | 摄像头数据线 7                            |
| **其他**      |               |                                           |
| LCD BL        | GPIO_NUM_6    | LCD 背光控制 (PWM)                        |
| LCD RST       | -             | 通过 PCF8574 控制                         |

## 如何使用

1.  **连接硬件**：根据引脚连接表将 LCD 屏幕和摄像头模块连接到 ESP32-S3 开发板。
2.  **编译和烧录**：编译项目并将其烧录到 ESP32-S3 开发板。
3.  **查看输出**：设备启动后，实时摄像头画面应显示在 LCD 屏幕上。

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

