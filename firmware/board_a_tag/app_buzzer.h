#ifndef APP_BUZZER_H__
#define APP_BUZZER_H__

#include <stdbool.h>
#include <stdint.h>
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_BUZZER_DEFAULT_FREQ_HZ      2000
#define APP_BUZZER_DEFAULT_DURATION_MS  3000

void app_buzzer_nfc_pins_as_gpio_init(void);
void app_buzzer_gpio_init(void);
ret_code_t app_buzzer_timer_init(void);
ret_code_t app_buzzer_start(uint16_t freq_hz, uint16_t duration_ms);
ret_code_t app_buzzer_start_default(void);
void app_buzzer_stop(void);
bool app_buzzer_is_active(void);

#ifdef __cplusplus
}
#endif

#endif
