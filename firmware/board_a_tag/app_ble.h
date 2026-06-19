#ifndef APP_BLE_H__
#define APP_BLE_H__

#include <stdbool.h>
#include <stdint.h>

#define APP_BLE_DEVICE_NAME "CC-UWB-TAG"

#ifdef __cplusplus
extern "C" {
#endif

void app_ble_init(void);
bool app_ble_is_connected(void);
void app_ble_send(uint8_t *p_data, uint16_t len);
void app_ble_process(void);

#ifdef __cplusplus
}
#endif

#endif
