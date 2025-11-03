#include "web_socket.h"
#include <esp_log.h>
#include <esp_pthread.h>
#include <cstdlib>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "WebSocket";

// 修正后的 base64_encode 函数
static std::string base64_encode(const unsigned char *data, size_t len) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string ret;
    ret.reserve(((len + 2) / 3) * 4);

    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

WebSocket::WebSocket(Transport *transport) : transport_(transport) {}

WebSocket::~WebSocket() {
    Disconnect(); // 确保资源被正确释放
    delete transport_;
}

void WebSocket::SetHeader(const char* key, const char* value) {
    headers_[key] = value;
}

void WebSocket::SetReceiveBufferSize(size_t size) {
    receive_buffer_size_ = size;
}

bool WebSocket::IsConnected() const {
    return transport_->connected();
}

bool WebSocket::Handshake(const std::string& host, const std::string& port, const std::string& path) {
    // 使用 transport 建立连接
    if (!transport_->Connect(host.c_str(), std::stoi(port))) {
        ESP_LOGE(TAG, "Failed to connect to server");
        return false;
    }

    // 设置 WebSocket 特定的头部
    SetHeader("Upgrade", "websocket");
    SetHeader("Connection", "Upgrade");
    SetHeader("Sec-WebSocket-Version", "13");

    // 生成随机的 Sec-WebSocket-Key
    unsigned char key[16];
    for (int i = 0; i < 16; ++i) {
        key[i] = rand() % 256;
    }
    std::string base64_key = base64_encode(key, 16);
    SetHeader("Sec-WebSocket-Key", base64_key.c_str());

    // 发送 WebSocket 握手请求
    std::string request = "GET " + path + " HTTP/1.1\r\n";
    if (headers_.find("Host") == headers_.end()) {
        request += "Host: " + host + "\r\n";
    }
    for (const auto& header : headers_) {
        request += header.first + ": " + header.second + "\r\n";
    }
    request += "\r\n";

    if (!SendAllRaw(request.c_str(), request.length())) {
        ESP_LOGE(TAG, "Failed to send WebSocket handshake request");
        transport_->Disconnect();
        return false;
    }

    std::string buffer;
    char c = 0;
    while (transport_->connected()) {
        int ret = transport_->Receive(&c, 1);
        if (ret == 1) {
            buffer.push_back(c);
            if (buffer.size() >= 4 && buffer.substr(buffer.size() - 4) == "\r\n\r\n") {
                break;
            }
        } else if (ret <= 0 && ret != MBEDTLS_ERR_SSL_WANT_READ) {
            ESP_LOGE(TAG, "Failed to receive handshake response: %d", ret);
            transport_->Disconnect();
            return false;
        }
    }

    if (buffer.find("HTTP/1.1 101") == std::string::npos) {
        ESP_LOGE(TAG, "WebSocket handshake failed. Response:\n%s", buffer.c_str());
        transport_->Disconnect();
        return false;
    }

    return true;
}

bool WebSocket::Connect(const char* uri) {
    // ... (URI 解析代码保持不变) ...
    std::string uri_str(uri);
    std::string protocol, host, port, path;
    size_t pos = 0;
    size_t next_pos = 0;

    // 解析协议
    next_pos = uri_str.find("://");
    if (next_pos == std::string::npos) {
        ESP_LOGE(TAG, "Invalid URI format");
        return false;
    }
    protocol = uri_str.substr(0, next_pos);
    pos = next_pos + 3;

    // 解析主机
    next_pos = uri_str.find(':', pos);
    if (next_pos == std::string::npos) {
        next_pos = uri_str.find('/', pos);
        if (next_pos == std::string::npos) {
            host = uri_str.substr(pos);
            path = "/";
        } else {
            host = uri_str.substr(pos, next_pos - pos);
            path = uri_str.substr(next_pos);
        }
        port = (protocol == "wss") ? "443" : "80";
    } else {
        host = uri_str.substr(pos, next_pos - pos);
        pos = next_pos + 1;
        
        // 解析端口
        next_pos = uri_str.find('/', pos);
        if (next_pos == std::string::npos) {
            port = uri_str.substr(pos);
            path = "/";
        } else {
            port = uri_str.substr(pos, next_pos - pos);
            path = uri_str.substr(next_pos);
        }
    }


    ESP_LOGI(TAG, "Connecting to %s://%s:%s%s", protocol.c_str(), host.c_str(), port.c_str(), path.c_str());

    if (!Handshake(host, port, path)) {
        return false;
    }
    
    ESP_LOGI(TAG, "WebSocket handshake successful");

    if (on_connected_) {
        on_connected_();
    }
    
    stop_io_task_ = false;

    // 启动 I/O 任务线程
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "websocket_io";
    cfg.stack_size = 4096;
    cfg.prio = 5;
    esp_pthread_set_cfg(&cfg);

    io_thread_ = std::thread([this]() { IoTask(); });

    return true;
}

