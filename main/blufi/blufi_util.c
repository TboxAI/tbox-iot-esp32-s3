/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


/****************************************************************************
* This is a demo for bluetooth config wifi connection to ap. You can config ESP32 to connect a softap
* or config ESP32 as a softap to be connected by other device. APP can be downloaded from github
* android source code: https://github.com/EspressifApp/EspBlufi
* iOS source code: https://github.com/EspressifApp/EspBlufiForiOS
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <cJSON.h>
#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
#include "esp_bt.h"
#endif

#include "blufi_util.h"
extern void clear_saved_wifi();

#define EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY 3
#define EXAMPLE_INVALID_REASON                255
#define EXAMPLE_INVALID_RSSI                  -128

#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK

#define ESP_BLUFI_EVENT_MAX (ESP_BLUFI_EVENT_RECV_CUSTOM_DATA + 1)

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

#define WIFI_LIST_NUM   10

static wifi_config_t sta_config;
static wifi_config_t ap_config;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static uint8_t example_wifi_retry = 0;
static bool expect_wifi_scanning_ret = false;

/* store the station info for send back to phone */
static bool gl_sta_connected = false;
static bool gl_sta_got_ip = false;
static bool ble_is_connected = false;
static uint8_t gl_sta_bssid[6];
static uint8_t gl_sta_ssid[32];
static int gl_sta_ssid_len;
static int num_report_errors = 0;
static wifi_sta_list_t gl_sta_list;
static bool gl_sta_is_connecting = false;
static esp_blufi_extra_info_t gl_sta_conn_info;
static blufi_event_callback_t g_callback;
static void *g_thiz;

extern esp_netif_t * s_wifi_sta_netif;

static void example_record_wifi_conn_info(int rssi, uint8_t reason)
{
    memset(&gl_sta_conn_info, 0, sizeof(esp_blufi_extra_info_t));
    if (gl_sta_is_connecting) {
        gl_sta_conn_info.sta_max_conn_retry_set = true;
        gl_sta_conn_info.sta_max_conn_retry = EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY;
    } else {
        gl_sta_conn_info.sta_conn_rssi_set = true;
        gl_sta_conn_info.sta_conn_rssi = rssi;
        gl_sta_conn_info.sta_conn_end_reason_set = true;
        gl_sta_conn_info.sta_conn_end_reason = reason;
    }
}

static void example_wifi_connect(void)
{
    example_wifi_retry = 0;
    gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
    example_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON);
}

static bool example_wifi_reconnect(void)
{
    bool ret;
    if (gl_sta_is_connecting && example_wifi_retry++ < EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY) {
        BLUFI_INFO("BLUFI WiFi starts reconnection\n");
        gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
        example_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON);
        ret = true;
    } else {
        ret = false;
    }
    return ret;
}

