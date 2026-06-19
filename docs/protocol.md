# BLE NUS 调试协议

Board A 使用 Nordic UART Service。

## UUID

| 项 | UUID |
|---|---|
| NUS Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| NUS RX, 写入 Board A | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| NUS TX, Board A 通知 | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

## 命令

| 命令 | 方向 | 含义 | 响应 |
|---:|---|---|---:|
| `0x01` | Host -> Board A | Ping | `0x81` |
| `0x10` | Host -> Board A | UWB Start，上电 DWM3000 | `0x90` |
| `0x11` | Host -> Board A | UWB Stop，关闭 DWM3000 | `0x91` |
| `0x20` | Host -> Board A | Buzzer ON | `0xA0` |
| `0x21` | Host -> Board A | Buzzer OFF | `0xA1` |
| `0x30 mask` | Host -> Board A | LED 控制 | `0xB0 mask` |
| `0x80` | Board A -> Host | Keepalive | 无需响应 |
| `0xFF err` | Board A -> Host | 错误响应 | 无 |

## LED mask

| mask | 行为 |
|---:|---|
| `0x00` | LED1/LED2 关闭 |
| `0x01` | LED1 开 |
| `0x02` | LED2 开 |
| `0x03` | LED1/LED2 开 |

## 错误码

| 错误码 | 含义 |
|---:|---|
| `0x01` | 未知命令 |
| `0x02` | 长度错误 |
| `0x03` | 动作执行失败 |

## 当前限制

`UWB Start` 当前只完成 DWM3000 电源上电和复位释放，还没有启动完整测距状态机。
