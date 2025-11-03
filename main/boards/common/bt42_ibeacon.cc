#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_ibeacon_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <stdlib.h>
#include <vector>
#include <cstring> // For memcpy
#include "nvs_flash.h"
#include "nvs.h"
#include "fcble_hal.h"
#include "fcble_api.h"
#include "bt42_ibeacon.h"

namespace { // 使用匿名命名空间来隐藏内部实现细节              // 定义最大存储空间
// 日志标签
constexpr const char* TAG = "BT42IBeacon";
constexpr uint64_t PAIRING_TIMER_INTERVAL_US = 15 * 1000; 
} // namespace

static const uint8_t test_did[6] = {0x01,0x02,0x03,0x04,0x05,0x06};
static const uint8_t test_license[26] = {
    0x2a,0xa8,0x00,0x00,0xd4,0xa8,0xb2,0xed,\
    0x6d,0x31,0x09,0x8b,0xe3,0xc3,0xe4,0xe8,\
    0xe5,0xe2,0x97,0xbb,0x01,0x02,0x03,0x04,\
    0x05,0x06
};

BT42IBeacon::BT42IBeacon():pairing_timer_handle_(nullptr) {
}

BT42IBeacon::~BT42IBeacon() {
    if (pairing_timer_handle_) {
        StopPairing();
        pairing_timer_handle_ = nullptr;
    }
}

esp_err_t BT42IBeacon::StartPairing() {
    if (pairing_timer_handle_) {
        ESP_LOGW(TAG, "Pairing timer already running.");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "StartPairing()");
    // 初始化
    fcble_dev_info_t dev_info;

    memset(&dev_info, 0, sizeof(fcble_dev_info_t));
    dev_info.pid = 43050;
    memcpy(dev_info.did, test_did, 6);
    dev_info.license = const_cast<uint8_t*>(test_license);
    dev_info.ability = 0;
    dev_info.plat_id = 0;
    dev_info.vendor_id = 0;
    dev_info.soft_ver = 1;

    fcble_init(&dev_info);
    fcble_event_notify_register(&BT42IBeacon::fcble_event_notify_cb, this);

    // 开启配对
    fcble_pairing_start(0);

    ESP_LOGI(TAG, "Starting pairing mode timer (15ms interval)...");

    const esp_timer_create_args_t timer_args = {
        .callback = &BT42IBeacon::pairing_timer_callback_trampoline, // C 回调函数
        .arg = this,                                     // 传递 this 指针
        .dispatch_method = ESP_TIMER_TASK,
        .name = "BT42IBeacon_pairing_timer"                  // 方便调试
    };

    esp_err_t ret = esp_timer_create(&timer_args, &pairing_timer_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_timer_start_periodic(pairing_timer_handle_, PAIRING_TIMER_INTERVAL_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(ret));
        // 如果启动失败，删除已创建的定时器
        esp_timer_delete(pairing_timer_handle_);
        pairing_timer_handle_ = nullptr;
        return ret;
    }

    return true;
}

esp_err_t BT42IBeacon::StopPairing() {
    ESP_LOGI(TAG, "StopPairing()");
    if (!pairing_timer_handle_) {
        ESP_LOGW(TAG, "Pairing timer is not running.");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping and deleting pairing mode timer.");
    
    // 停止定时器。这会保证回调不再被触发。
    esp_err_t ret_stop = esp_timer_stop(pairing_timer_handle_);
    if (ret_stop != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop timer: %s", esp_err_to_name(ret_stop));
        // 即使停止失败，我们仍然尝试删除
    }

    // 删除定时器，释放资源
    esp_err_t ret_delete = esp_timer_delete(pairing_timer_handle_);
    if (ret_delete != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete timer: %s", esp_err_to_name(ret_delete));
        return ret_delete; // 返回删除操作的错误
    }
    
    // 将句柄置空，表示定时器已不存在
    pairing_timer_handle_ = nullptr;

    return ret_stop; // 返回停止操作的结果
}

bool BT42IBeacon::ControlPeerDevice(uint8_t light_level, uint8_t mode, uint8_t color_change)
{
    int32_t ret = fcble_led_groove_packet_send(light_level, mode, color_change);
    if (ret != 0) {
        ESP_LOGE(TAG, "fcble_led_groove_packet_send() failed: %d", ret);
    }
    return 0 == ret;
}

void BT42IBeacon::OnPairingTimer() {
    fcble_event_handle();
}

void BT42IBeacon::OnPairingStatus(pairing_status_cb_t cb) {
    pairing_status_cb_ = cb;
}

void BT42IBeacon::OnFcBleEventNotify(uint8_t type, uint8_t *data, uint8_t len)
{
    ESP_LOGI(TAG, "onFcBleEventNotify: type=%d, len=%d", type, len);
    switch(type)
    {
        case FCBLE_EVENT_NOTIFY_TEST_START:
            break;

        case FCBLE_EVENT_NOTIFY_TEST_DONE:
            break;

        case FCBLE_EVENT_NOTIFY_CONFIG_DONE:
            // 配对成功
            ESP_LOGI(TAG, "Pairing succeeded.");
            if (pairing_status_cb_) {
                pairing_status_cb_(true);
            }
            break;

        case FCBLE_EVENT_NOTIFY_GROUP_CHANGE:
            break;

        case FCBLE_EVENT_NOTIFY_ADDR_CHANGE:
            break;

        case FCBLE_EVENT_NOTIFY_RM_BIND:
            break;

        case FCBLE_EVENT_NOTIFY_RM_CLEAN:
            break;

        case FCBLE_EVENT_NOTIFY_TIME_SYNC:
            break;

        case FCBLE_EVENT_NOTIFY_TEMP_GROUP_SET:
            break;

        case FCBLE_EVENT_NOTIFY_STATUS_REPORT_REQ:
            break;

        case FCBLE_EVENT_NOTIFY_DATA_REPORT:
            break;

		case FCBLE_EVENT_NOTIFY_BYPASS_REQ:
			break;
        default:
            break;
    }
}

void BT42IBeacon::pairing_timer_callback_trampoline(void* arg) {
    BT42IBeacon* instance = static_cast<BT42IBeacon*>(arg);
    instance->OnPairingTimer();
}

void BT42IBeacon::fcble_event_notify_cb(uint8_t type, uint8_t * data, uint8_t len, void *ctx) {
    BT42IBeacon* instance = static_cast<BT42IBeacon*>(ctx);
    instance->OnFcBleEventNotify(type, data, len);
}