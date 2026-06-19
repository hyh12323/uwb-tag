#include "app_ble.h"

#include <string.h>
#include "app_buzzer.h"
#include "app_commands.h"
#include "app_error.h"
#include "app_status_led.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_hci.h"
#include "ble_nus.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_soc.h"

#define APP_BLE_CONN_CFG_TAG            1
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN
#define APP_BLE_OBSERVER_PRIO           3
#define APP_ADV_INTERVAL                1600  // 1600 * 0.625 ms = 1000 ms
#define APP_ADV_DURATION                0
#define APP_STANDBY_LED_HEARTBEAT       0
#define RUN_CONN_PARAMS_MODULE          0
#define ADV_RESTART_DELAY_MS            500
#define KEEPALIVE_INTERVAL_MS           2000
#define NUS_READY_TIMEOUT_MS            10000
#define RSP_KEEPALIVE                   0x80
#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)
#define SLAVE_LATENCY                   0
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(20000, UNIT_10_MS)
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)
#define MAX_CONN_PARAMS_UPDATE_COUNT    3
#define DEAD_BEEF                       0xDEADBEEF

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);
NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);
BLE_ADVERTISING_DEF(m_advertising);
APP_TIMER_DEF(m_adv_restart_timer);
APP_TIMER_DEF(m_keepalive_timer);
APP_TIMER_DEF(m_nus_ready_timer);

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;
static bool       m_nus_ready            = false;
static ble_uuid_t m_adv_uuids[] = {
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

static void advertising_start(void);

bool app_ble_is_connected(void)
{
    return m_conn_handle != BLE_CONN_HANDLE_INVALID;
}

void app_ble_send(uint8_t *p_data, uint16_t len)
{
    if (!app_ble_is_connected())
    {
        NRF_LOG_WARNING("app_ble_send: not connected, drop %d bytes", len);
        return;
    }

    uint32_t err = ble_nus_data_send(&m_nus, p_data, &len, m_conn_handle);
    if (err == NRF_SUCCESS)
    {
        NRF_LOG_INFO("TX[%d]: %02X %02X %02X %02X %02X",
                     len, p_data[0],
                     len > 1 ? p_data[1] : 0,
                     len > 2 ? p_data[2] : 0,
                     len > 3 ? p_data[3] : 0,
                     len > 4 ? p_data[4] : 0);
    }
    else
    {
        NRF_LOG_ERROR("app_ble_send failed: 0x%08X", err);
    }
}

void app_ble_process(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}

static void adv_restart_timeout_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);

    if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        NRF_LOG_INFO("Restarting advertising after disconnect");
        advertising_start();
    }
}

static void keepalive_timeout_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);

    if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return;
    }

    uint8_t data = RSP_KEEPALIVE;
    uint16_t len = sizeof(data);
    ret_code_t err_code = ble_nus_data_send(&m_nus, &data, &len, m_conn_handle);
    if ((err_code != NRF_SUCCESS) &&
        (err_code != NRF_ERROR_INVALID_STATE) &&
        (err_code != NRF_ERROR_RESOURCES))
    {
        NRF_LOG_WARNING("BLE keepalive failed: 0x%08X", err_code);
    }
}

static void nus_ready_timeout_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);

    if ((m_conn_handle != BLE_CONN_HANDLE_INVALID) && !m_nus_ready)
    {
        NRF_LOG_WARNING("NUS not ready after connect; disconnecting stale GATT link");
        (void)sd_ble_gap_disconnect(m_conn_handle,
                                    BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    }
}

static void nus_data_handler(ble_nus_evt_t *p_evt)
{
    if (p_evt->type == BLE_NUS_EVT_COMM_STARTED)
    {
        m_nus_ready = true;
        (void)app_timer_stop(m_nus_ready_timer);
        NRF_LOG_INFO("NUS notifications enabled; keepalive started");
        (void)app_timer_start(m_keepalive_timer,
                              APP_TIMER_TICKS(KEEPALIVE_INTERVAL_MS),
                              NULL);
        return;
    }

    if (p_evt->type == BLE_NUS_EVT_COMM_STOPPED)
    {
        m_nus_ready = false;
        NRF_LOG_INFO("NUS notifications disabled; keepalive stopped");
        (void)app_timer_stop(m_keepalive_timer);
        return;
    }

    if (p_evt->type != BLE_NUS_EVT_RX_DATA)
    {
        return;
    }

    uint8_t const *p_data = p_evt->params.rx_data.p_data;
    uint16_t length = p_evt->params.rx_data.length;

    (void)app_commands_handle(p_data, length, app_ble_send);
}

