# ESP32-S3 Arduino SD 卡读写测试示例

## 概述

本示例是 `04_sd_card_test` 的 ESP-Arduino 版本，用于测试 SD 卡挂载、写入、读回和校验。

使用的库：

- LCD：`GFX Library for Arduino`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- SD 卡：ESP32 Arduino core 的 `SD_MMC`
- UI：`lvgl`

点击屏幕上的 `SD CARD TEST` 按钮后，程序会先重新检测并挂载 SD 卡，然后向 SD 卡根目录写入 `/SD CARD TEST.txt`，再读回内容并比较。如果一致，显示 `WRITE SUCCESS`；如果未检测到 SD 卡，显示 `NO SD CARD DETECTED`；其他读写异常显示错误信息。

## 测试流程

1. 初始化 LCD、CST826 触摸、PCF8574 IO 扩展和 LVGL。
2. 点击 `SD CARD TEST` 按钮。
3. 释放上一次的 `SD_MMC` 挂载状态，并重新挂载 SD 卡。
4. 写入随机字符串到 `/SD CARD TEST.txt`。
5. 关闭文件后重新打开并读回内容。
6. 比较写入和读回内容，并在屏幕上显示测试结果。

这样处理后，插卡测试成功后再拔卡，下一次点击会重新检测 SD 卡并显示 `NO SD CARD DETECTED`。

## SD 卡说明

- 使用 ESP32 Arduino core 的 `SD_MMC`，配置为 1-bit 模式。
- SD 卡根目录会生成或覆盖 `/SD CARD TEST.txt`。
- 测试文件内容类似 `SD CARD TEST RAND NUM 12345678`，每次点击都会更新。

## Arduino IDE 设置

使用 Arduino IDE 打开本例程时，开发板建议选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## 文件

- `04_sd_card_test.ino`: Arduino 主程序。
- `BoardConfig.h`: 板级引脚、分辨率和 SD 卡引脚配置。
- `build_opt.h`: LVGL 编译选项，当前仅跳过默认 `lv_conf.h`。
