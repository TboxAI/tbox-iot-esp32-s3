#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_ibeacon_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "fcble_hal.h"
#include "fcble_api.h"

// --- NVS 配置常量 ---
#define NVS_NAMESPACE "fcble_hal_nvm"
#define NVS_BLOB_KEY "storage_blob"
#define NVM_MAX_SIZE 256
#define TAG "fcble_hal"

/**
  ESP32 平台适配
 */

/**
 * @brief 每个 64Hz tick 对应的微秒数 (1,000,000 us / 64 Hz = 15625 us).
 * 
 * 使用 const or constexpr 定义这个魔术数字，可以提高代码的可读性和可维护性。
 * 使用 int64_t 保证在和 esp_timer_get_time() 的返回值运算时类型匹配。
 */
const int64_t MICROSECONDS_PER_64HZ_TICK = 1000000 / 64;

/**
 * @brief 获取系统从启动开始以 64Hz 频率计数的 ticks.
 *
 * 这个函数基于 ESP32 的高精度硬件定时器实现，与 FreeRTOS 的系统 tick 无关，
 * 保证了无论 FreeRTOS 配置如何，都能提供稳定和精确的 64Hz tick 计数。
 * 
 * @return uint32_t 当前的 64Hz ticks 计数值。该值会在大约 777 天后回绕 (overflow)。
 */
uint32_t fcble_hal_system_ticks(void)
{
    // 1. 获取自系统启动以来的总微秒数
    int64_t current_microseconds = esp_timer_get_time();

    // 2. 通过整除计算出经过了多少个 64Hz 的时间间隔
    //    C++ 的整型除法会自动舍去小数部分，正好是我们想要的结果。
    uint32_t ticks = current_microseconds / MICROSECONDS_PER_64HZ_TICK;

    return ticks;
}

int32_t fcble_hal_nvm_write(uint16_t addr, uint8_t *data, uint8_t len)
{
    // 1. 参数校验
    if (data == NULL) {
        ESP_LOGE(TAG, "Write failed: data pointer is null.");
        return -1; // 或者更具体的错误码
    }
    if (addr + len > NVM_MAX_SIZE) {
        ESP_LOGE(TAG, "Write failed: address range [%u, %u) exceeds max size %zu.", addr, addr + len, NVM_MAX_SIZE);
        return -2;
    }
    if (len == 0) {
        return 0; // 写入0字节，直接成功
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // 2. 打开 NVS 命名空间
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return -3;
    }

    // 3. Read-Modify-Write 策略
    uint8_t nvm_image[NVM_MAX_SIZE] = { 0 }; // 创建一个内存中的镜像，初始化为0
    size_t required_size = NVM_MAX_SIZE;

    // 尝试读取现有的 blob
    err = nvs_get_blob(nvs_handle, NVS_BLOB_KEY, nvm_image, &required_size);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to read existing blob: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -4;
    }
    // 如果 key 不存在 (ESP_ERR_NVS_NOT_FOUND)，nvm_image 已经是全零，正好符合首次写入的场景。

    // 4. 在内存中修改数据
    memcpy(nvm_image + addr, data, len);

    // 5. 将修改后的整个 blob 写回 NVS
    err = nvs_set_blob(nvs_handle, NVS_BLOB_KEY, nvm_image, NVM_MAX_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write blob back to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -5;
    }

    // 6. 提交更改
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -6;
    }

    // 7. 关闭句柄
    nvs_close(nvs_handle);
    ESP_LOGD(TAG, "Successfully wrote %u bytes at address %u.", len, addr);
    return 0; // 成功
}

