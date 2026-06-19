#ifndef APP_COMMANDS_H__
#define APP_COMMANDS_H__

#include <stdint.h>
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_commands_send_fn_t)(uint8_t *p_data, uint16_t len);

ret_code_t app_commands_handle(uint8_t const *p_data,
                               uint16_t length,
                               app_commands_send_fn_t send_fn);

#ifdef __cplusplus
}
#endif

#endif
