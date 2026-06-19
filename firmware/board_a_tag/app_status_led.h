#ifndef APP_STATUS_LED_H__
#define APP_STATUS_LED_H__

#include <stdbool.h>
#include <stdint.h>
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_status_led_gpio_init(void);
ret_code_t app_status_led_timer_init(void);
ret_code_t app_status_led_heartbeat_start(uint32_t interval_ticks);
void app_status_led_connected_set(bool connected);
void app_status_led_set_mask(uint8_t mask);

#ifdef __cplusplus
}
#endif

#endif
