# 2026-06-14 Board A 蜂鸣器与 NFC 调试日志

## 项目背景

Board A 是基于 nRF52832 的 UWB + BLE 防丢标签终端。当前阶段主要验证外设：

- 蜂鸣器驱动：PAM8904，EN1 = P0.10，DIN = P0.13
- 外部 NFC 标签：NTAG I2C Plus，SCL = P0.20，SDA = P0.22，FD = P0.03

本日志记录 2026-06-14 调试中发现的问题、根因和后续产品化建议。

## 蜂鸣器调试记录

### 现象

网页点击蜂鸣器 ON 后，蜂鸣器不响。万用表测得：

- EN1 约 0.01V
- DIN 约 0.3V

后来加入 DC 诊断模式，让固件在点击蜂鸣器 ON 后将 P0.10 和 P0.13 都固定输出高电平 30 秒。

### 根因 1：P0.10 是 nRF52832 的 NFC2 复用脚

P0.10 在 nRF52832 上默认可能作为 NFC2 使用，不一定是普通 GPIO。EN1 接在 P0.10 时，必须释放 NFCT 复用。

当前固件加入了：

```c
nfc_pins_as_gpio_init();
```

它会将 UICR 的 NFCPINS 配置为 GPIO 模式。首次写 UICR 后会自动复位一次，这是正常现象。

修复后，EN1 能正常输出 3.3V。

### 根因 2：P0.13 到 PAM8904 DIN 串联了 100nF 电容

原理图中 P0.13 到 DIN 中间串联了 100nF 电容。这个设计对 PAM8904 的 DIN 数字输入不合适。

DC 诊断模式下：

- P0.13 端能测到 3.3V
- DIN 端只有约 0.3V

这是因为串联电容隔直流。

短接电容两端后：

- DC 模式下，镊子触碰瞬间压电片只震一下
- PWM 模式恢复后，蜂鸣器能持续响

这说明 P0.13 的 PWM 输出和 PAM8904 基本正常，串联电容是设计风险点。

### 当前结论

原型板在 PWM 模式下即使不短接电容也能响，是因为串联电容传递了 PWM 的交流边沿，DIN 端可能依赖芯片内部偏置/钳位勉强识别。

但这不是推荐设计。正式板应改为：

```text
nRF52832 P0.13 ---- 0R 或 22R~100R 小串阻 ---- PAM8904 DIN
```

不要串联 100nF 电容。

如需 EMI 预留，可在 DIN 端预留小电容到 GND，但默认不贴，且容量应远小于 100nF。

## NFC 调试记录

### 目标功能

使用外部 NTAG I2C Plus 作为 NFC 标签。开机后 MCU 通过 I2C 写入 NDEF URL，使陌生手机靠近 NFC 线圈时自动弹出链接。

当前配置示例：

```c
#define NFC_NDEF_URL "https://hyhcloud.top:2356"
```

### 已验证现象

首次成功写入后，手机可以读取并跳转 NFC URL。

固件日志曾出现：

```text
NFC tag programmed URL: https://example.com/cc-uwb-tag
```

随后手机靠近线圈可以读取旧 URL。

### 坑 1：仅写 NDEF TLV 不一定够，还需要合法 CC

手机要识别 NTAG 为标准 NDEF Type 2 Tag，需要 Capability Container。曾补写：

```text
E1 10 6D 00
```

这让手机能够按 NDEF URL 识别标签。

### 坑 2：不能粗暴读写 NTAG block 0

这是今天最关键的问题。

NTAG I2C Plus 的 block 0 很特殊，里面包含：

- UID/厂商信息
- I2C 地址配置字节
- Capability Container 所在 page

规格书指出：读取 block 0 的 byte 0 时，返回的是 UID0，也就是 `0x04`，不是真实的 I2C 地址配置值。

之前错误做法是：

```text
读 block 0 -> 修改 CC 字节 -> 将整个 block 0 写回
```

这会把 block 0 byte 0 写成 `0x04`。