static int softap_get_current_connection_number(void)
{
    esp_err_t ret;
    ret = esp_wifi_ap_get_sta_list(&gl_sta_list);
    if (ret == ESP_OK)
    {
        return gl_sta_list.num;
    }

    return 0;
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    wifi_mode_t mode;

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
        esp_blufi_extra_info_t info;

        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_get_mode(&mode);

        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, gl_sta_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;
        gl_sta_got_ip = true;
        if (ble_is_connected == true) {
            uint8_t mac[6];
#if CONFIG_IDF_TARGET_ESP32P4
            esp_wifi_get_mac(WIFI_IF_STA, mac);
#else
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
#endif
            char got_ip[128];
            snprintf(got_ip, sizeof(got_ip), "{\"cmd\":\"IP_EVENT_STA_GOT_IP\", \"station_mac_addr\": \"%02x:%02x:%02x:%02x:%02x:%02x\"}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            esp_blufi_send_custom_data((uint8_t *)got_ip, strlen(got_ip));
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_get_current_connection_number(), &info);
        } else {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }
        break;
    }
    default:
        break;
    }
    return;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    wifi_event_sta_connected_t *event;
    wifi_event_sta_disconnected_t *disconnected_event;
    wifi_mode_t mode;
    BLUFI_INFO("wifi_event_handler, event_id=%d\n", event_id);

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        example_wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        BLUFI_INFO("BLUFI WiFi connected");
        gl_sta_connected = true;
        gl_sta_is_connecting = false;
        event = (wifi_event_sta_connected_t*) event_data;
        memcpy(gl_sta_bssid, event->bssid, 6);
        memcpy(gl_sta_ssid, event->ssid, event->ssid_len);
        gl_sta_ssid_len = event->ssid_len;
        if (g_callback) {
            g_callback(g_thiz, EVENT_CONNECT_AP_CONNECTED, NULL); // notify connected
        }
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        /* Only handle reconnection during connecting */
        if (!expect_wifi_scanning_ret && gl_sta_connected == false && example_wifi_reconnect() == false) {
            gl_sta_is_connecting = false;
            disconnected_event = (wifi_event_sta_disconnected_t*) event_data;
            example_record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);
        }
        BLUFI_INFO("BLUFI WiFi disconnected");
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        gl_sta_connected = false;
        gl_sta_got_ip = false;
        memset(gl_sta_ssid, 0, 32);
        memset(gl_sta_bssid, 0, 6);
        gl_sta_ssid_len = 0;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        if (g_callback) {
            g_callback(g_thiz, EVENT_CONNECT_AP_DISCONNECTED, NULL);
        }
        const char *disconnected = "{\"cmd\":\"WIFI_EVENT_DISCONNECTED\"}";
        esp_blufi_send_custom_data((uint8_t *)disconnected, strlen(disconnected));
        break;
    case WIFI_EVENT_AP_START:
        esp_wifi_get_mode(&mode);

        /* TODO: get config or information of softap, then set to report extra_info */
        if (ble_is_connected == true) {
            if (gl_sta_connected) {
                esp_blufi_extra_info_t info;
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, gl_sta_bssid, 6);
                info.sta_bssid_set = true;
                info.sta_ssid = gl_sta_ssid;
                info.sta_ssid_len = gl_sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
            } else if (gl_sta_is_connecting) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &gl_sta_conn_info);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &gl_sta_conn_info);
            }
        } else {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }
        break;
    case WIFI_EVENT_SCAN_DONE: {
        BLUFI_INFO("BLUFI WIFI_EVENT_SCAN_DONE");
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        if (apCount == 0) {
            BLUFI_ERROR("No AP found");
            const char *no_ap = "{\"cmd\":\"WIFI_EVENT_SCAN_DONE\", \"ret\":\"No_AP_found\"}";
            esp_blufi_send_custom_data((uint8_t *)no_ap, strlen(no_ap));
            break;
        }
        wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        if (!ap_list) {
            BLUFI_ERROR("malloc error, ap_list is NULL");
            esp_wifi_clear_ap_list();
            break;
        }
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));
        esp_blufi_ap_record_t * blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
        if (!blufi_ap_list) {
            if (ap_list) {
                free(ap_list);
            }
            BLUFI_ERROR("malloc error, blufi_ap_list is NULL");
            break;
        }
        for (int i = 0; i < apCount; ++i)
        {
            blufi_ap_list[i].rssi = ap_list[i].rssi;
            memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
        }

        if (ble_is_connected == true) {
            esp_blufi_send_wifi_list(apCount, blufi_ap_list);
            if (g_callback) {
                g_callback(g_thiz, EVENT_CONNECT_AP_GOT_AP_LIST, NULL);
            }
        } else {
            BLUFI_INFO("BLUFI BLE is not connected yet\n");
        }

        esp_wifi_scan_stop();
        free(ap_list);
        free(blufi_ap_list);
        break;
    }
    case WIFI_EVENT_AP_STACONNECTED: {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        BLUFI_INFO("station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        BLUFI_INFO("station "MACSTR" leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
        break;
    }

    default:
        break;
    }
    return;
}

