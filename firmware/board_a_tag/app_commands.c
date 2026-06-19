#include "app_commands.h"

#include <stddef.h>
#include "app_buzzer.h"
#include "app_status_led.h"
#include "app_uwb.h"
#include "nrf_log.h"

#define CMD_PING            0x01
#define CMD_UWB_START       0x10
#define CMD_UWB_STOP        0x11
#define CMD_BUZZER_ON       0x20
#define CMD_BUZZER_OFF      0x21
#define CMD_LED_SET         0x30

#define RSP_PONG            0x81
#define RSP_UWB_STARTED     0x90
#define RSP_UWB_STOPPED     0x91
#define RSP_BUZZER_ON       0xA0
#define RSP_BUZZER_OFF      0xA1
#define RSP_LED_OK          0xB0
#define RSP_ERROR           0xFF

#define ERR_UNKNOWN_CMD     0x01
#define ERR_INVALID_LENGTH  0x02
#define ERR_ACTION_FAILED   0x03

static void send_u8(app_commands_send_fn_t send_fn, uint8_t value)
{
    uint8_t rsp = value;
    send_fn(&rsp, 1);
}

static void send_error(app_commands_send_fn_t send_fn, uint8_t err)
{
    uint8_t rsp[2] = {RSP_ERROR, err};
    send_fn(rsp, sizeof(rsp));
}

static void handle_ping(app_commands_send_fn_t send_fn)
{
    send_u8(send_fn, RSP_PONG);
}

static void handle_uwb_start(app_commands_send_fn_t send_fn)
{
    ret_code_t err_code = app_uwb_power_on();
    if (err_code == NRF_SUCCESS)
    {
        NRF_LOG_INFO("UWB Start command accepted");
        // TODO: start DWM3000 ranging state machine.
        send_u8(send_fn, RSP_UWB_STARTED);
    }
    else
    {
        NRF_LOG_ERROR("UWB Start failed: 0x%08X", err_code);
        send_error(send_fn, ERR_ACTION_FAILED);
    }
}

static void handle_uwb_stop(app_commands_send_fn_t send_fn)
{
    // TODO: stop DWM3000 ranging state machine before power down.
    app_uwb_power_off();
    NRF_LOG_INFO("UWB Stop command accepted");
    send_u8(send_fn, RSP_UWB_STOPPED);
}

static void handle_buzzer_on(app_commands_send_fn_t send_fn)
{
    ret_code_t err_code = app_buzzer_start_default();
    if (err_code == NRF_SUCCESS)
    {
        NRF_LOG_INFO("Buzzer ON command accepted");
        send_u8(send_fn, RSP_BUZZER_ON);
    }
    else
    {
        NRF_LOG_ERROR("Buzzer ON failed: 0x%08X", err_code);
        send_error(send_fn, ERR_ACTION_FAILED);
    }
}

static void handle_buzzer_off(app_commands_send_fn_t send_fn)
{
    app_buzzer_stop();
    NRF_LOG_INFO("Buzzer OFF");
    send_u8(send_fn, RSP_BUZZER_OFF);
}

static void handle_led_set(uint8_t led_mask, app_commands_send_fn_t send_fn)
{
    app_status_led_set_mask(led_mask);
    uint8_t rsp[2] = {RSP_LED_OK, led_mask};
    send_fn(rsp, sizeof(rsp));
}

ret_code_t app_commands_handle(uint8_t const *p_data,
                               uint16_t length,
                               app_commands_send_fn_t send_fn)
{
    if ((p_data == NULL) || (send_fn == NULL))
    {
        return NRF_ERROR_NULL;
    }

    if (length < 1)
    {
        return NRF_ERROR_INVALID_LENGTH;
    }

    uint8_t cmd = p_data[0];
    NRF_LOG_INFO("RX cmd=0x%02X len=%d", cmd, length);

    switch (cmd)
    {
        case CMD_PING:
            handle_ping(send_fn);
            break;

        case CMD_UWB_START:
            handle_uwb_start(send_fn);
            break;

        case CMD_UWB_STOP:
            handle_uwb_stop(send_fn);
            break;

        case CMD_BUZZER_ON:
            handle_buzzer_on(send_fn);
            break;

        case CMD_BUZZER_OFF:
            handle_buzzer_off(send_fn);
            break;

        case CMD_LED_SET:
            if (length < 2)
            {
                send_error(send_fn, ERR_INVALID_LENGTH);
                return NRF_ERROR_INVALID_LENGTH;
            }
            handle_led_set(p_data[1], send_fn);
            break;

        default:
            NRF_LOG_WARNING("Unknown cmd: 0x%02X", cmd);
            send_error(send_fn, ERR_UNKNOWN_CMD);
            return NRF_ERROR_NOT_SUPPORTED;
    }

    return NRF_SUCCESS;
}
