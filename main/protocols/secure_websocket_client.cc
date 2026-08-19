#include "secure_websocket_client.h"

#include "incoming_frame_limits.h"

#include <esp_log.h>

#include <atomic>
#include <limits>
#include <utility>

#define TAG "SecureWS"

namespace {
// The ESP WebSocket client owns the network task. Callers only attempt an
// immediate enqueue/write and never wait on network backpressure.
constexpr TickType_t kSendTimeout = 0;
}  // namespace

SecureWebSocketClient::SecureWebSocketClient(std::string url, std::string ca_certificate_pem)
    : url_(std::move(url)), ca_certificate_pem_(std::move(ca_certificate_pem)) {
    events_ = xEventGroupCreate();
}

SecureWebSocketClient::~SecureWebSocketClient() {
    Shutdown();
    if (events_ != nullptr) {
        vEventGroupDelete(events_);
    }
}

void SecureWebSocketClient::SetHeader(const char* key, const char* value) {
    if (key != nullptr && value != nullptr) {
        headers_[key] = value;
    }
}

void SecureWebSocketClient::OnData(std::function<void(const char*, size_t, bool)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_data_ = std::move(callback);
}

void SecureWebSocketClient::OnDisconnected(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_disconnected_ = std::move(callback);
}

void SecureWebSocketClient::DetachCallbacks() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_data_ = nullptr;
    on_disconnected_ = nullptr;
}

bool SecureWebSocketClient::Connect() {
    if (cancelled_.load() || events_ == nullptr || url_.rfind("wss://", 0) != 0 ||
        ca_certificate_pem_.empty()) {
        last_error_ = ESP_ERR_INVALID_ARG;
        return false;
    }

    std::unique_lock<std::mutex> lifecycle_lock(client_mutex_);
    if (cancelled_.load()) {
        last_error_ = ESP_ERR_INVALID_STATE;
        return false;
    }

    esp_websocket_client_config_t config = {};
    config.uri = url_.c_str();
    config.cert_pem = ca_certificate_pem_.c_str();
    config.cert_len = ca_certificate_pem_.size() + 1;
    config.disable_auto_reconnect = true;
    config.network_timeout_ms = 10000;
    config.buffer_size = 4096;
    client_ = esp_websocket_client_init(&config);
    if (client_ == nullptr) {
        last_error_ = ESP_ERR_NO_MEM;
        return false;
    }

    for (const auto& [key, value] : headers_) {
        if (esp_websocket_client_append_header(client_, key.c_str(), value.c_str()) != ESP_OK) {
            last_error_ = ESP_ERR_INVALID_ARG;
            lifecycle_lock.unlock();
            Shutdown();
            return false;
        }
    }
    const esp_err_t register_result =
        esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, HandleEvent, this);
    if (register_result != ESP_OK) {
        last_error_ = register_result;
        lifecycle_lock.unlock();
        Shutdown();
        return false;
    }
    xEventGroupClearBits(events_, kConnectedBit | kFailedBit);
    if (cancelled_.load()) {
        last_error_ = ESP_ERR_INVALID_STATE;
        lifecycle_lock.unlock();
        Shutdown();
        return false;
    }
    const esp_err_t start_result = esp_websocket_client_start(client_);
    if (start_result != ESP_OK) {
        last_error_ = start_result;
        lifecycle_lock.unlock();
        Shutdown();
        return false;
    }
    lifecycle_lock.unlock();

    const EventBits_t result = xEventGroupWaitBits(events_, kConnectedBit | kFailedBit, pdTRUE,
                                                   pdFALSE, pdMS_TO_TICKS(10000));
    if (cancelled_.load() || (result & kConnectedBit) == 0) {
        if (last_error_ == 0) {
            last_error_ = cancelled_.load() ? ESP_ERR_INVALID_STATE : ESP_ERR_TIMEOUT;
        }
        Shutdown();
        return false;
    }
    return true;
}

void SecureWebSocketClient::Cancel() {
    cancelled_.store(true);
    if (events_ != nullptr) {
        xEventGroupSetBits(events_, kFailedBit);
    }
}

