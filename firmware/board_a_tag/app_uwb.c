#include "app_uwb.h"

#include <stdint.h>
#include "custom_board.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

void app_uwb_gpio_init(void)
{
    nrf_gpio_cfg_output(UWB_PWR_PIN);
    nrf_gpio_cfg_output(UWB_WAKEUP_PIN);
    nrf_gpio_cfg_output(UWB_RST_PIN);

    nrf_gpio_pin_write(UWB_WAKEUP_PIN, 0);
    nrf_gpio_pin_write(UWB_RST_PIN, 0);
    nrf_gpio_pin_write(UWB_PWR_PIN, 1);
}

ret_code_t app_uwb_power_on(void)
{
    nrf_gpio_cfg_output(UWB_PWR_PIN);
    nrf_gpio_cfg_output(UWB_WAKEUP_PIN);
    nrf_gpio_cfg_output(UWB_RST_PIN);

    nrf_gpio_pin_write(UWB_PWR_PIN, 0);
    nrf_gpio_pin_write(UWB_WAKEUP_PIN, 1);

    nrf_gpio_pin_write(UWB_RST_PIN, 0);
    nrf_delay_us(100);
    nrf_gpio_pin_write(UWB_RST_PIN, 1);

    nrf_delay_ms(50);

    NRF_LOG_INFO("UWB power ON: PWR=%d WAKEUP=%d RST=%d",
                 nrf_gpio_pin_read(UWB_PWR_PIN),
                 nrf_gpio_pin_read(UWB_WAKEUP_PIN),
                 nrf_gpio_pin_read(UWB_RST_PIN));

    return NRF_SUCCESS;
}

void app_uwb_power_off(void)
{
    nrf_gpio_pin_write(UWB_WAKEUP_PIN, 0);
    nrf_gpio_pin_write(UWB_RST_PIN, 0);
    nrf_gpio_pin_write(UWB_PWR_PIN, 1);

    NRF_LOG_INFO("UWB power OFF: PWR=%d WAKEUP=%d RST=%d",
                 nrf_gpio_pin_read(UWB_PWR_PIN),
                 nrf_gpio_pin_read(UWB_WAKEUP_PIN),
                 nrf_gpio_pin_read(UWB_RST_PIN));
}

void app_uwb_hardware_verify(void)
{
    NRF_LOG_INFO("--- UWB Hardware Verify Start ---");

    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_pin_write(LED_1, 1);
    uint32_t led_in  = nrf_gpio_pin_read(LED_1);
    uint32_t led_out = nrf_gpio_pin_out_read(LED_1);
    nrf_gpio_pin_write(LED_1, 0);
    NRF_LOG_INFO("GPIO sanity: LED_1 OUT=%d IN=%d", led_out, led_in);

    (void)app_uwb_power_on();

    NRF_LOG_INFO("GPIO: PWR(P0.27)=%d WAKEUP(P0.02)=%d RST(P0.19)=%d",
                 nrf_gpio_pin_read(UWB_PWR_PIN),
                 nrf_gpio_pin_read(UWB_WAKEUP_PIN),
                 nrf_gpio_pin_read(UWB_RST_PIN));

    nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(UWB_SPI_INSTANCE);
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin   = UWB_SPI_SCK;
    spi_config.mosi_pin  = UWB_SPI_MOSI;
    spi_config.miso_pin  = UWB_SPI_MISO;
    spi_config.ss_pin    = UWB_SPI_CS;
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;
    spi_config.mode      = NRF_DRV_SPI_MODE_0;
    spi_config.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;

    ret_code_t err_code = nrf_drv_spi_init(&spi, &spi_config, NULL, NULL);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI0 init failed: 0x%08X", err_code);
        return;
    }
    NRF_LOG_INFO("SPI0 initialized (MODE0, 4MHz)");

    uint8_t tx_buf[6] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t rx_buf[6] = {0};

    err_code = nrf_drv_spi_transfer(&spi, tx_buf, sizeof(tx_buf),
                                    rx_buf, sizeof(rx_buf));
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("SPI transfer failed: 0x%08X", err_code);
        nrf_drv_spi_uninit(&spi);
        return;
    }

    NRF_LOG_INFO("SPI rx[0..5]: %02X %02X %02X %02X %02X %02X",
                 rx_buf[0], rx_buf[1], rx_buf[2],
                 rx_buf[3], rx_buf[4], rx_buf[5]);

    uint32_t dev_id = rx_buf[2] | ((uint32_t)rx_buf[3] << 8)
                                | ((uint32_t)rx_buf[4] << 16)
                                | ((uint32_t)rx_buf[5] << 24);

    if (dev_id == 0xDECA0302 || dev_id == 0xDECA0312)
    {
        NRF_LOG_INFO("DWM3000 detected! DEV_ID: 0x%08X", dev_id);
    }
    else
    {
        NRF_LOG_ERROR("DWM3000 DEV_ID mismatch! Expected 0xDECA0302/0xDECA0312, got 0x%08X", dev_id);
    }

    nrf_drv_spi_uninit(&spi);
    NRF_LOG_INFO("--- UWB Hardware Verify End ---");
    NRF_LOG_FLUSH();
}
