#ifndef APP_UWB_H__
#define APP_UWB_H__

#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_uwb_gpio_init(void);
ret_code_t app_uwb_power_on(void);
void app_uwb_power_off(void);
void app_uwb_hardware_verify(void);

#ifdef __cplusplus
}
#endif

#endif
