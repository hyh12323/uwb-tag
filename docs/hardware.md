# 硬件说明

## Board A 定位

Board A 是失物端/标签端，放置在待找物体旁边。它通过 BLE 与 Board B 或 WebBluetooth 调试页面通信，通过 UWB 执行后续测距，并通过 NFC 提供被动标签信息。

## 主控

- MCU: Nordic nRF52832 QFAA
- SDK: nRF5 SDK v17.1.0
- SoftDevice: S132 v7.2.0

## 引脚分配

### UWB: DWM3000

| 功能 | 引脚 | 说明 |
|---|---:|---|
| 电源控制 | P0.27 | 低电平给 DWM3000 供电 |
| WAKEUP | P0.02 | 高电平唤醒 |
| RST | P0.19 | 复位控制 |
| SPI CS | P0.17 | SPI 片选 |
| SPI SCK | P0.14 | SPI 时钟 |
| SPI MOSI | P0.16 | SPI 主出从入 |
| SPI MISO | P0.15 | SPI 主入从出 |
| IRQ | P0.18 | 外部中断 |

### Buzzer: PAM8904

| 功能 | 引脚 | 说明 |
|---|---:|---|
| EN1 | P0.10 | 高电平使能。该脚默认可能是 nRF52 NFC 复用脚，固件会配置 UICR 为 GPIO。 |
| DIN | P0.13 | PWM 方波输入。当前默认 2 kHz。 |

### NFC: NTAG I2C Plus

| 功能 | 引脚 |
|---|---:|
| SCL | P0.20 |
| SDA | P0.22 |
| FD | P0.03 |

说明：当前固件使用外部 NTAG I2C Plus，不使用 nRF52 内部 NFC Tag 外设。

### LED

| 功能 | 引脚 | 说明 |
|---|---:|---|
| LED1 | P0.29 | 手动控制/调试，待机心跳默认关闭 |
| LED2 | P0.30 | BLE 连接状态 |

## 低功耗默认状态

上电后默认：

- DWM3000 断电
- 蜂鸣器关闭
- LED1/LED2 关闭
- BLE 进入 1 秒间隔广播
- NFC 标签内容写入一次后不需要 MCU 持续处理
