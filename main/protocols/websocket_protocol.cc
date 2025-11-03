#include "websocket_protocol.h"
#include "board.h"
#include "system_info.h"
#include "application.h"
#include "settings.h"

#include <cstring>
#include <stdexcept>
#include <cJSON.h>
#include <esp_log.h>
#include <arpa/inet.h>
#include "assets/lang_config.h"
#include <mbedtls/md.h>
#include "ota.h"

#define TAG "WS"

#define HMAC_OUTPUT_LEN 32  // HMAC-SHA256 输出长度
#define MAC_ADDRESS_LEN 18  // MAC 地址长度

// 函数：将十六进制字符串转换为字节数组
std::vector<unsigned char> hexStringToByteArray(const std::string& hex_str) {
    // 每个字节由两个字符表示，因此需要的字节数是字符串长度的一半
    std::vector<unsigned char> byteArray;

    if (hex_str.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have an even length");
    }

    for (size_t i = 0; i < hex_str.length(); i += 2) {
        // 取出每两个字符
        std::string byteString = hex_str.substr(i, 2);
        // 将字符转换为十进制整数
        unsigned char byte = static_cast<unsigned char>(strtol(byteString.c_str(), nullptr, 16));
        byteArray.push_back(byte);
    }

    return byteArray;
}

// 函数：将字节数组转换为十六进制字符串
std::string bytesToHexString(const unsigned char* bytes, size_t length) {
    std::ostringstream oss;

    for (size_t i = 0; i < length; ++i) {
        // 每个字节以 2 位十六进制数格式输出
        oss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(bytes[i]);
    }

    return oss.str();  // 返回十六进制字符串
}

// 函数：生成签名
void generate_signature(const char *mac, const char *token, const unsigned char *device_key, unsigned char *signature) {
    char message[MAC_ADDRESS_LEN + 256]; // 存储 MAC + token
    snprintf(message, sizeof(message), "%s%s", mac, token); // 合并 MAC 和 token

    // 使用 HMAC-SHA256 生成签名
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);  // 1表示使用HMAC
    mbedtls_md_hmac_starts(&ctx, device_key, HMAC_OUTPUT_LEN);
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)message, strlen(message));
    mbedtls_md_hmac_finish(&ctx, signature);
    mbedtls_md_free(&ctx);
}

WebsocketProtocol::WebsocketProtocol() {
    event_group_handle_ = xEventGroupCreate();
}

WebsocketProtocol::~WebsocketProtocol() {
    if (websocket_ != nullptr) {
        delete websocket_;
    }
    vEventGroupDelete(event_group_handle_);
}

bool WebsocketProtocol::Start() {
    // Only connect to server when audio channel is needed
    return true;
}

bool WebsocketProtocol::SendAudio(const AudioStreamPacket& packet) {
    if (websocket_ == nullptr) {
        return false;
    }

    if (version_ == 2) {
        std::string serialized;
        serialized.resize(sizeof(BinaryProtocol2) + packet.payload.size());
        auto bp2 = (BinaryProtocol2*)serialized.data();
        bp2->version = htons(version_);
        bp2->type = 0;
        bp2->reserved = 0;
        bp2->timestamp = htonl(packet.timestamp);
        bp2->payload_size = htonl(packet.payload.size());
        memcpy(bp2->payload, packet.payload.data(), packet.payload.size());

        return websocket_->Send(serialized.data(), serialized.size(), true);
    } else if (version_ == 3) {
        std::string serialized;
        serialized.resize(sizeof(BinaryProtocol3) + packet.payload.size());
        auto bp3 = (BinaryProtocol3*)serialized.data();
        bp3->type = 0;
        bp3->reserved = 0;
        bp3->payload_size = htons(packet.payload.size());
        memcpy(bp3->payload, packet.payload.data(), packet.payload.size());

        return websocket_->Send(serialized.data(), serialized.size(), true);
    } else {
        return websocket_->Send(packet.payload.data(), packet.payload.size(), true);
    }
}

bool WebsocketProtocol::SendText(const std::string& text) {
    if (websocket_ == nullptr) {
        return false;
    }

    if (!websocket_->Send(text)) {
        ESP_LOGE(TAG, "Failed to send text: %s", text.c_str());
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }

    return true;
}

bool WebsocketProtocol::IsAudioChannelOpened() const {
    return websocket_ != nullptr && websocket_->IsConnected() && !error_occurred_ && !IsTimeout();
}

void WebsocketProtocol::CloseAudioChannel() {
    if (websocket_ != nullptr) {
        delete websocket_;
        websocket_ = nullptr;
    }
}