void WebSocket::Disconnect() {
    stop_io_task_ = true;
    ESP_LOGI(TAG, "stop_io_task_ case when WebSocket::Disconnect()");
    queue_cond_var_.notify_one(); // 唤醒可能在等待的 I/O 线程

    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    if (transport_->connected()) {
        transport_->Disconnect();
    }

    // 清空队列
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while(!send_queue_.empty()) {
        send_queue_.pop();
    }
}


bool WebSocket::Send(const std::string& data) {
    return QueueDataFrame(data.data(), data.size(), false, true);
}

bool WebSocket::Send(const void* data, size_t len, bool binary, bool fin) {
    return QueueDataFrame(data, len, binary, fin);
}

void WebSocket::Ping() {
    QueueControlFrame(0x9, nullptr, 0);
}

void WebSocket::Close() {
    if (transport_->connected()) {
        QueueControlFrame(0x8, nullptr, 0);
    }
}

// --- 新增的队列操作 ---
bool WebSocket::QueueDataFrame(const void* data, size_t len, bool binary, bool fin) {
    if (!transport_->connected() || stop_io_task_) {
        ESP_LOGE(TAG, "Cannot send, not connected");
        return false;
    }

    if (len > 65535) {
        ESP_LOGE(TAG, "Data too large, maximum supported size is 65535 bytes");
        return false;
    }

    std::vector<uint8_t> frame;
    frame.reserve(len + 10);

    uint8_t first_byte = (fin ? 0x80 : 0x00);
    if (binary) {
        first_byte |= 0x02;
    } else if (!continuation_) {
        first_byte |= 0x01;
    }
    frame.push_back(first_byte);

    if (len < 126) {
        frame.push_back(0x80 | len);
    } else {
        frame.push_back(0x80 | 126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    }

    uint8_t mask[4];
    for (int i = 0; i < 4; ++i) mask[i] = rand() & 0xFF;
    frame.insert(frame.end(), mask, mask + 4);

    const uint8_t* payload = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(payload[i] ^ mask[i % 4]);
    }

    continuation_ = !fin;

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (send_queue_.size() >= MAX_SEND_QUEUE_SIZE) {
            ESP_LOGW(TAG, "Send queue is full (size: %u). Discarding oldest message to enqueue a data frame.", send_queue_.size());
            send_queue_.pop();
        }
        send_queue_.push(std::move(frame));
    }
    queue_cond_var_.notify_one();
    return true;
}

bool WebSocket::QueueControlFrame(uint8_t opcode, const void* data, size_t len) {
    if (!transport_->connected() || stop_io_task_) {
        return false;
    }

    if (len > 125) {
        ESP_LOGE(TAG, "Control frame payload too large");
        return false;
    }

    std::vector<uint8_t> frame;
    frame.reserve(len + 6);
    frame.push_back(0x80 | opcode);
    frame.push_back(0x80 | len);

    uint8_t mask[4];
    for (int i = 0; i < 4; ++i) mask[i] = rand() & 0xFF;
    frame.insert(frame.end(), mask, mask + 4);

    const uint8_t* payload = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(payload[i] ^ mask[i % 4]);
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (send_queue_.size() >= MAX_SEND_QUEUE_SIZE) {
            ESP_LOGW(TAG, "Send queue is full (size: %zu). Discarding oldest message to enqueue a control frame.", send_queue_.size());
            send_queue_.pop();
        }
        send_queue_.push(std::move(frame));
    }
    queue_cond_var_.notify_one();
    return true;
}
// -----------------------

void WebSocket::OnConnected(std::function<void()> callback) {
    on_connected_ = callback;
}
void WebSocket::OnDisconnected(std::function<void()> callback) {
    on_disconnected_ = callback;
}
void WebSocket::OnData(std::function<void(const char*, size_t, bool binary)> callback) {
    on_data_ = callback;
}
void WebSocket::OnError(std::function<void(int)> callback) {
    on_error_ = callback;
}