static void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    if (s_wifi_sta_netif == NULL) {
        s_wifi_sta_netif = esp_netif_create_default_wifi_sta();
        assert(s_wifi_sta_netif);
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    example_record_wifi_conn_info(EXAMPLE_INVALID_RSSI, EXAMPLE_INVALID_REASON);
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static esp_blufi_callbacks_t example_callbacks = {
    .event_cb = blufi_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

static const char* blufi_event_strings[] = {
    [ESP_BLUFI_EVENT_INIT_FINISH]                = "INIT_FINISH",
    [ESP_BLUFI_EVENT_DEINIT_FINISH]              = "DEINIT_FINISH",
    [ESP_BLUFI_EVENT_SET_WIFI_OPMODE]            = "SET_WIFI_OPMODE",
    [ESP_BLUFI_EVENT_BLE_CONNECT]                = "BLE_CONNECT",
    [ESP_BLUFI_EVENT_BLE_DISCONNECT]             = "BLE_DISCONNECT",
    [ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP]          = "REQ_CONNECT_TO_AP",
    [ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP]     = "REQ_DISCONNECT_FROM_AP",
    [ESP_BLUFI_EVENT_GET_WIFI_STATUS]            = "GET_WIFI_STATUS",
    [ESP_BLUFI_EVENT_DEAUTHENTICATE_STA]         = "DEAUTHENTICATE_STA",
    [ESP_BLUFI_EVENT_RECV_STA_BSSID]             = "RECV_STA_BSSID",
    [ESP_BLUFI_EVENT_RECV_STA_SSID]              = "RECV_STA_SSID",
    [ESP_BLUFI_EVENT_RECV_STA_PASSWD]            = "RECV_STA_PASSWD",
    [ESP_BLUFI_EVENT_RECV_SOFTAP_SSID]           = "RECV_SOFTAP_SSID",
    [ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD]         = "RECV_SOFTAP_PASSWD",
    [ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM]   = "RECV_SOFTAP_MAX_CONN_NUM",
    [ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE]      = "RECV_SOFTAP_AUTH_MODE",
    [ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL]        = "RECV_SOFTAP_CHANNEL",
    [ESP_BLUFI_EVENT_RECV_USERNAME]              = "RECV_USERNAME",
    [ESP_BLUFI_EVENT_RECV_CA_CERT]               = "RECV_CA_CERT",
    [ESP_BLUFI_EVENT_RECV_CLIENT_CERT]           = "RECV_CLIENT_CERT",
    [ESP_BLUFI_EVENT_RECV_SERVER_CERT]           = "RECV_SERVER_CERT",
    [ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY]       = "RECV_CLIENT_PRIV_KEY",
    [ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY]       = "RECV_SERVER_PRIV_KEY",
    [ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE]  = "RECV_SLAVE_DISCONNECT_BLE",
    [ESP_BLUFI_EVENT_GET_WIFI_LIST]              = "GET_WIFI_LIST",
    [ESP_BLUFI_EVENT_REPORT_ERROR]               = "REPORT_ERROR",
    [ESP_BLUFI_EVENT_RECV_CUSTOM_DATA]           = "RECV_CUSTOM_DATA",
    [ESP_BLUFI_EVENT_MAX]                        = "UNKNOWN_EVENT" // Sentinel for out-of-bounds access
};

static const char* blufi_event_to_string(esp_blufi_cb_event_t event)
{
    if (event >= 0 && event < ESP_BLUFI_EVENT_MAX) {
        return blufi_event_strings[event];
    }
    // Handle out-of-bounds or negative values safely
    return blufi_event_strings[ESP_BLUFI_EVENT_MAX]; 
}

/**
 * @brief 安全地处理接收到的自定义数据，检查 cmd 字段。
 *
 * @param data 指向接收数据的指针。
 * @param data_len 数据的长度。
 */
static void handle_custom_data(const uint8_t *data, uint32_t data_len)
{
    // 1. 基本的有效性检查
    if (data == NULL || data_len == 0) {
        BLUFI_ERROR("Received empty custom data.");
        return;
    }

    // cJSON 解析函数期望一个以 null 结尾的字符串。
    // 接收到的数据可能不是，所以我们需要创建一个临时的、以 null 结尾的缓冲区。
    // 这是最安全的做法，可以防止 cJSON 读取越界。
    char *json_string = (char *)malloc(data_len + 1);
    if (json_string == NULL) {
        BLUFI_ERROR("Failed to allocate memory for JSON string.");
        return;
    }
    memcpy(json_string, data, data_len);
    json_string[data_len] = '\0'; // 确保字符串以 null 结尾

    cJSON *root = cJSON_Parse(json_string);
    
    // 释放临时缓冲区，无论解析是否成功
    free(json_string);

    // 2. 检查 JSON 解析是否成功
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            BLUFI_ERROR("cJSON_Parse failed near: %s", error_ptr);
        } else {
            BLUFI_ERROR("cJSON_Parse failed, but no error pointer available.");
        }
        // 不需要调用 cJSON_Delete(root)，因为 root 本身就是 NULL
        return;
    }

    // 3. 提取 "cmd" 字段
    cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(root, "cmd");

    // 4. 验证字段是否存在、类型是否为字符串
    if (!cJSON_IsString(cmd_item) || (cmd_item->valuestring == NULL)) {
        BLUFI_ERROR("'cmd' field is not a valid string or is missing.");
        cJSON_Delete(root); // 清理 cJSON 对象
        return;
    }
    
    BLUFI_INFO("Received command: %s", cmd_item->valuestring);

    // 5. 比较命令字符串
    if (strcmp(cmd_item->valuestring, "REQ_CLEAR_SAVED_WIFI") == 0) {
        BLUFI_INFO("Received command 'REQ_CLEAR_SAVED_WIFI'.");
        
        clear_saved_wifi(); // 清除保存的 WiFi 信息
        
        // 向手机 App 发送一个确认回包
        const char *response = "{\"cmd\":\"ACK_CLEAR_SAVED_WIFI\", \"ret\":\"OK\"}";
        esp_blufi_send_custom_data((uint8_t *)response, strlen(response));
    } else {
        BLUFI_ERROR("Received unknown command: %s", cmd_item->valuestring);
    }

    // 6. 清理 cJSON 对象，释放所有相关内存
    cJSON_Delete(root);
}

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    BLUFI_INFO("BLUFI event: %s\n", blufi_event_to_string(event));
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        // BLUFI_INFO("BLUFI init finish\n");
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        // BLUFI_INFO("BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        BLUFI_INFO("BLUFI ble connect\n");
        ble_is_connected = true;
        expect_wifi_scanning_ret = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        BLUFI_INFO("BLUFI ble disconnect\n");
        ble_is_connected = false;
        expect_wifi_scanning_ret = false;
        blufi_security_deinit();
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        // BLUFI_INFO("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK( esp_wifi_set_mode(param->wifi_mode.op_mode) );
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        // BLUFI_INFO("BLUFI request wifi connect to AP\n");
        /* there is no wifi callback when the device has already connected to this wifi
        so disconnect wifi before connection.
        */
       if (g_callback) {
            g_callback(g_thiz, EVENT_CONNECT_REQ_CONNECT_TO_AP, NULL);
        }
        esp_wifi_disconnect();
        example_wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        // BLUFI_INFO("BLUFI request wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        num_report_errors++;
        BLUFI_ERROR("BLUFI report error, error code %d\n", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        if (num_report_errors > 3) {
            BLUFI_ERROR("BLUFI report error so many times, restart system");
            const char *rebooted = "{\"cmd\":\"SYSTEM_REBOOTED_AS_BLUFI_ERROR\"}";
            esp_blufi_send_custom_data((uint8_t *)rebooted, strlen(rebooted));
            vTaskDelay(pdMS_TO_TICKS(1500));
            esp_restart();
        }
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);

        if (gl_sta_connected) {
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
        } else if (gl_sta_is_connecting) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &gl_sta_conn_info);
        } else {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &gl_sta_conn_info);
        }
        BLUFI_INFO("BLUFI get wifi status from AP\n");
        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        BLUFI_INFO("blufi close a gatt connection");
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;
	case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        // BLUFI_INFO("Recv STA BSSID %s\n", sta_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_SSID:
        if (param->sta_ssid.ssid_len >= sizeof(sta_config.sta.ssid)/sizeof(sta_config.sta.ssid[0])) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            BLUFI_INFO("Invalid STA SSID\n");
            break;
        }
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        // BLUFI_INFO("Recv STA SSID %s\n", sta_config.sta.ssid);
        if (g_callback) {
                g_callback(g_thiz, EVENT_CONNECT_AP_RECV_SSID, (const char*)sta_config.sta.ssid);
        }
        break;
	case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        if (param->sta_passwd.passwd_len >= sizeof(sta_config.sta.password)/sizeof(sta_config.sta.password[0])) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            BLUFI_INFO("Invalid STA PASSWORD\n");
            break;
        }
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        sta_config.sta.threshold.authmode = EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        // BLUFI_INFO("Recv STA PASSWORD %s\n", sta_config.sta.password);
        if (g_callback) {
            g_callback(g_thiz, EVENT_CONNECT_AP_RECV_PASSWORD, (const char*)sta_config.sta.password);
        }
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
        if (param->softap_ssid.ssid_len >= sizeof(ap_config.ap.ssid)/sizeof(ap_config.ap.ssid[0])) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            BLUFI_INFO("Invalid SOFTAP SSID\n");
            break;
        }
        strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
        ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP SSID %s, ssid len %d\n", ap_config.ap.ssid, ap_config.ap.ssid_len);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
        if (param->softap_passwd.passwd_len >= sizeof(ap_config.ap.password)/sizeof(ap_config.ap.password[0])) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            BLUFI_INFO("Invalid SOFTAP PASSWD\n");
            break;
        }
        strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
        ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP PASSWORD %s len = %d\n", ap_config.ap.password, param->softap_passwd.passwd_len);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
        if (param->softap_max_conn_num.max_conn_num > 4) {
            return;
        }
        ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP MAX CONN NUM %d\n", ap_config.ap.max_connection);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
        if (param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX) {
            return;
        }
        ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP AUTH MODE %d\n", ap_config.ap.authmode);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        if (param->softap_channel.channel > 13) {
            return;
        }
        ap_config.ap.channel = param->softap_channel.channel;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP CHANNEL %d\n", ap_config.ap.channel);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST:{
        wifi_scan_config_t scanConf = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false
        };
        esp_err_t ret = esp_wifi_scan_start(&scanConf, true);
        if (ret != ESP_OK) {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        // BLUFI_INFO("Recv Custom Data %d\n", (int)param->custom_data.data_len);
        handle_custom_data(param->custom_data.data, param->custom_data.data_len);
        break;
	case ESP_BLUFI_EVENT_RECV_USERNAME:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CA_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
        /* Not handle currently */
        break;;
	case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        /* Not handle currently */
        break;
    default:
        break;
    }
}

int start_blufi(blufi_event_callback_t callback, void *thiz)
{
    g_callback = callback;
    g_thiz = thiz;

    initialise_wifi();
    esp_err_t ret;

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    ret = esp_blufi_controller_init();
    if (ret) {
        BLUFI_ERROR("%s BLUFI controller init failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }
#endif

    ret = esp_blufi_host_and_cb_init(&example_callbacks);
    if (ret) {
        BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(ret));
        return -2;
    }

    BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
    return 0;
}

int stop_blufi(void)
{
    g_callback = NULL;
    g_thiz = NULL;
#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED       
    esp_blufi_controller_deinit();
#endif
    blufi_security_deinit();
    esp_err_t ret;
    ret = esp_blufi_host_deinit();
    if (ret) {
        BLUFI_ERROR("%s deinit host failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }
    return 0;
}