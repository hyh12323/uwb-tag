# CC-UWB-TAG

基于 Nordic nRF52832、DWM3000、NTAG I2C Plus 的 UWB + BLE 双模定位防丢标签固件。

当前仓库主要包含 Board A，也就是失物端/标签端。Board A 作为 BLE Peripheral，通过 Nordic UART Service 接收控制命令，并控制 UWB、NFC、蜂鸣器和状态 LED。

## 当前状态

已验证功能：

- BLE Peripheral 广播，设备名 `CC-UWB-TAG`
- Nordic UART Service 命令收发
- WebBluetooth 调试面板
- 蜂鸣器开关控制
- LED 控制
- DWM3000 电源控制和 DEV_ID 读取验证
- 外部 NTAG I2C Plus 写入 NDEF URL
- 待机低功耗优化：1 秒广播间隔，默认关闭 LED 心跳

暂未完成：

- DWM3000 双向测距状态机
- Board B 网关固件
- 正式客户端页面
- 电池电量 ADC 采样和上报

## 硬件

核心器件：

- MCU: Nordic nRF52832 QFAA
- BLE Stack: SoftDevice S132
- UWB: DWM3000
- NFC: NTAG I2C Plus
- Buzzer Driver: PAM8904
- Battery: 1S LiPo, 当前测试为 150 mAh

关键引脚见 [docs/hardware.md](docs/hardware.md)。

## 软件环境

本项目基于：

- nRF5 SDK v17.1.0
- SoftDevice S132 v7.2.0
- Segger Embedded Studio
- 目标芯片：nRF52832_xxAA

注意：仓库不包含 Nordic nRF5 SDK。请自行从 Nordic 官方下载 `nRF5_SDK_17.1.0_ddde560`。

## 目录结构

```text
.
├─ firmware/
│  └─ board_a_tag/
│     ├─ main.c
│     ├─ app_ble.c/.h
│     ├─ app_buzzer.c/.h
│     ├─ app_commands.c/.h
│     ├─ app_nfc_tag.c/.h
│     ├─ app_status_led.c/.h
│     ├─ app_uwb.c/.h
│     ├─ custom_board.h
│     └─ pca10040/s132/
├─ web/
│  └─ ble_debug.html
├─ docs/
└─ README.md
```

## 使用 Segger Embedded Studio 编译

打开工程：

```text
firmware/board_a_tag/pca10040/s132/ses/ble_app_uart_pca10040_s132.emProject
```

首次打开后，需要在 SES 工程宏里设置：

```text
SDK_ROOT=<你的 nRF5_SDK_17.1.0_ddde560 路径>
PROJ_DIR=../../..
```

工程当前已把 `SDK_ROOT` 留成占位值：

```text
SDK_ROOT=PLEASE_SET_NRF5_SDK_ROOT
```

需要改成你本机 SDK 的实际路径。

## 烧录

推荐流程：

1. 烧录 SoftDevice S132 v7.2.0。
2. 烧录应用固件。
3. 用 J-Link RTT 查看日志。
4. 用 `web/ble_debug.html` 通过 Chrome WebBluetooth 连接设备。

如果 SoftDevice 已经烧录过，并且没有执行整片 erase，后续通常只需要烧录应用固件。

## Web 调试面板

打开：

```text
web/ble_debug.html
```

要求：

- Chrome/Edge
- 支持 WebBluetooth
- 通过 HTTPS 或本地 `file://` 打开

命令协议见 [docs/protocol.md](docs/protocol.md)。

## 低功耗说明

当前固件默认：

- BLE 广播间隔：1 秒
- 待机 LED1 心跳：关闭
- UWB：默认断电，仅收到 UWB Start 命令后上电
- 蜂鸣器：默认关闭

150 mAh 电池早期测试记录见 [docs/power_test_log.md](docs/power_test_log.md)。

## 许可

本项目使用 MIT License。Nordic nRF5 SDK、SoftDevice、DWM3000 相关官方驱动/文档遵循各自厂商许可，未包含在本仓库中。