void WebSocket::IoTask() {
    auto receive_buffer = new char[receive_buffer_size_];
    size_t buffer_offset = 0;

    std::vector<char> current_message;
    bool is_fragmented = false;
    bool is_binary = false;
    ESP_LOGI(TAG, "I/O task started");

    while (!stop_io_task_) {
        // --- 1. 处理发送队列 ---
        std::queue<std::vector<uint8_t>> local_send_queue;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 使用 wait_for 实现带超时的等待，避免在无数据时空转
            // 同时，它也作为 IoTask 的主延时
            queue_cond_var_.wait_for(lock, std::chrono::milliseconds(20), [this] {
                return !send_queue_.empty() || stop_io_task_;
            });

            if (stop_io_task_) {
                ESP_LOGI(TAG, "stop_io_task_ case 1");
                break;
            }

            // 将共享队列中的所有任务移动到本地队列，以快速释放锁
            if (!send_queue_.empty()) {
                local_send_queue.swap(send_queue_);
            }
        } // 锁在这里释放

        // 在锁外发送数据
        size_t send_failed_times = 0;
        while (!local_send_queue.empty()) {
            auto& frame = local_send_queue.front();
            if (!SendAllRaw(frame.data(), frame.size())) {
                send_failed_times++;
                ESP_LOGW(TAG, "Failed to send frame, send_failed_times:%d", send_failed_times);
            }
            local_send_queue.pop();
        }

        if (send_failed_times > 2 || transport_->connected() == false) {
            ESP_LOGE(TAG, "send_failed_times: %d, connected: %d, io task exit!", send_failed_times, transport_->connected());
            break;
        }

        if (stop_io_task_) {
            ESP_LOGI(TAG, "stop_io_task_ case 2");
            break;
        }

        // --- 2. 处理接收 ---
        int ret = transport_->Receive(receive_buffer + buffer_offset, receive_buffer_size_ - buffer_offset);
        if (ret < 0) {
            // MBEDTLS_ERR_SSL_WANT_READ 和 WANT_WRITE 是正常情况，直接继续循环
            if (ret != ESP_TLS_ERR_SSL_WANT_WRITE && ret != ESP_TLS_ERR_SSL_WANT_READ) {
                if (on_error_) on_error_(ret);
                ESP_LOGE(TAG, "Transport receive error: %d, disconnecting.", ret);
                break; // 致命错误，退出循环
            }
        } else if (ret == 0) {
             ESP_LOGE(TAG, "Connection closed by peer.");
             break; // 连接正常关闭
        } else if (ret > 0) {
            buffer_offset += ret;
            size_t frame_start = 0;

             while (frame_start < buffer_offset) {
                if (buffer_offset - frame_start < 2) break; // 需要更多数据

                uint8_t opcode = receive_buffer[frame_start] & 0x0F;
                bool fin = (receive_buffer[frame_start] & 0x80) != 0;
                uint8_t mask_bit = receive_buffer[frame_start + 1] & 0x80;
                uint64_t payload_length = receive_buffer[frame_start + 1] & 0x7F;

                size_t header_length = 2;
                if (payload_length == 126) {
                    if (buffer_offset - frame_start < 4) break; // 需要更多数据
                    payload_length = (static_cast<uint8_t>(receive_buffer[frame_start + 2]) << 8) | static_cast<uint8_t>(receive_buffer[frame_start + 3]);
                    header_length += 2;
                } else if (payload_length == 127) {
                    // 64位长度处理，注意类型转换
                    if (buffer_offset - frame_start < 10) break; 
                    payload_length = 0;
                    for (int i = 0; i < 8; ++i) {
                        payload_length = (payload_length << 8) | static_cast<uint8_t>(receive_buffer[frame_start + 2 + i]);
                    }
                    header_length += 8;
                }

                if (mask_bit) { // 来自服务器的帧不应该有 mask
                    ESP_LOGE(TAG, "Received a masked frame from the server, closing connection.");
                    stop_io_task_ = true;
                    break;
                }

                if (buffer_offset - frame_start < header_length + payload_length) break;

                char* payload = receive_buffer + frame_start + header_length;

                switch (opcode) {
                    case 0x0: case 0x1: case 0x2:
                        if (opcode != 0x0 && is_fragmented) { ESP_LOGE(TAG, "Received new message while fragmenting"); break; }
                        if (opcode != 0x0) {
                            is_fragmented = !fin;
                            is_binary = (opcode == 0x2);
                            current_message.clear();
                        }
                        current_message.insert(current_message.end(), payload, payload + payload_length);
                        if (fin) {
                            if (on_data_) on_data_(current_message.data(), current_message.size(), is_binary);
                            is_fragmented = false;
                        }
                        break;
                    case 0x8:
                        ESP_LOGE(TAG, "Received close frame.");
                        stop_io_task_ = true;
                        break;
                    case 0x9:
                        QueueControlFrame(0xA, payload, payload_length);
                        break;
                    case 0xA: break; 
                    default: ESP_LOGE(TAG, "Unknown opcode: %d", opcode); break;
                }

                if (stop_io_task_) break;
                frame_start += header_length + payload_length;
            }

            if (stop_io_task_) break;

            if (frame_start > 0 && frame_start <= buffer_offset) {
                memmove(receive_buffer, receive_buffer + frame_start, buffer_offset - frame_start);
                buffer_offset -= frame_start;
            }

            if (buffer_offset >= receive_buffer_size_) {
                ESP_LOGE(TAG, "Receive buffer overflow");
                stop_io_task_ = true;
            }
        }
    }

    ESP_LOGW(TAG, "I/O task finished, stop_io_task_: %d", stop_io_task_ ? 1 : 0);
    delete[] receive_buffer;
    
    if (transport_->connected()) {
        transport_->Disconnect();
    }
    if (on_disconnected_) {
        on_disconnected_();
    }
}

bool WebSocket::SendAllRaw(const void* data, size_t len) {
    auto ptr = (char*)data;

    while (transport_->connected() && len > 0) {
        int sent = transport_->Send(ptr, len);
        if (sent == ESP_TLS_ERR_SSL_WANT_WRITE || sent == ESP_TLS_ERR_SSL_WANT_READ) {
            vTaskDelay(pdMS_TO_TICKS(3));
        } else if (sent < 0) {
            ESP_LOGE(TAG, "SendAllRaw failed with error: %d", sent);
            return false;
        }
        ptr += sent;
        len -= sent;
    }
    return true;
}