static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)
{
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        NRF_LOG_WARNING("Connection parameter update failed; keeping link alive");
    }
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("ble_advertising module entered FAST mode");
            break;

        case BLE_ADV_EVT_IDLE:
            sd_power_system_off();
            break;

        default:
            break;
    }
}

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    UNUSED_PARAMETER(p_context);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            app_status_led_connected_set(true);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            m_nus_ready = false;
            APP_ERROR_CHECK(nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle));
            NRF_LOG_INFO("BLE connected (handle=%d)", m_conn_handle);
            (void)app_timer_start(m_nus_ready_timer,
                                  APP_TIMER_TICKS(NUS_READY_TIMEOUT_MS),
                                  NULL);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            app_status_led_connected_set(false);
            (void)app_timer_stop(m_keepalive_timer);
            (void)app_timer_stop(m_nus_ready_timer);
            m_nus_ready = false;
            NRF_LOG_INFO("BLE disconnected, reason=0x%02X",
                         p_ble_evt->evt.gap_evt.params.disconnected.reason);
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            (void)app_timer_start(m_adv_restart_timer,
                                  APP_TIMER_TICKS(ADV_RESTART_DELAY_MS),
                                  NULL);
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST: {
            ble_gap_conn_params_t const *p_params =
                &p_ble_evt->evt.gap_evt.params.conn_param_update_request.conn_params;
            NRF_LOG_INFO("BLE conn param update request: min=%d max=%d latency=%d timeout=%d",
                         p_params->min_conn_interval,
                         p_params->max_conn_interval,
                         p_params->slave_latency,
                         p_params->conn_sup_timeout);
            APP_ERROR_CHECK(sd_ble_gap_conn_param_update(
                p_ble_evt->evt.gap_evt.conn_handle, p_params));
            break;
        }

        case BLE_GAP_EVT_CONN_PARAM_UPDATE: {
            ble_gap_conn_params_t const *p_params =
                &p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params;
            NRF_LOG_INFO("BLE conn params updated: min=%d max=%d latency=%d timeout=%d",
                         p_params->min_conn_interval,
                         p_params->max_conn_interval,
                         p_params->slave_latency,
                         p_params->conn_sup_timeout);
            break;
        }

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
            NRF_LOG_INFO("BLE PHY update request");
            ble_gap_phys_t const phys = {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            APP_ERROR_CHECK(sd_ble_gap_phy_update(
                p_ble_evt->evt.gap_evt.conn_handle, &phys));
            break;
        }

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            NRF_LOG_INFO("BLE security request rejected: pairing not supported");
            APP_ERROR_CHECK(sd_ble_gap_sec_params_reply(
                m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL));
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            NRF_LOG_INFO("BLE system attributes missing; setting defaults");
            APP_ERROR_CHECK(sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0));
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            NRF_LOG_WARNING("BLE GATTC timeout; disconnecting");
            APP_ERROR_CHECK(sd_ble_gap_disconnect(
                p_ble_evt->evt.gattc_evt.conn_handle,
                BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION));
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            NRF_LOG_WARNING("BLE GATTS timeout; disconnecting");
            APP_ERROR_CHECK(sd_ble_gap_disconnect(
                p_ble_evt->evt.gatts_evt.conn_handle,
                BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION));
            break;

        default:
            break;
    }
}

