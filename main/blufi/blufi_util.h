/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


#pragma once

#include "esp_blufi_api.h"
#include "esp_blufi.h"

#define BLUFI_UTIL_TAG "BLUFI"
#define BLUFI_INFO(fmt, ...)   ESP_LOGI(BLUFI_UTIL_TAG, fmt, ##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...)  ESP_LOGE(BLUFI_UTIL_TAG, fmt, ##__VA_ARGS__)
typedef enum {
    EVENT_CONNECT_AP_FAIL = -1,
    EVENT_CONNECT_AP_CONNECTED = 0,
    EVENT_CONNECT_AP_WIP = 1,
    EVENT_CONNECT_AP_DISCONNECTED = 2,
    EVENT_CONNECT_AP_GOT_AP_LIST = 3,
    EVENT_CONNECT_AP_RECV_SSID = 4,
    EVENT_CONNECT_AP_RECV_PASSWORD = 5,
    EVENT_CONNECT_REQ_CONNECT_TO_AP = 6
} blufi_event_t;

typedef void (*blufi_event_callback_t)(void *thiz, blufi_event_t event, const char *info);

#ifdef __cplusplus
extern "C" {
#endif
void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

int blufi_security_init(void);
void blufi_security_deinit(void);
int esp_blufi_gap_register_callback(void);
esp_err_t esp_blufi_host_init(void);
esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks);
esp_err_t esp_blufi_host_deinit(void);
esp_err_t esp_blufi_controller_init(void);
esp_err_t esp_blufi_controller_deinit(void);

int start_blufi(blufi_event_callback_t callback, void *thiz);
int stop_blufi(void);
#ifdef __cplusplus
}
#endif