int32_t fcble_hal_nvm_read(uint16_t addr, uint8_t *buf, uint8_t len)
{
    // 1. 参数校验
    if (buf == NULL) {
        ESP_LOGE(TAG, "Read failed: buffer pointer is null.");
        return -1;
    }
    if (addr + len > NVM_MAX_SIZE) {
        ESP_LOGE(TAG, "Read failed: address range [%u, %u) exceeds max size %zu.", addr, addr + len, NVM_MAX_SIZE);
        return -2;
    }
    if (len == 0) {
        return 0; // 读取0字节，直接成功
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // 2. 打开 NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return -3;
    }

    // 3. 读取整个 blob 到一个临时 buffer
    uint8_t nvm_image[NVM_MAX_SIZE] = { 0 };
    size_t required_size = NVM_MAX_SIZE;
    
    err = nvs_get_blob(nvs_handle, NVS_BLOB_KEY, nvm_image, &required_size);

    // 4. 处理读取结果
    if (err == ESP_OK) {
        // 成功读取，从内存镜像中拷贝需要的部分到用户 buffer
        memcpy(buf, nvm_image + addr, len);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 如果 key 不存在，说明从未写入过。根据通用约定，返回全0。
        ESP_LOGW(TAG, "NVM key '%s' not found. Returning zeroed buffer.", NVS_BLOB_KEY);
        memset(buf, 0, len);
    } else {
        // 其他读取错误
        ESP_LOGE(TAG, "Failed to read blob from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -4;
    }

    // 5. 关闭句柄
    nvs_close(nvs_handle);
    ESP_LOGD(TAG, "Successfully read %u bytes from address %u.", len, addr);
    return 0; // 成功
}

/**
 * @brief 将二进制数据以十六进制字符串格式打印到缓冲区。
 * 
 * @param buffer       目标字符缓冲区，用于存放结果。
 * @param buffer_size  目标缓冲区的总大小。
 * @param data         需要转换的二进制数据源。
 * @param data_len     二进制数据的长度（字节）。
 * @return int         成功时返回写入缓冲区的字符数（不包括末尾的 '\0'），
 *                     如果缓冲区大小不足则返回 -1。
 */
int data_to_hex_string(char *buffer, size_t buffer_size, const uint8_t *data, size_t data_len) {
    // 检查输入参数是否有效
    if (buffer == NULL || data == NULL) {
        return -1;
    }
    
    // 计算需要的缓冲区大小：每个字节转为2个hex字符 + 末尾的 '\0'
    size_t required_size = data_len * 2 + 1;
    if (buffer_size < required_size) {
        // 缓冲区太小，无法存放完整的十六进制字符串
        return -1;
    }

    // 用于跟踪当前在 buffer 中的写入位置
    char *ptr = buffer;
    for (size_t i = 0; i < data_len; i++) {
        // %02X: 格式化为2位、大写的十六进制数，不足2位时前面补0
        sprintf(ptr, "%02X", data[i]);
        ptr += 2; // 每写入一个字节（2个hex字符），指针向后移动2位
    }
    
    // 在字符串末尾添加 null 终止符
    *ptr = '\0';
    
    return data_len * 2;
}

extern esp_ble_ibeacon_vendor_t vendor_config;

///Declare static functions
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
        esp_ble_gap_start_advertising(&ble_adv_params);
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        ESP_LOGE(TAG, "esp_gap_cb: ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT event");
        // the unit of the duration is second, 0 means scan permanently
        uint32_t duration = 0;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scanning start failed, error %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Scanning start successfully");
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed, error %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            /* Search for BLE iBeacon Packet */
            esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(scan_result->scan_rst.ble_adv);
            // ESP_LOGE(TAG, "----------iBeacon Found----------");
            // ESP_LOGE(TAG, "Device address: " ESP_BD_ADDR_STR "", ESP_BD_ADDR_HEX(scan_result->scan_rst.bda));
            // ESP_LOG_BUFFER_HEX("IBEACON: Proximity UUID", ibeacon_data->ibeacon_vendor.proximity_uuid, ESP_UUID_LEN_128);

            uint16_t major = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.major);
            uint16_t minor = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.minor);
            // ESP_LOGE(TAG, "Major: 0x%04x (%d)", major, major);
            // ESP_LOGE(TAG, "Minor: 0x%04x (%d)", minor, minor);
            // ESP_LOGE(TAG, "Measured power (RSSI at a 1m distance): %d dBm", ibeacon_data->ibeacon_vendor.measured_power);
            // ESP_LOGE(TAG, "RSSI of packet: %d dbm", scan_result->scan_rst.rssi);
            // char hex_buffer[64] = {0}; 
            // int written_chars = data_to_hex_string(hex_buffer, sizeof(hex_buffer), scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len);
            // ESP_LOGE(TAG, "before fcble_ble_frame_handle(), reveived data: %s, datalen: %d", hex_buffer, scan_result->scan_rst.adv_data_len);
            uint8_t pdu[40] = {0};
            if (scan_result->scan_rst.adv_data_len <= 31) {
                memcpy(pdu + 8, scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len);
                fcble_ble_frame_handle(pdu, scan_result->scan_rst.adv_data_len + 8);
                // ESP_LOGE(TAG, "after fcble_ble_frame_handle(), reveived data: %s", hex_buffer);
            }
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Scanning stop failed, error %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Scanning stop successfully");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Advertising stop failed, error %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Advertising stop successfully");
        }
        break;

    default:
        break;
    }
}

void ble_ibeacon_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(TAG, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }
}

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

void ble_ibeacon_init(void)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_ibeacon_appRegister();

    esp_ble_gap_set_scan_params(&ble_scan_params);
}

/**
 * 蓝牙初始化
 */
int32_t fcble_hal_ble_init(uint8_t channel) {
    ESP_LOGI("MEMORY_DIAG", ">>> Preparing to initialize Bluetooth <<<");
    ESP_LOGI("MEMORY_DIAG", "----------------------------------------------------");

    // 打印所有类型的堆内存信息
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    ESP_LOGI("MEMORY_DIAG", "Total Free Heap: %u bytes", info.total_free_bytes);
    ESP_LOGI("MEMORY_DIAG", "Total Allocated Heap: %u bytes", info.total_allocated_bytes);
    ESP_LOGI("MEMORY_DIAG", "Largest Free Block: %u bytes", info.largest_free_block);
    ESP_LOGI("MEMORY_DIAG", "Minimum Free Heap Ever: %u bytes", info.minimum_free_bytes);

    ESP_LOGI("MEMORY_DIAG", "----------------------------------------------------");

    // 特别打印 DRAM (DMA-capable) 的信息，这是最关键的
    ESP_LOGI("MEMORY_DIAG", "Free DRAM (DMA): %u bytes", heap_caps_get_free_size(MALLOC_CAP_DMA));
    ESP_LOGI("MEMORY_DIAG", "Largest Free DRAM Block: %u bytes", heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

    ESP_LOGI("MEMORY_DIAG", "----------------------------------------------------");

    ble_ibeacon_init();
    return ESP_OK;
}

int32_t fcble_hal_ble_frame_send(uint8_t *pdu, uint8_t pdu_len) {
    return esp_ble_gap_config_adv_data_raw(pdu + 8, pdu_len - 8);
}