bool SecureWebSocketClient::IsConnected() const {
    if (cancelled_.load()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(client_mutex_);
    return !cancelled_.load() && client_ != nullptr && esp_websocket_client_is_connected(client_);
}

bool SecureWebSocketClient::Send(const std::string& data) {
    return Send(data.data(), data.size(), false);
}

bool SecureWebSocketClient::Send(const void* data, size_t length, bool binary) {
    if (cancelled_.load() || data == nullptr || length == 0 ||
        length > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    std::lock_guard<std::mutex> lock(client_mutex_);
    if (cancelled_.load() || client_ == nullptr || !esp_websocket_client_is_connected(client_)) {
        return false;
    }
    const int sent = binary
                         ? esp_websocket_client_send_bin(client_, static_cast<const char*>(data),
                                                         static_cast<int>(length), kSendTimeout)
                         : esp_websocket_client_send_text(client_, static_cast<const char*>(data),
                                                          static_cast<int>(length), kSendTimeout);
    return sent == static_cast<int>(length);
}

void SecureWebSocketClient::HandleEvent(void* argument, esp_event_base_t event_base,
                                        int32_t event_id, void* event_data) {
    (void)event_base;
    auto* client = static_cast<SecureWebSocketClient*>(argument);
    client->HandleEvent(event_id, static_cast<esp_websocket_event_data_t*>(event_data));
}

void SecureWebSocketClient::HandleEvent(int32_t event_id, const esp_websocket_event_data_t* event) {
    if (event_id == WEBSOCKET_EVENT_CONNECTED) {
        if (cancelled_.load()) {
            xEventGroupSetBits(events_, kFailedBit);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            disconnected_notified_ = false;
        }
        xEventGroupSetBits(events_, kConnectedBit);
        return;
    }
    if (event_id == WEBSOCKET_EVENT_ERROR) {
        last_error_ = event == nullptr ? ESP_FAIL : event->error_handle.esp_tls_last_esp_err;
        if (last_error_ == ESP_OK) {
            last_error_ = ESP_FAIL;
        }
        xEventGroupSetBits(events_, kFailedBit);
        return;
    }
    if (event_id == WEBSOCKET_EVENT_DISCONNECTED || event_id == WEBSOCKET_EVENT_CLOSED) {
        xEventGroupSetBits(events_, kFailedBit);
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (!disconnected_notified_ && on_disconnected_) {
            disconnected_notified_ = true;
            on_disconnected_();
        }
        return;
    }
    if (cancelled_.load() || event_id != WEBSOCKET_EVENT_DATA || event == nullptr ||
        event->data_len < 0 || event->payload_offset < 0 || event->payload_len < 0 ||
        (event->data_len > 0 && event->data_ptr == nullptr)) {
        return;
    }

    const size_t data_length = static_cast<size_t>(event->data_len);
    const size_t payload_offset = static_cast<size_t>(event->payload_offset);
    const size_t payload_length = static_cast<size_t>(event->payload_len);
    if (!IsValidIncomingPayload(payload_length, kMaxIncomingWebSocketMessageBytes) ||
        payload_offset > payload_length || data_length > payload_length - payload_offset ||
        (payload_offset != 0 && payload_offset != incoming_message_.size())) {
        incoming_message_.clear();
        ESP_LOGW(TAG, "Rejected invalid fragmented WebSocket message");
        return;
    }

    if (event->payload_offset == 0) {
        incoming_message_.clear();
        incoming_binary_ = event->op_code == 0x2;
        incoming_message_.reserve(payload_length);
    }
    incoming_message_.insert(incoming_message_.end(), event->data_ptr,
                             event->data_ptr + event->data_len);
    if (event->fin) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (incoming_message_.size() == payload_length && on_data_) {
            on_data_(incoming_message_.data(), incoming_message_.size(), incoming_binary_);
        }
        incoming_message_.clear();
    }
}

void SecureWebSocketClient::Shutdown() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    if (client_ == nullptr) {
        return;
    }
    if (esp_websocket_client_is_connected(client_)) {
        esp_websocket_client_close(client_, pdMS_TO_TICKS(1000));
    }
    esp_websocket_client_stop(client_);
    esp_websocket_unregister_events(client_, WEBSOCKET_EVENT_ANY, HandleEvent);
    esp_websocket_client_destroy(client_);
    client_ = nullptr;
}
