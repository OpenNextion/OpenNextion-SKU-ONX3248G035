# ESP32-S3 Arduino 电池电量测试示例

## 概述

本示例是 `08_battery_test` 的 ESP-Arduino 版本，用于测试 ONX3248G035 的电池电压采样、电量估算和充电状态显示。

使用的库：

- LCD：`GFX Library for Arduino`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- UI：`lvgl`
- ADC：ESP32 Arduino core ADC API

## 功能

- 使用 GPIO4 读取电池分压后的 ADC 电压。
- 按 ESP-IDF 例程中的分压电阻参数换算电池端电压。
- 通过 PCF8574 的 `CHG_N` 引脚读取充电状态。
- 使用 LVGL 显示电池图标、百分比、电压和充电状态。

## 电量显示策略

ESP-IDF 版本使用 `espressif/adc_battery_estimation` 组件和 OCV-SOC 模型。Arduino IDE 不能直接使用 ESP-IDF Component Manager，因此本例程使用轻量级 Arduino 实现：

- ADC 每次读取 `16` 次并取平均。
- 电池电压使用分压公式换算：`Vbat = Vadc * (Rupper + Rlower) / Rlower`。
- 电压经过低通滤波：`filtered = filtered * 7/8 + new * 1/8`。
- 原始电量按 `3300 mV -> 0%`、`4200 mV -> 100%` 线性估算。
- 显示百分比使用 `2%` 迟滞，避免 97% / 98% 边界抖动。
- 插拔电源时百分比先保持不变，之后最多每 `30 s` 变化 `1%`，避免充电端电压瞬态导致电量猛涨猛跌。

电压行显示的是滤波后的端电压；百分比显示的是稳定后的估算容量。

## Arduino IDE 设置

使用 Arduino IDE 打开本例程时，开发板建议选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## 文件

- `08_battery_test.ino`: Arduino 主程序。
- `BoardConfig.h`: LCD、触摸、PCF8574、ADC 和分压电阻配置。
- `build_opt.h`: LVGL 编译选项，当前仅跳过默认 `lv_conf.h`。