bool WebsocketProtocol::OpenAudioChannel() {
    if (websocket_ != nullptr) {
        delete websocket_;
    }

    // 获取设备唯一标识符
    std::string mac_address = SystemInfo::GetMacAddress();
    ESP_LOGW(TAG, "Device mac addr: %s\n", mac_address.c_str());

    // 生成签名
    Settings settings("websocket", true);
    std::string url = settings.GetString("url");
    ESP_LOGW(TAG, "websocket server url: %s", url.c_str());
    settings.SetString("url", url);
    std::string token = settings.GetString("token");
    // ESP_LOGW(TAG, "Serverside token: %s\n", token.c_str());
    // ESP_LOGW(TAG, "Const Device Key: %s\n", device_key.c_str());
    auto device_key_array = hexStringToByteArray(device_key);
    unsigned char signature[HMAC_OUTPUT_LEN] = { 0 }; // 存储签名
    generate_signature(mac_address.c_str(), token.c_str(), device_key_array.data(), signature);
    std::string signature_string =  bytesToHexString(signature, HMAC_OUTPUT_LEN);

    int version = settings.GetInt("version");
    if (version != 0) {
        version_ = version;
    }

    error_occurred_ = false;

    websocket_ = Board::GetInstance().CreateWebSocket();
    
    if (!signature_string.empty()) {
        // If token not has a space, add "Bearer " prefix
        if (signature_string.find(" ") == std::string::npos) {
            signature_string = "Bearer " + signature_string;
        }
        // ESP_LOGI(TAG, "signature_string %s", signature_string.c_str());
        websocket_->SetHeader("Authorization", signature_string.c_str());
    }
    websocket_->SetHeader("Protocol-Version", std::to_string(version_).c_str());
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    websocket_->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());

    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            if (on_incoming_audio_ != nullptr) {
                if (version_ == 2) {
                    BinaryProtocol2* bp2 = (BinaryProtocol2*)data;
                    bp2->version = ntohs(bp2->version);
                    bp2->type = ntohs(bp2->type);
                    bp2->timestamp = ntohl(bp2->timestamp);
                    bp2->payload_size = ntohl(bp2->payload_size);
                    auto payload = (uint8_t*)bp2->payload;
                    on_incoming_audio_(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = bp2->timestamp,
                        .payload = std::vector<uint8_t>(payload, payload + bp2->payload_size)
                    });
                } else if (version_ == 3) {
                    BinaryProtocol3* bp3 = (BinaryProtocol3*)data;
                    bp3->type = bp3->type;
                    bp3->payload_size = ntohs(bp3->payload_size);
                    auto payload = (uint8_t*)bp3->payload;
                    on_incoming_audio_(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>(payload, payload + bp3->payload_size)
                    });
                } else {
                    on_incoming_audio_(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len)
                    });
                }
            }
        } else {
            // Parse JSON data
            auto root = cJSON_Parse(data);
            auto type = cJSON_GetObjectItem(root, "type");
            if (cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "hello") == 0) {
                    ParseServerHello(root);
                } else {
                    if (on_incoming_json_ != nullptr) {
                        on_incoming_json_(root);
                    }
                }
            } else {
                ESP_LOGE(TAG, "Missing message type, data: %s", data);
            }
            cJSON_Delete(root);
        }
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    websocket_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Websocket disconnected");
        if (on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();
        }
    });

    ESP_LOGI(TAG, "Connecting to websocket server: %s with version: %d", url.c_str(), version_);
    if (!websocket_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect to websocket server");
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    // Send hello message to describe the client
    auto message = GetHelloMessage();
    if (!SendText(message)) {
        return false;
    }

    // Wait for server hello
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }

    return true;
}

std::string WebsocketProtocol::GetHelloMessage() {
    // keys: message type, version, audio_params (format, sample_rate, channels)
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", version_);
    cJSON* features = cJSON_CreateObject();
#if CONFIG_USE_SERVER_AEC
    cJSON_AddBoolToObject(features, "aec", true);
#endif
#if CONFIG_IOT_PROTOCOL_MCP
    cJSON_AddBoolToObject(features, "mcp", true);
#endif
    cJSON_AddItemToObject(root, "features", features);
    cJSON_AddStringToObject(root, "transport", "websocket");
    cJSON* audio_params = cJSON_CreateObject();
    cJSON_AddStringToObject(audio_params, "format", "opus");
    cJSON_AddNumberToObject(audio_params, "sample_rate", 16000);
    cJSON_AddNumberToObject(audio_params, "channels", 1);
    cJSON_AddNumberToObject(audio_params, "frame_duration", OPUS_FRAME_DURATION_MS);
    cJSON_AddItemToObject(root, "audio_params", audio_params);
    auto json_str = cJSON_PrintUnformatted(root);
    std::string message(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return message;
}

void WebsocketProtocol::ParseServerHello(const cJSON* root) {
    auto transport = cJSON_GetObjectItem(root, "transport");
    if (transport == nullptr || strcmp(transport->valuestring, "websocket") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport->valuestring);
        return;
    }

    auto session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id)) {
        session_id_ = session_id->valuestring;
        ESP_LOGI(TAG, "Session ID: %s", session_id_.c_str());
    }

    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (cJSON_IsObject(audio_params)) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (cJSON_IsNumber(sample_rate)) {
            server_sample_rate_ = sample_rate->valueint;
        }
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (cJSON_IsNumber(frame_duration)) {
            server_frame_duration_ = frame_duration->valueint;
        }
    }

    xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT);
}