static void gatt_evt_handler(nrf_ble_gatt_t *p_gatt,
                             nrf_ble_gatt_evt_t const *p_evt)
{
    UNUSED_PARAMETER(p_gatt);

    if ((m_conn_handle == p_evt->conn_handle) &&
        (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective
                                 - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("ATT MTU updated, max data=%d", m_ble_nus_max_data_len);
    }
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_status_led_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_buzzer_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_adv_restart_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                adv_restart_timeout_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_keepalive_timer,
                                APP_TIMER_MODE_REPEATED,
                                keepalive_timeout_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_nus_ready_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                nus_ready_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

static void ble_stack_init(void)
{
    APP_ERROR_CHECK(nrf_sdh_enable_request());

    uint32_t ram_start = 0;
    APP_ERROR_CHECK(nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start));
    APP_ERROR_CHECK(nrf_sdh_ble_enable(&ram_start));

    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO,
                         ble_evt_handler, NULL);
}

static void gap_params_init(void)
{
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    APP_ERROR_CHECK(sd_ble_gap_device_name_set(&sec_mode,
                    (const uint8_t *)APP_BLE_DEVICE_NAME,
                    strlen(APP_BLE_DEVICE_NAME)));

    ble_gap_conn_params_t gap_conn_params;
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
    APP_ERROR_CHECK(sd_ble_gap_ppcp_set(&gap_conn_params));
}

static void gatt_init(void)
{
    APP_ERROR_CHECK(nrf_ble_gatt_init(&m_gatt, gatt_evt_handler));
    APP_ERROR_CHECK(nrf_ble_gatt_att_mtu_periph_set(&m_gatt,
                    NRF_SDH_BLE_GATT_MAX_MTU_SIZE));
}

static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void services_init(void)
{
    nrf_ble_qwr_init_t qwr_init = {0};
    qwr_init.error_handler = nrf_qwr_error_handler;
    APP_ERROR_CHECK(nrf_ble_qwr_init(&m_qwr, &qwr_init));

    ble_nus_init_t nus_init;
    memset(&nus_init, 0, sizeof(nus_init));
    nus_init.data_handler = nus_data_handler;
    APP_ERROR_CHECK(ble_nus_init(&m_nus, &nus_init));
}

static void conn_params_init(void)
{
    ble_conn_params_init_t cp_init;
    memset(&cp_init, 0, sizeof(cp_init));
    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    ret_code_t err_code = ble_conn_params_init(&cp_init);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("ble_conn_params_init failed: 0x%08X", err_code);
        NRF_LOG_FLUSH();
        APP_ERROR_CHECK(err_code);
    }
}

static void advertising_init(void)
{
    ble_advertising_init_t init;
    memset(&init, 0, sizeof(init));
    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance      = false;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.srdata.uuids_complete.uuid_cnt  = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids   = m_adv_uuids;
    init.config.ble_adv_fast_enabled     = true;
    init.config.ble_adv_fast_interval    = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout     = APP_ADV_DURATION;
    init.evt_handler                     = on_adv_evt;

    APP_ERROR_CHECK(ble_advertising_init(&m_advertising, &init));
    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void advertising_start(void)
{
    ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    if (err_code == NRF_ERROR_INVALID_STATE)
    {
        NRF_LOG_WARNING("Advertising already active");
        NRF_LOG_FLUSH();
        return;
    }

    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("ble_advertising_start failed: 0x%08X", err_code);
        NRF_LOG_FLUSH();
        APP_ERROR_CHECK(err_code);
    }

    NRF_LOG_INFO("SoftDevice accepted advertising start");
    NRF_LOG_FLUSH();

    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV,
                                       m_advertising.adv_handle, 4);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_WARNING("sd_ble_gap_tx_power_set failed: 0x%08X", err_code);
    }
}

void app_ble_init(void)
{
    timers_init();

    APP_ERROR_CHECK(nrf_pwr_mgmt_init());

    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();

#if RUN_CONN_PARAMS_MODULE
    conn_params_init();
#else
    NRF_LOG_INFO("BLE connection parameter module disabled for WebBluetooth debug");
#endif

    advertising_start();

#if APP_STANDBY_LED_HEARTBEAT
    APP_ERROR_CHECK(app_status_led_heartbeat_start(APP_TIMER_TICKS(1000)));
#else
    NRF_LOG_INFO("Standby LED heartbeat disabled for low power");
#endif

    NRF_LOG_INFO("[BLE] Init complete, advertising as '%s'", APP_BLE_DEVICE_NAME);
    NRF_LOG_FLUSH();
}
