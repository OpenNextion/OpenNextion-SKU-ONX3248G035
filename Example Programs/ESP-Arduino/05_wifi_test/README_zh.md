# ESP32-S3 Arduino WiFi 测试示例

## 概述

本示例是 `05_wifi_test` 的 ESP-Arduino 版本，用于测试 WiFi 扫描和基础连接 UI。

使用的库：

- LCD：`GFX Library for Arduino`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- WiFi：ESP32 Arduino core 的 `WiFi`
- UI：`lvgl`

点击 `Scan` 按钮后，程序会扫描附近 WiFi AP，并显示 SSID、RSSI 和加密状态。点击某个 AP 后会弹出密码输入框，输入密码并点击 `Connect` 可尝试连接。

## 说明

WiFi 扫描和连接使用 ESP32 Arduino core 的标准 `WiFi.h`，没有自写 WiFi 管理栈。

## 操作流程

1. 上电后进入 WiFi Test 页面。
2. 点击右上角 `Scan`，程序切换到 STA 模式并扫描附近 AP。
3. 列表最多显示 20 个 AP，每一项包含 SSID、RSSI 和 `open` / `secure` 状态。
4. 点击 AP 后弹出密码输入面板。
5. 点击密码输入框会显示 LVGL 内置键盘。
6. 输入密码并点击 `Connect` 后开始连接。
7. 连接成功时显示当前 SSID 和 IP 地址；连接失败或断开时显示对应状态。

## 注意事项

- 本例程只演示扫描和基础连接，不保存 WiFi 配置。
- 开放网络可直接点击 AP 后点击 `Connect`，密码框可以留空。
- 扫描前会调用 `WiFi.disconnect(true)`，用于刷新扫描结果和连接状态。

## Arduino IDE 设置

使用 Arduino IDE 打开本例程时，开发板建议选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## 文件

- `05_wifi_test.ino`: Arduino 主程序。
- `BoardConfig.h`: 板级引脚、分辨率和触摸/LCD 配置。
- `build_opt.h`: LVGL 编译选项，当前仅跳过默认 `lv_conf.h`。
