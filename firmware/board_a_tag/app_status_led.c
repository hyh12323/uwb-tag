#include "app_status_led.h"

#include "app_timer.h"
#include "custom_board.h"
#include "nrf_gpio.h"

APP_TIMER_DEF(m_status_led_timer);

static bool m_led1_on = false;

static void status_led_timeout_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);

    m_led1_on = !m_led1_on;
    nrf_gpio_pin_write(LED_1, m_led1_on ? LEDS_ACTIVE_STATE
                                        : (1 - LEDS_ACTIVE_STATE));
}

void app_status_led_gpio_init(void)
{
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_2);
    nrf_gpio_pin_write(LED_1, 1 - LEDS_ACTIVE_STATE);
    nrf_gpio_pin_write(LED_2, 1 - LEDS_ACTIVE_STATE);
}

ret_code_t app_status_led_timer_init(void)
{
    return app_timer_create(&m_status_led_timer,
                            APP_TIMER_MODE_REPEATED,
                            status_led_timeout_handler);
}

ret_code_t app_status_led_heartbeat_start(uint32_t interval_ticks)
{
    return app_timer_start(m_status_led_timer, interval_ticks, NULL);
}

void app_status_led_connected_set(bool connected)
{
    nrf_gpio_pin_write(LED_2, connected ? LEDS_ACTIVE_STATE
                                        : (1 - LEDS_ACTIVE_STATE));
}

void app_status_led_set_mask(uint8_t mask)
{
    nrf_gpio_pin_write(LED_1, (mask & 0x01) ? LEDS_ACTIVE_STATE
                                            : (1 - LEDS_ACTIVE_STATE));
    nrf_gpio_pin_write(LED_2, (mask & 0x02) ? LEDS_ACTIVE_STATE
                                            : (1 - LEDS_ACTIVE_STATE));
}
