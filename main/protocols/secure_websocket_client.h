#ifndef SECURE_WEBSOCKET_CLIENT_H
#define SECURE_WEBSOCKET_CLIENT_H

#include <esp_event.h>
#include <esp_websocket_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

class SecureWebSocketClient {
public:
    SecureWebSocketClient(std::string url, std::string ca_certificate_pem);
    ~SecureWebSocketClient();

    void SetHeader(const char* key, const char* value);
    void OnData(std::function<void(const char*, size_t, bool)> callback);
    void OnDisconnected(std::function<void()> callback);

    bool Connect();
    bool IsConnected() const;
    bool Send(const std::string& data);
    bool Send(const void* data, size_t length, bool binary);
    int GetLastError() const { return last_error_; }

private:
    static constexpr EventBits_t kConnectedBit = BIT0;
    static constexpr EventBits_t kFailedBit = BIT1;

    static void HandleEvent(void* argument, esp_event_base_t event_base, int32_t event_id,
                            void* event_data);
    void HandleEvent(int32_t event_id, const esp_websocket_event_data_t* event);
    void Shutdown();

    std::string url_;
    std::string ca_certificate_pem_;
    std::map<std::string, std::string> headers_;
    esp_websocket_client_handle_t client_ = nullptr;
    EventGroupHandle_t events_ = nullptr;
    std::function<void(const char*, size_t, bool)> on_data_;
    std::function<void()> on_disconnected_;
    std::vector<char> incoming_message_;
    bool incoming_binary_ = false;
    bool disconnected_notified_ = false;
    int last_error_ = 0;
};

#endif  // SECURE_WEBSOCKET_CLIENT_H
