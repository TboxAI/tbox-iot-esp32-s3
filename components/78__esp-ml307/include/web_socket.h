#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "transport.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <esp_tls.h>

const size_t MAX_SEND_QUEUE_SIZE = 60;

class WebSocket {
public:
    WebSocket(Transport *transport);
    ~WebSocket();

    void SetHeader(const char* key, const char* value);
    void SetReceiveBufferSize(size_t size);
    bool IsConnected() const;

    bool Connect(const char* uri);
    void Disconnect();

    // 异步发送，立即返回
    bool Send(const std::string& data);
    bool Send(const void* data, size_t len, bool binary = false, bool fin = true);

    void Ping();
    void Close();

    void OnConnected(std::function<void()> callback);
    void OnDisconnected(std::function<void()> callback);
    void OnData(std::function<void(const char*, size_t, bool binary)> callback);
    void OnError(std::function<void(int)> callback);

private:
    Transport *transport_;
    std::thread io_thread_;
    std::atomic<bool> stop_io_task_{false};

    // --- 线程安全发送队列 ---
    std::queue<std::vector<uint8_t>> send_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cond_var_;
    // -----------------------

    bool continuation_ = false;
    size_t receive_buffer_size_ = 4096;

    std::map<std::string, std::string> headers_;
    std::function<void(const char*, size_t, bool binary)> on_data_;
    std::function<void(int)> on_error_;
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;

    void IoTask(); // 任务现在负责 I/O
    bool SendAllRaw(const void* data, size_t len);
    bool QueueControlFrame(uint8_t opcode, const void* data, size_t len);
    bool QueueDataFrame(const void* data, size_t len, bool binary, bool fin);

    // 内部帮助函数
    bool Handshake(const std::string& host, const std::string& port, const std::string& path);
};

#endif // WEBSOCKET_H