而 NTAG I2C 地址配置字节和 7-bit 地址相关：

```text
0xAA >> 1 = 0x55   默认地址
0x04 >> 1 = 0x02   被误写后的地址
```

所以后续继续用 `0x55` 访问时，I2C 地址阶段全部 NACK：

```text
NFC TWIM error: 0x00000002
NFC tag program failed: 0x00000003
```

`0x00000002` 是 TWIM 的 ANACK/address NACK。

### 修复方式

当前这颗已经被误写地址的 NTAG，I2C 地址改为：

```c
#define NFC_NTAG_I2C_ADDR 0x02
```

同时禁止普通固件再自动改 block 0：

```c
#define NFC_TAG_ENSURE_CC_ON_BOOT 0
```

并在代码中加入注释，提醒不要在正式运行路径中读 block 0 后整块写回。

### NFC 产品化建议

1. 新芯片默认地址仍应是 `0x55`。
2. 对当前这块被误写过的芯片，应继续使用 `0x02`，或者按规格书用正确方式恢复地址。
3. 正式封装时建议支持地址探测：
   - 优先扫 `0x55`
   - 再扫 `0x02`
   - 必要时扫 `0x01-0x7F`
4. 不要在正常启动流程反复写 EEPROM。
5. 应增加“内容变化才写”的比较逻辑，减少 EEPROM 磨损。
6. 推荐 NFC URL 使用真实 HTTPS 链接。国内环境下不建议最终依赖 `github.io`，可用阿里云 OSS、腾讯云 COS、飞书/语雀公开页或自有域名服务。

## 当前固件中的关键保留项

蜂鸣器：

```c
#define BUZZER_DC_DIAG_MODE 0
#define BUZZER_TONE_HZ 2000
```

NFC：

```c
#define NFC_TAG_WRITE_ON_BOOT 1
#define NFC_TAG_ENSURE_CC_ON_BOOT 0
#define NFC_NTAG_I2C_ADDR 0x02
#define NFC_NDEF_URL "https://hyhcloud.top:2356"
```

BLE WebBluetooth 调试：

```c
#define RUN_CONN_PARAMS_MODULE 0
```

这是为了避免 WebBluetooth 调试阶段连接参数协商导致连接不稳定。正式 Board B 连接策略可以重新设计。

## 后续封装建议

正式产品主程序不应继续把蜂鸣器、NFC、UWB 验证逻辑全部堆在 `main.c`。

建议拆成以下模块：

```text
app_buzzer.h / app_buzzer.c
app_nfc_tag.h / app_nfc_tag.c
app_ble_proto.h / app_ble_proto.c
app_uwb.h / app_uwb.c
```

蜂鸣器模块提供：

```c
ret_code_t app_buzzer_init(void);
ret_code_t app_buzzer_start(uint16_t freq_hz, uint16_t duration_ms);
void app_buzzer_stop(void);
bool app_buzzer_is_active(void);
```

NFC 模块提供：

```c
ret_code_t app_nfc_tag_init(void);
ret_code_t app_nfc_tag_set_url(char const *url);
ret_code_t app_nfc_tag_set_text(char const *text, char const *lang);
ret_code_t app_nfc_tag_program_if_changed(void);
```

为了不再踩 block 0 的坑，NFC 模块内部应：

- 不在普通路径写 block 0
- 支持 `0x55` / `0x02` 地址探测
- 支持读回 NDEF 内容做比较
- 只在内容变化时写 EEPROM
- 明确区分“初始化空白标签”和“更新已有标签内容”

## 今日结论

蜂鸣器问题主要是 P0.10 NFC 复用和 DIN 串联电容导致的硬件/引脚问题；软件 PWM 输出本身已验证。

NFC 问题主要是 NTAG block 0 特殊内存语义导致的地址配置误写；规格书确认后已定位并修复。

今天的关键教训：

```text
不要把存储器芯片的配置区当普通 EEPROM 整块读改写。
```

