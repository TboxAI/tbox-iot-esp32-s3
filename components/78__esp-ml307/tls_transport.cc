#include "tls_transport.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_crt_bundle.h>
#include <cstring>
#include "lwip/sockets.h" // 包含 lwip 的 socket 定义

#define TAG "TlsTransport"

TlsTransport::TlsTransport() {
    tls_client_ = esp_tls_init();
}

TlsTransport::~TlsTransport() {
    if (tls_client_) {
        esp_tls_conn_destroy(tls_client_);
    }
}

bool TlsTransport::Connect(const char* host, int port) {
    esp_tls_cfg_t cfg = {};
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms = 10000;

    int ret = esp_tls_conn_new_sync(host, strlen(host), port, &cfg, tls_client_);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to connect to %s:%d", host, port);
        return false;
    }

    connected_ = true;

    // --- 步骤 2 & 3: 切换到非阻塞模式 ---
    int sockfd = -1;
    // 获取底层的 socket 文件描述符
    if (esp_tls_get_conn_sockfd(tls_client_, &sockfd) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get underlying socket FD");
        Disconnect(); // 清理连接
        return false;
    }
    
    if (sockfd < 0) {
        ESP_LOGE(TAG, "Invalid socket FD received: %d", sockfd);
        Disconnect();
        return false;
    }

    ESP_LOGI(TAG, "Switching socket (FD: %d) to non-blocking mode.", sockfd);

    // 获取当前 socket 的标志
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        ESP_LOGE(TAG, "Failed to get socket flags (fcntl, F_GETFL)");
        Disconnect();
        return false;
    }

    // 添加 O_NONBLOCK 标志
    flags |= O_NONBLOCK;

    // 设置新的标志
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        ESP_LOGE(TAG, "Failed to set socket to non-blocking (fcntl, F_SETFL)");
        Disconnect();
        return false;
    }

    ESP_LOGI(TAG, "Socket is now in non-blocking mode. Ready for high-throughput I/O.");

    return true;
}

void TlsTransport::Disconnect() {
    if (tls_client_) {
        esp_tls_conn_destroy(tls_client_);
        tls_client_ = nullptr;
    }
    connected_ = false;
}

int TlsTransport::Send(const char* data, size_t length) {
    int ret = esp_tls_conn_write(tls_client_, data, length);
    if (ret <= 0 && ret != ESP_TLS_ERR_SSL_WANT_WRITE && ret != ESP_TLS_ERR_SSL_WANT_READ) {
        connected_ = false;
        ESP_LOGE(TAG, "TLS发送失败: %d", ret);
    }
    return ret;
}

int TlsTransport::Receive(char* buffer, size_t bufferSize) {
    int ret = esp_tls_conn_read(tls_client_, buffer, bufferSize);

    if (ret == 0) {
        connected_ = false;
    } else if (ret < 0 && ret != ESP_TLS_ERR_SSL_WANT_WRITE && ret != ESP_TLS_ERR_SSL_WANT_READ) {
        ESP_LOGE(TAG, "TLS 读取失败: %d", ret);
    }
    return ret;
}
