#include "app_nfc_tag.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "custom_board.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"

// Current prototype board note:
// A previous block-0 write changed the NTAG I2C address byte from 0xAA to 0x04.
// 0x04 >> 1 gives 7-bit address 0x02. New, untouched NTAG I2C Plus chips
// normally use 0x55.
#define NFC_NTAG_I2C_ADDR               0x02
#define NFC_NTAG_USER_BLOCK_START       1
#define NFC_NTAG_BLOCK_SIZE             16
#define NFC_NTAG_WRITE_DELAY_MS         6
#define NFC_NTAG_BOOT_DELAY_MS          100
#define NFC_NTAG_I2C_RETRY_COUNT        8
#define NFC_NDEF_LANG_DEFAULT           "en"
#define NFC_NDEF_BUF_SIZE               160

static bool m_nfc_twi_log_errors = true;

static void nfc_twi_init(void)
{
    NRF_TWIM0->ENABLE = 0;

    nrf_gpio_cfg(NFC_TWI_SCL,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_cfg(NFC_TWI_SDA,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    NRF_TWIM0->PSEL.SCL  = NFC_TWI_SCL;
    NRF_TWIM0->PSEL.SDA  = NFC_TWI_SDA;
    NRF_TWIM0->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K100;
    NRF_TWIM0->ADDRESS   = NFC_NTAG_I2C_ADDR;
    NRF_TWIM0->ENABLE    = (TWIM_ENABLE_ENABLE_Enabled << TWIM_ENABLE_ENABLE_Pos);
}

static void nfc_twi_uninit(void)
{
    NRF_TWIM0->ENABLE = 0;
    NRF_TWIM0->PSEL.SCL = 0xFFFFFFFF;
    NRF_TWIM0->PSEL.SDA = 0xFFFFFFFF;
}

static void nfc_twi_stop(void)
{
    uint32_t timeout = 10000;

    NRF_TWIM0->EVENTS_STOPPED = 0;
    NRF_TWIM0->TASKS_STOP = 1;
    while ((NRF_TWIM0->EVENTS_STOPPED == 0) && (--timeout != 0))
    {
    }
}

static void nfc_i2c_bus_recover(void)
{
    nfc_twi_uninit();

    nrf_gpio_cfg_output(NFC_TWI_SCL);
    nrf_gpio_cfg(NFC_TWI_SDA,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_pin_set(NFC_TWI_SCL);
    nrf_delay_us(5);

    for (uint8_t i = 0; i < 9; i++)
    {
        nrf_gpio_pin_clear(NFC_TWI_SCL);
        nrf_delay_us(5);
        nrf_gpio_pin_set(NFC_TWI_SCL);
        nrf_delay_us(5);
    }

    nfc_twi_init();
}

static ret_code_t nfc_twi_tx(uint8_t const *p_data, uint8_t len)
{
    uint32_t timeout = 1000000;

    NRF_TWIM0->EVENTS_STOPPED = 0;
    NRF_TWIM0->EVENTS_ERROR   = 0;
    NRF_TWIM0->ERRORSRC       = NRF_TWIM0->ERRORSRC;
    NRF_TWIM0->SHORTS         = TWIM_SHORTS_LASTTX_STOP_Msk;
    NRF_TWIM0->TXD.PTR        = (uint32_t)p_data;
    NRF_TWIM0->TXD.MAXCNT     = len;
    NRF_TWIM0->TASKS_STARTTX  = 1;

    while ((NRF_TWIM0->EVENTS_STOPPED == 0) &&
           (NRF_TWIM0->EVENTS_ERROR == 0) &&
           (--timeout != 0))
    {
    }

    NRF_TWIM0->SHORTS = 0;

    if (timeout == 0)
    {
        nfc_twi_stop();
        return NRF_ERROR_TIMEOUT;
    }

    if (NRF_TWIM0->EVENTS_ERROR)
    {
        uint32_t error = NRF_TWIM0->ERRORSRC;
        NRF_TWIM0->ERRORSRC = error;
        nfc_twi_stop();
        if (m_nfc_twi_log_errors)
        {
            NRF_LOG_ERROR("NFC TWIM error: 0x%08X", error);
        }
        return NRF_ERROR_INTERNAL;
    }

    return NRF_SUCCESS;
}

static ret_code_t ntag_i2c_write_block(uint8_t block,
                                       uint8_t const data[NFC_NTAG_BLOCK_SIZE])
{
    uint8_t tx[NFC_NTAG_BLOCK_SIZE + 1];
    ret_code_t err_code = NRF_SUCCESS;

    tx[0] = block;
    memcpy(&tx[1], data, NFC_NTAG_BLOCK_SIZE);

    for (uint8_t retry = 0; retry < NFC_NTAG_I2C_RETRY_COUNT; retry++)
    {
        err_code = nfc_twi_tx(tx, sizeof(tx));
        if (err_code == NRF_SUCCESS)
        {
            break;
        }

        nfc_i2c_bus_recover();
        nrf_delay_ms(NFC_NTAG_WRITE_DELAY_MS);
    }

    nrf_delay_ms(NFC_NTAG_WRITE_DELAY_MS);

    return err_code;
}

static void nfc_i2c_diag(void)
{
    uint8_t dummy = 0x00;
    bool old_log_state = m_nfc_twi_log_errors;

    nfc_twi_uninit();
    nrf_gpio_cfg(NFC_TWI_SCL,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_cfg(NFC_TWI_SDA,
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    NRF_LOG_INFO("NFC I2C idle levels: SCL=%d SDA=%d",
                 nrf_gpio_pin_read(NFC_TWI_SCL),
                 nrf_gpio_pin_read(NFC_TWI_SDA));

    nfc_i2c_bus_recover();
    m_nfc_twi_log_errors = false;

    NRF_LOG_INFO("NFC I2C address scan 0x01-0x7F start");
    for (uint8_t addr = 0x01; addr <= 0x7F; addr++)
    {
        NRF_TWIM0->ADDRESS = addr;
        if (nfc_twi_tx(&dummy, 1) == NRF_SUCCESS)
        {
            NRF_LOG_INFO("NFC I2C device ACK at 0x%02X", addr);
        }
        nfc_i2c_bus_recover();
    }
    NRF_TWIM0->ADDRESS = NFC_NTAG_I2C_ADDR;
    m_nfc_twi_log_errors = old_log_state;
    NRF_LOG_INFO("NFC I2C address scan end");
}

static uint8_t ndef_uri_prefix_code(char const *uri, char const **p_body)
{
    static char const http_www[]  = "http://www.";
    static char const https_www[] = "https://www.";
    static char const http[]      = "http://";
    static char const https[]     = "https://";

    if (strncmp(uri, http_www, strlen(http_www)) == 0)
    {
        *p_body = uri + strlen(http_www);
        return 0x01;
    }
    if (strncmp(uri, https_www, strlen(https_www)) == 0)
    {
        *p_body = uri + strlen(https_www);
        return 0x02;
    }
    if (strncmp(uri, http, strlen(http)) == 0)
    {
        *p_body = uri + strlen(http);
        return 0x03;
    }
    if (strncmp(uri, https, strlen(https)) == 0)
    {
        *p_body = uri + strlen(https);
        return 0x04;
    }

    *p_body = uri;
    return 0x00;
}

static ret_code_t ndef_build_uri_tlv(uint8_t *p_buf, uint16_t buf_size,
                                     char const *uri, uint16_t *p_len)
{
    char const *uri_body = uri;
    uint8_t prefix_code = ndef_uri_prefix_code(uri, &uri_body);
    uint16_t body_len = (uint16_t)strlen(uri_body);
    uint16_t payload_len = (uint16_t)(1 + body_len);
    uint16_t ndef_len = (uint16_t)(4 + payload_len);
    uint16_t total_len = (uint16_t)(2 + ndef_len + 1);

    if ((payload_len > 255) || (ndef_len > 254) || (total_len > buf_size))
    {
        return NRF_ERROR_INVALID_LENGTH;
    }

    uint16_t i = 0;
    p_buf[i++] = 0x03;
    p_buf[i++] = (uint8_t)ndef_len;
    p_buf[i++] = 0xD1;
    p_buf[i++] = 0x01;
    p_buf[i++] = (uint8_t)payload_len;
    p_buf[i++] = 'U';
    p_buf[i++] = prefix_code;
    memcpy(&p_buf[i], uri_body, body_len);
    i += body_len;
    p_buf[i++] = 0xFE;

    *p_len = i;
    return NRF_SUCCESS;
}

static ret_code_t ndef_build_text_tlv(uint8_t *p_buf, uint16_t buf_size,
                                      char const *text, char const *lang,
                                      uint16_t *p_len)
{
    uint16_t text_len = (uint16_t)strlen(text);
    uint16_t lang_len = (uint16_t)strlen(lang);
    uint16_t payload_len = (uint16_t)(1 + lang_len + text_len);
    uint16_t ndef_len = (uint16_t)(4 + payload_len);
    uint16_t total_len = (uint16_t)(2 + ndef_len + 1);

    if ((lang_len > 63) || (payload_len > 255) ||
        (ndef_len > 254) || (total_len > buf_size))
    {
        return NRF_ERROR_INVALID_LENGTH;
    }

    uint16_t i = 0;
    p_buf[i++] = 0x03;
    p_buf[i++] = (uint8_t)ndef_len;
    p_buf[i++] = 0xD1;
    p_buf[i++] = 0x01;
    p_buf[i++] = (uint8_t)payload_len;
    p_buf[i++] = 'T';
    p_buf[i++] = (uint8_t)lang_len;
    memcpy(&p_buf[i], lang, lang_len);
    i += lang_len;
    memcpy(&p_buf[i], text, text_len);
    i += text_len;
    p_buf[i++] = 0xFE;

    *p_len = i;
    return NRF_SUCCESS;
}

static ret_code_t nfc_tag_write_ndef(uint8_t const *p_tlv, uint16_t len)
{
    uint8_t block[NFC_NTAG_BLOCK_SIZE];
    uint16_t offset = 0;
    uint8_t block_index = NFC_NTAG_USER_BLOCK_START;

    nrf_delay_ms(NFC_NTAG_BOOT_DELAY_MS);
    nfc_twi_init();
    nfc_i2c_bus_recover();

    while (offset < len)
    {
        uint8_t chunk = (uint8_t)((len - offset) > NFC_NTAG_BLOCK_SIZE ?
                                  NFC_NTAG_BLOCK_SIZE : (len - offset));
        memset(block, 0x00, sizeof(block));
        memcpy(block, &p_tlv[offset], chunk);

        ret_code_t err_code = ntag_i2c_write_block(block_index, block);
        if (err_code != NRF_SUCCESS)
        {
            nfc_twi_uninit();
            return err_code;
        }

        offset += chunk;
        block_index++;
    }

    nfc_twi_uninit();
    return NRF_SUCCESS;
}

ret_code_t app_nfc_tag_program_url(char const *url)
{
    uint8_t tlv[NFC_NDEF_BUF_SIZE];
    uint16_t tlv_len = 0;

    ret_code_t err_code = ndef_build_uri_tlv(tlv, sizeof(tlv), url, &tlv_len);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("NFC NDEF URL build failed: 0x%08X", err_code);
        return err_code;
    }

    err_code = nfc_tag_write_ndef(tlv, tlv_len);
    if (err_code == NRF_SUCCESS)
    {
        NRF_LOG_INFO("NFC tag programmed URL: %s", url);
    }
    else
    {
        NRF_LOG_ERROR("NFC tag program failed: 0x%08X", err_code);
        nfc_i2c_diag();
    }

    return err_code;
}

ret_code_t app_nfc_tag_program_text(char const *text, char const *lang)
{
    uint8_t tlv[NFC_NDEF_BUF_SIZE];
    uint16_t tlv_len = 0;
    char const *ndef_lang = (lang == NULL) ? NFC_NDEF_LANG_DEFAULT : lang;

    ret_code_t err_code = ndef_build_text_tlv(tlv, sizeof(tlv), text, ndef_lang, &tlv_len);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("NFC NDEF text build failed: 0x%08X", err_code);
        return err_code;
    }

    err_code = nfc_tag_write_ndef(tlv, tlv_len);
    if (err_code == NRF_SUCCESS)
    {
        NRF_LOG_INFO("NFC tag programmed text: %s", text);
    }
    else
    {
        NRF_LOG_ERROR("NFC tag program failed: 0x%08X", err_code);
        nfc_i2c_diag();
    }

    return err_code;
}

void app_nfc_tag_program_default(void)
{
    (void)app_nfc_tag_program_url(APP_NFC_TAG_DEFAULT_URL);
}
