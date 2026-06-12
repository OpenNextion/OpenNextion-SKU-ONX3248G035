# ESP32-S3 Arduino SD 卡图片测试示例

## 概述

本示例是 `03_sd_card_image_test` 的 ESP-Arduino 版本，用于测试 SD 卡图片文件扫描和全屏图片浏览。

使用的库：

- LCD：`GFX Library for Arduino`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- SD 卡：ESP32 Arduino core 的 `SD_MMC`
- UI：`lvgl`
- JPG/JPEG 解码：ESP32 Arduino core 内置的 Espressif JPEG decoder
- PNG 解码：LVGL 内置 `lodepng`

程序会扫描 SD 卡 `/images` 目录中的图片文件。找到图片后会进入全屏图片浏览界面，左右滑动切换上一张/下一张。

## 说明

当前版本不自写图片解码器，JPG/JPEG 使用 ESP32 Arduino core 提供的 `esp_jpeg_decode()`，PNG 使用 LVGL 自带的 `lodepng`。图片会先完整解码到 PSRAM 帧缓冲，再一次性分段刷新到 LCD，避免屏幕出现明显的从上到下解码刷新过程。

图片显示策略为固定宽度适配：无论图片原始方向如何，都会按屏幕宽度缩放到 320 像素宽，并在垂直方向居中显示。其他扩展名会参与扫描以兼容 ESP-IDF 版本的目录规则，但 Arduino 版本当前只直接渲染 JPG/JPEG/PNG；遇到暂未支持的格式时屏幕会显示解码失败提示。

`build_opt.h` 中启用了 `LV_USE_PNG` 和 `LV_MEM_CUSTOM`，用于让 LVGL 编译 PNG 支持，并让 PNG 解码输出使用可用的外部内存配置。

## SD 卡目录

请在 SD 卡根目录创建：

```text
/images
```

并放入图片文件。

推荐优先放入：

```text
.jpg
.jpeg
.png
```

## Arduino IDE 设置

使用 Arduino IDE 打开本例程时，开发板建议选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`
