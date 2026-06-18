# HiSpark SLE RSSI Ranging with OLED

基于 HiSpark WS63 与 BS21 的 SLE RSSI 距离估算实验。

## 功能

- BS21 持续发送 SLE 广播
- WS63 扫描广播并读取 RSSI
- 对 RSSI 进行滑动平均
- 使用对数路径损耗模型估算距离
- 在 SSD1306 OLED 上实时显示 RSSI、平均值和估算距离
- 信号超时时显示 WAITING SIGNAL

## 硬件

- HiSpark WS63
- HiSpark BS21 / HH-D03
- SSD1306 128×64 OLED
- I2C：SCL GPIO15，SDA GPIO16

## 目录

- `ws63/sle_speed_client`：WS63 扫描与 RSSI 距离估算
- `ws63/helloworld_oled`：SSD1306 OLED 示例
- `bs21/sle_uart`：BS21 SLE 广播端
- `configs`：相关 CMake、Kconfig 与目标配置

## 距离模型

RSSI 距离仅为估计值，需要根据实际环境标定参考 RSSI A 和路径损耗指数 n。

## 注意

本仓库只包含修改过的示例源码和配置，不包含完整 HiSpark SDK 与工具链。
