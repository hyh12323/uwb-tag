#include "app_ble.h"
#include "app_buzzer.h"
#include "app_error.h"
#include "app_nfc_tag.h"
#include "app_status_led.h"
#include "app_uwb.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// 仅在硬件调试阶段置 1。UWB 验证流程会给 DWM3000 上电并执行 SPI 读写，
// 正式待机固件中保持关闭，避免增加启动耗时和待机功耗。
#define RUN_UWB_VERIFY_ON_BOOT 0

int main(void)  
{
    // P0.10 上电后可能处于 nRF52 默认 NFC 复用功能。蜂鸣器 EN1 需要把
    // P0.10 当普通 GPIO 使用，因此最先检查并配置 UICR。若 UICR 被改写，
    // 该函数可能触发一次系统复位。
    app_buzzer_nfc_pins_as_gpio_init();

    // 初始化所有业务 GPIO，并放到安全默认状态。UWB 默认断电；BLE 广播
    // 和 NFC 标签写入不需要 DWM3000 参与。
    app_status_led_gpio_init();
    app_buzzer_gpio_init();
    app_uwb_gpio_init();

    // 本工程使用 J-Link RTT 输出日志。后续模块初始化过程会打印 NRF_LOG，
    // 因此日志系统必须先初始化。
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    // 给外部 NTAG I2C Plus 写入默认 NDEF URL/文本。写入完成后，手机读取
    // NFC 主要由标签芯片被动完成，MCU 不需要持续运行 NFC 任务。
    app_nfc_tag_program_default();

    // 初始化 BLE 子系统：app_timer、电源管理、SoftDevice、GAP/GATT、NUS、
    // 广播、连接保活以及连接异常看门狗。
    app_ble_init();

#if RUN_UWB_VERIFY_ON_BOOT
    // UWB 硬件冒烟测试：给 DWM3000 上电、初始化 SPI、读取 DEV_ID。
    // 正式待机版本默认关闭。
    app_uwb_hardware_verify();
#else
    NRF_LOG_INFO("UWB hardware verify skipped on boot");
    NRF_LOG_FLUSH();
#endif

    // 启动完成提示。执行到这里后，设备应以 APP_BLE_DEVICE_NAME 进入广播，
    // 等待 Board B 或 WebBluetooth 调试页面连接。
    NRF_LOG_INFO("========================================");
    NRF_LOG_INFO("CC-UWB-TAG firmware ready.");
    NRF_LOG_INFO("Advertising as '%s', waiting for Board B...", APP_BLE_DEVICE_NAME);
    NRF_LOG_INFO("========================================");
    NRF_LOG_FLUSH();

    for (;;)
    {
        // 主循环唯一入口。优先处理 RTT 延迟日志；没有日志需要输出时，
        // 进入 SoftDevice 兼容的低功耗 idle。
        app_ble_process();
    }
}
