#include "app_buzzer.h"

#include "app_timer.h"
#include "app_util_platform.h"
#include "custom_board.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_log.h"

#define BUZZER_TIMER_BASE_HZ 1000000
#define BUZZER_DC_DIAG_MODE 0
#define BUZZER_DC_DIAG_MS   30000

APP_TIMER_DEF(m_buzzer_auto_off_timer);

static bool     m_buzzer_active = false;
static uint16_t m_pwm_seq       = 0;

void app_buzzer_nfc_pins_as_gpio_init(void)
{
#if defined(NFCT_PRESENT)
    if ((NRF_UICR->NFCPINS & UICR_NFCPINS_PROTECT_Msk) ==
        (UICR_NFCPINS_PROTECT_NFC << UICR_NFCPINS_PROTECT_Pos))
    {
        NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}

        NRF_UICR->NFCPINS &= ~UICR_NFCPINS_PROTECT_Msk;

        NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}

        NVIC_SystemReset();
    }
#endif
}

void app_buzzer_gpio_init(void)
{
    nrf_gpio_cfg_output(BUZZER_EN1_PIN);
    nrf_gpio_cfg_output(BUZZER_DIN_PIN);
    nrf_gpio_pin_clear(BUZZER_DIN_PIN);
    nrf_gpio_pin_clear(BUZZER_EN1_PIN);
}

static void buzzer_auto_off_timeout_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);

    if (m_buzzer_active)
    {
        NRF_LOG_INFO("Buzzer auto off");
        app_buzzer_stop();
    }
}

ret_code_t app_buzzer_timer_init(void)
{
    return app_timer_create(&m_buzzer_auto_off_timer,
                            APP_TIMER_MODE_SINGLE_SHOT,
                            buzzer_auto_off_timeout_handler);
}

ret_code_t app_buzzer_start(uint16_t freq_hz, uint16_t duration_ms)
{
    if (freq_hz == 0)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    if (m_buzzer_active)
    {
        return NRF_SUCCESS;
    }

    uint16_t countertop = (uint16_t)(BUZZER_TIMER_BASE_HZ / freq_hz);
    if (countertop == 0)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    nrf_gpio_cfg(BUZZER_EN1_PIN,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_H0H1,
                 NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_cfg(BUZZER_DIN_PIN,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_H0H1,
                 NRF_GPIO_PIN_NOSENSE);

#if BUZZER_DC_DIAG_MODE
    NRF_PWM0->TASKS_STOP = 1;
    NRF_PWM0->ENABLE = (PWM_ENABLE_ENABLE_Disabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM0->PSEL.OUT[0] = 0xFFFFFFFF;

    nrf_gpio_pin_set(BUZZER_EN1_PIN);
    nrf_gpio_pin_set(BUZZER_DIN_PIN);
    m_buzzer_active = true;

    (void)app_timer_start(m_buzzer_auto_off_timer,
                          APP_TIMER_TICKS(BUZZER_DC_DIAG_MS),
                          NULL);

    NRF_LOG_INFO("Buzzer DC diag start: EN=P0.%d HIGH, DIN=P0.%d HIGH, hold=%d ms",
                 BUZZER_EN1_PIN, BUZZER_DIN_PIN, BUZZER_DC_DIAG_MS);
    NRF_LOG_INFO("Buzzer GPIO readback: EN OUT=%d IN=%d, DIN OUT=%d IN=%d",
                 nrf_gpio_pin_out_read(BUZZER_EN1_PIN),
                 nrf_gpio_pin_read(BUZZER_EN1_PIN),
                 nrf_gpio_pin_out_read(BUZZER_DIN_PIN),
                 nrf_gpio_pin_read(BUZZER_DIN_PIN));

    return NRF_SUCCESS;
#else
    m_pwm_seq = countertop / 2;
    nrf_gpio_pin_clear(BUZZER_DIN_PIN);

    NRF_PWM0->ENABLE = (PWM_ENABLE_ENABLE_Disabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM0->PSEL.OUT[0] = BUZZER_DIN_PIN;
    NRF_PWM0->PSEL.OUT[1] = 0xFFFFFFFF;
    NRF_PWM0->PSEL.OUT[2] = 0xFFFFFFFF;
    NRF_PWM0->PSEL.OUT[3] = 0xFFFFFFFF;
    NRF_PWM0->MODE        = (PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos);
    NRF_PWM0->PRESCALER   = (PWM_PRESCALER_PRESCALER_DIV_16 << PWM_PRESCALER_PRESCALER_Pos);
    NRF_PWM0->COUNTERTOP  = countertop;
    NRF_PWM0->DECODER     = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) |
                            (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
    NRF_PWM0->LOOP        = 1;
    NRF_PWM0->SEQ[0].PTR  = (uint32_t)&m_pwm_seq;
    NRF_PWM0->SEQ[0].CNT  = 1;
    NRF_PWM0->SEQ[0].REFRESH  = 0;
    NRF_PWM0->SEQ[0].ENDDELAY = 0;
    NRF_PWM0->SHORTS      = PWM_SHORTS_LOOPSDONE_SEQSTART0_Msk;
    NRF_PWM0->ENABLE      = (PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos);

    nrf_gpio_pin_set(BUZZER_EN1_PIN);
    NRF_PWM0->TASKS_SEQSTART[0] = 1;
    m_buzzer_active = true;

    if (duration_ms > 0)
    {
        (void)app_timer_start(m_buzzer_auto_off_timer,
                              APP_TIMER_TICKS(duration_ms),
                              NULL);
    }

    NRF_LOG_INFO("Buzzer PWM start: EN=P0.%d DIN=P0.%d freq=%dHz top=%d duty=%d",
                 BUZZER_EN1_PIN, BUZZER_DIN_PIN, freq_hz, countertop, m_pwm_seq);

    return NRF_SUCCESS;
#endif
}

ret_code_t app_buzzer_start_default(void)
{
    return app_buzzer_start(APP_BUZZER_DEFAULT_FREQ_HZ,
                            APP_BUZZER_DEFAULT_DURATION_MS);
}

void app_buzzer_stop(void)
{
    (void)app_timer_stop(m_buzzer_auto_off_timer);

    NRF_PWM0->TASKS_STOP = 1;
    NRF_PWM0->ENABLE = (PWM_ENABLE_ENABLE_Disabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM0->PSEL.OUT[0] = 0xFFFFFFFF;

    nrf_gpio_cfg_output(BUZZER_DIN_PIN);
    nrf_gpio_pin_clear(BUZZER_DIN_PIN);
    nrf_gpio_pin_clear(BUZZER_EN1_PIN);
    m_buzzer_active = false;

    NRF_LOG_INFO("Buzzer stop");
}

bool app_buzzer_is_active(void)
{
    return m_buzzer_active;
}
