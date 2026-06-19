#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// ============================================================
// 自定义 BLE+UWB 双模定位板 引脚定义
// MCU: nRF52832  SoftDevice: S132
// ============================================================

// ---- LEDs (2个) ----
#define LEDS_NUMBER         2
#define LED_1               29
#define LED_2               30
#define LED_START           LED_1
#define LED_STOP            LED_2
#define LEDS_ACTIVE_STATE   1
#define LEDS_LIST           { LED_1, LED_2 }
#define BSP_LED_0           LED_1
#define BSP_LED_1           LED_2
#define LEDS_INV_MASK       LEDS_MASK

// ---- Button (1个) ----
#define BUTTONS_NUMBER      1
#define BUTTON_1            31
#define BUTTON_START        BUTTON_1
#define BUTTON_STOP         BUTTON_1
#define BUTTON_PULL         NRF_GPIO_PIN_PULLUP
#define BUTTONS_ACTIVE_STATE 0
#define BUTTONS_LIST        { BUTTON_1 }
#define BSP_BUTTON_0        BUTTON_1

// ---- Buzzer 蜂鸣器 (PAM8904EGPR) ----
#define BUZZER_EN1_PIN      10   // EN1，拉高使能
#define BUZZER_DIN_PIN      13   // 输入2kHz-4kHz方波

// ---- NFC 芯片 (NTAG I2C Plus) ----
#define NFC_TWI_SCL         20   // TWI0 SCL
#define NFC_TWI_SDA         22   // TWI0 SDA
#define NFC_FD_PIN          3    // 场检测中断 (FD)

// ---- DWM3000 UWB 模块 (SPI0) ----
#define UWB_PWR_PIN         27   // P0.27, 拉低供电
#define UWB_SPI_INSTANCE    0
#define UWB_SPI_SCK         14   // SPI0 SCK
#define UWB_SPI_MOSI        16   // SPI0 MOSI
#define UWB_SPI_MISO        15   // SPI0 MISO
#define UWB_SPI_CS          17   // SPI0 CS
#define UWB_IRQ_PIN         18   // 外部中断
#define UWB_RST_PIN         19   // DWM3000 复位，拉低复位
#define UWB_WAKEUP_PIN      2    // DWM3000 WAKEUP，拉高唤醒

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_BOARD_H
