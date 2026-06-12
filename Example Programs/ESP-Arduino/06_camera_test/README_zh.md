# ESP32-S3 Arduino 摄像头测试示例

## 概述

本示例是 `06_camera_test` 的 ESP-Arduino 版本，用于测试 ONX3248G035 的摄像头采集和 LCD 实时预览。

使用的库：

- LCD：`GFX Library for Arduino`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- UI：`lvgl`
- 摄像头：ESP32 Arduino core 自带 `esp_camera`

## 功能

- 使用 Arduino_GFX 驱动 ST7796U LCD。
- 使用 Adafruit CST8XX 读取 CST826 触摸。
- 使用 LVGL 绘制标题和状态提示。
- 使用 ESP32 Arduino core 自带的 `esp_camera` 获取 RGB565/QVGA 图像。
- 摄像头帧通过 Arduino_GFX 直接刷新到屏幕预览区域，减少每帧复制。

## 运行流程

1. 初始化 I2C、PCF8574、LCD、背光和触摸。
2. 通过 PCF8574 拉低摄像头 `PWDN`，唤醒摄像头模组。
3. 初始化 `esp_camera`，配置为 QVGA `320x240`、`RGB565` 输出。
4. 使用 LVGL 绘制 `Camera Test` 标题和状态文本。
5. 主循环中约每 80 ms 获取一帧摄像头图像，并显示在屏幕中部。

## 摄像头配置

- 帧尺寸：`FRAMESIZE_QVGA`
- 分辨率：`320x240`
- 像素格式：`PIXFORMAT_RGB565`
- XCLK：`20 MHz`
- PSRAM 可用时使用 2 个帧缓冲；否则使用 1 个 DRAM 帧缓冲。

本例程行为与 ESP-IDF 版本保持一致：上电后持续显示摄像头预览，不提供暂停/恢复按钮。

## Arduino IDE 设置

使用 Arduino IDE 打开本例程时，开发板建议选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## 文件

- `06_camera_test.ino`: Arduino 主程序。
- `BoardConfig.h`: LCD、触摸、PCF8574 和摄像头引脚配置。
- `build_opt.h`: LVGL 编译选项，当前仅跳过默认 `lv_conf.h`。
