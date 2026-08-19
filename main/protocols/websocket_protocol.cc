#include "websocket_protocol.h"
#include "application.h"
#include "board.h"
#include "companion_connection.h"
#include "incoming_frame_limits.h"
#include "settings.h"
#include "system_info.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <arpa/inet.h>
#include <cJSON.h>
#include <mdns.h>
#include <cstring>
#include "assets/lang_config.h"

#define TAG "WS"

namespace {

bool ProbeMdnsHost(const std::string& configured_host) {
    std::string host = configured_host;
    constexpr const char* kLocalSuffix = ".local";
    if (host.size() > strlen(kLocalSuffix) &&
        host.compare(host.size() - strlen(kLocalSuffix), strlen(kLocalSuffix), kLocalSuffix) == 0) {
        host.resize(host.size() - strlen(kLocalSuffix));
    }
    if (host.empty()) {
        return false;
    }
    const esp_err_t init_result = mdns_init();
    if (init_result != ESP_OK && init_result != ESP_ERR_INVALID_STATE) {
        return false;
    }
    esp_ip4_addr_t address = {};
    return mdns_query_a(host.c_str(), 1500, &address) == ESP_OK;
}

CompanionConnectionConfig LoadCompanionConnectionConfig(const std::string& legacy_url) {
    Settings settings("companion", false);
    CompanionConnectionConfig config;
    const std::string mode = settings.GetString("mode");
    if (mode == "physical") {
        config.mode = CompanionConnectionMode::kPhysical;
    } else if (mode == "loopback") {
        config.mode = CompanionConnectionMode::kLoopbackDevelopment;
    }
    config.mdns_host = settings.GetString("mdns_host");
    const int32_t port = settings.GetInt("port", 8765);
    if (port > 0 && port <= UINT16_MAX) {
        config.port = static_cast<uint16_t>(port);
    }
    config.path = settings.GetString("path", "/xiaozhi/v1");
    config.fallback_url = settings.GetString("fallback_url", legacy_url);
    config.ca_certificate_pem = settings.GetString("ca_pem");
    return config;
}

}  // namespace

WebsocketProtocol::WebsocketProtocol()
    : reconnect_([]() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); },
                 []() { return esp_random(); }) {
    event_group_handle_ = xEventGroupCreate();
}

WebsocketProtocol::~WebsocketProtocol() {
    intentional_close_.store(true);
    std::shared_ptr<SecureWebSocketClient> secure;
    {
        std::lock_guard<std::mutex> lifecycle_lock(secure_connection_lifecycle_mutex_);
        if (websocket_ != nullptr || GetSecureTransport() != nullptr ||
            connection_task_active_.load()) {
            ++transport_generation_;
        }
        xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_CONNECTION_FAILED_EVENT);
        secure = TakeSecureTransport();
        if (secure) {
            secure->DetachCallbacks();
            secure->Cancel();
        }
        websocket_.reset();
    }
    if (connection_task_active_.load()) {
        xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_CONNECTION_TASK_DONE_EVENT,
                            pdTRUE, pdFALSE, portMAX_DELAY);
        connection_task_active_.store(false);
    }
    if (secure && !StartSecureShutdown(secure)) {
        DeferSecureShutdown(std::move(secure));
    }
    RetryDeferredSecureShutdowns();
    vEventGroupDelete(event_group_handle_);
}

bool WebsocketProtocol::Start() {
    // Only connect to server when audio channel is needed
    return true;
}

bool WebsocketProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!IsTransportConnected()) {
        return false;
    }

    if (version_ == 2) {
        std::string serialized;
        serialized.resize(sizeof(BinaryProtocol2) + packet->payload.size());
        auto bp2 = (BinaryProtocol2*)serialized.data();
        bp2->version = htons(version_);
        bp2->type = 0;
        bp2->reserved = 0;
        bp2->timestamp = htonl(packet->timestamp);
        bp2->payload_size = htonl(packet->payload.size());
        memcpy(bp2->payload, packet->payload.data(), packet->payload.size());

        return SendTransport(serialized.data(), serialized.size(), true);
    } else if (version_ == 3) {
        std::string serialized;
        serialized.resize(sizeof(BinaryProtocol3) + packet->payload.size());
        auto bp3 = (BinaryProtocol3*)serialized.data();
        bp3->type = 0;
        bp3->reserved = 0;
        bp3->payload_size = htons(packet->payload.size());
        memcpy(bp3->payload, packet->payload.data(), packet->payload.size());

        return SendTransport(serialized.data(), serialized.size(), true);
    } else {
        return SendTransport(packet->payload.data(), packet->payload.size(), true);
    }
}

bool WebsocketProtocol::SendText(const std::string& text) {
    if (!IsTransportConnected()) {
        return false;
    }

    std::string legacy_wire_text;
    const std::string* wire_text = &text;
    if (synthesized_legacy_session_) {
        const std::string local_session = "\"session_id\":\"" + session_id_ + "\"";
        const auto position = text.find(local_session);
        if (position != std::string::npos) {
            legacy_wire_text = text;
            legacy_wire_text.replace(position, local_session.size(), "\"session_id\":\"\"");
            wire_text = &legacy_wire_text;
        }
    }
    if (!SendTransport(wire_text->data(), wire_text->size(), false)) {
        ESP_LOGE(TAG, "Failed to send WebSocket text frame");
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }

    return true;
}

bool WebsocketProtocol::IsAudioChannelOpened() const {
    return channel_connected_.load() && IsTransportConnected() && !error_occurred_ && !IsTimeout();
}

void WebsocketProtocol::CloseAudioChannel(bool send_goodbye) {
    (void)send_goodbye;  // Websocket doesn't need to send goodbye message
    intentional_close_.store(true);
    bool was_connected = false;
    {
        std::lock_guard<std::mutex> lifecycle_lock(secure_connection_lifecycle_mutex_);
        const bool had_transport = websocket_ != nullptr || GetSecureTransport() != nullptr ||
                                   IsSecureConnectionTaskActive();
        if (had_transport) {
            ++transport_generation_;
        }
        xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_CONNECTION_FAILED_EVENT);
        auto secure = TakeSecureTransport();
        if (secure) {
            secure->DetachCallbacks();
            secure->Cancel();
            if (!StartSecureShutdown(secure)) {
                ESP_LOGE(TAG, "Failed to start secure WebSocket shutdown task");
                DeferSecureShutdown(std::move(secure));
            }
        }
        was_connected = channel_connected_.exchange(false);
        session_id_.clear();
        synthesized_legacy_session_ = false;
        ResetTransport();
    }
    intentional_close_.store(false);
    if (was_connected && on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
}

bool WebsocketProtocol::OpenAudioChannel() {
    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");
    int version = settings.GetInt("version");
    if (version != 0) {
        version_ = version;
    }

    if (!reconnect_.Ready()) {
        ESP_LOGW(TAG, "Connection attempt deferred by reconnect policy");
        return false;
    }
    RetryDeferredSecureShutdowns();
    if (IsSecureConnectionTaskActive()) {
        ESP_LOGW(TAG, "A WebSocket connection attempt is already active");
        return false;
    }

    error_occurred_ = false;
    session_id_.clear();
    synthesized_legacy_session_ = false;
    channel_connected_.store(false);
    xEventGroupClearBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT |
                                                  WEBSOCKET_PROTOCOL_CONNECTION_FAILED_EVENT);

    const CompanionConnectionConfig companion = LoadCompanionConnectionConfig(url);
    const bool companion_mode = companion.mode != CompanionConnectionMode::kLegacy;
    strict_session_validation_ = companion_mode;
    bool use_secure_transport = false;
    if (companion_mode) {
        const CompanionEndpoint endpoint = ResolveCompanionEndpoint(companion, ProbeMdnsHost);
        if (!endpoint.ok()) {
            ESP_LOGE(TAG, "Companion connection policy rejected endpoint: %s",
                     endpoint.error.c_str());
            reconnect_.RecordFailure();
            SetError(Lang::Strings::SERVER_NOT_CONNECTED);
            return false;
        }
        url = endpoint.url;
        use_secure_transport = companion.mode == CompanionConnectionMode::kPhysical;
    }

    if (use_secure_transport) {
        std::lock_guard<std::mutex> lock(secure_transport_mutex_);
        secure_websocket_ =
            std::make_shared<SecureWebSocketClient>(url, companion.ca_certificate_pem);
    } else {
        auto network = Board::GetInstance().GetNetwork();
        websocket_ = network->CreateWebSocket(1);
        if (websocket_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create websocket");
            reconnect_.RecordFailure();
            return false;
        }
    }
    const uint32_t transport_generation = ++transport_generation_;

    if (!token.empty()) {
        // If token not has a space, add "Bearer " prefix
        if (token.find(" ") == std::string::npos) {
            token = "Bearer " + token;
        }
        if (auto secure = GetSecureTransport()) {
            secure->SetHeader("Authorization", token.c_str());
        } else {
            websocket_->SetHeader("Authorization", token.c_str());
        }
    }
    const std::string protocol_version = std::to_string(version_);
    const std::string device_id = SystemInfo::GetMacAddress();
    const std::string client_id = Board::GetInstance().GetUuid();
    if (auto secure = GetSecureTransport()) {
        secure->SetHeader("Protocol-Version", protocol_version.c_str());
        secure->SetHeader("Device-Id", device_id.c_str());
        secure->SetHeader("Client-Id", client_id.c_str());
    } else {
        websocket_->SetHeader("Protocol-Version", protocol_version.c_str());
        websocket_->SetHeader("Device-Id", device_id.c_str());
        websocket_->SetHeader("Client-Id", client_id.c_str());
    }

    auto on_data = [this, transport_generation](const char* data, size_t len, bool binary) {
        if (transport_generation != transport_generation_.load()) {
            return;
        }
        if (binary) {
            if (on_incoming_audio_ != nullptr) {
                if (version_ == 2) {
                    if (len < sizeof(BinaryProtocol2)) {
                        return;
                    }
                    const auto* bp2 = reinterpret_cast<const BinaryProtocol2*>(data);
                    const uint32_t payload_size = ntohl(bp2->payload_size);
                    if (ntohs(bp2->version) != 2 || bp2->type != 0 ||
                        !IsValidIncomingFramedPayload(len, sizeof(BinaryProtocol2), payload_size,
                                                      kMaxIncomingAudioFrameBytes)) {
                        ESP_LOGW(TAG, "Rejected invalid WebSocket v2 audio frame");
                        return;
                    }
                    const auto* payload = bp2->payload;
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = ntohl(bp2->timestamp),
                        .session_id = session_id_,
                        .payload = std::vector<uint8_t>(payload, payload + payload_size)}));
                } else if (version_ == 3) {
                    if (len < sizeof(BinaryProtocol3)) {
                        return;
                    }
                    const auto* bp3 = reinterpret_cast<const BinaryProtocol3*>(data);
                    const uint16_t payload_size = ntohs(bp3->payload_size);
                    if (bp3->type != 0 ||
                        !IsValidIncomingFramedPayload(len, sizeof(BinaryProtocol3), payload_size,
                                                      kMaxIncomingAudioFrameBytes)) {
                        ESP_LOGW(TAG, "Rejected invalid WebSocket v3 audio frame");
                        return;
                    }
                    const auto* payload = bp3->payload;
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .session_id = session_id_,
                        .payload = std::vector<uint8_t>(payload, payload + payload_size)}));
                } else {
                    if (!IsValidIncomingPayload(len, kMaxIncomingAudioFrameBytes)) {
                        ESP_LOGW(TAG, "Rejected invalid WebSocket audio frame");
                        return;
                    }
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .session_id = session_id_,
                        .payload =
                            std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                                 reinterpret_cast<const uint8_t*>(data) + len)}));
                }
            }
        } else {
            if (!IsValidIncomingPayload(len, kMaxIncomingJsonBytes)) {
                ESP_LOGW(TAG, "Rejected invalid WebSocket JSON frame size");
                return;
            }
            auto root = cJSON_ParseWithLength(data, len);
            if (root == nullptr) {
                ESP_LOGE(TAG, "Malformed WebSocket JSON frame");
                return;
            }
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
                ESP_LOGE(TAG, "WebSocket JSON frame is missing its type");
            }
            cJSON_Delete(root);
        }
        last_incoming_time_ = std::chrono::steady_clock::now();
    };

    auto on_disconnected = [this, transport_generation]() {
        if (transport_generation != transport_generation_.load()) {
            return;
        }
        ESP_LOGI(TAG, "Websocket disconnected");
        xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_CONNECTION_FAILED_EVENT);
        session_id_.clear();
        const bool was_connected = channel_connected_.exchange(false);
        if (was_connected && !intentional_close_.load()) {
            reconnect_.RecordFailure();
        }
        if (on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();
        }
        if (!intentional_close_.load() && on_disconnected_ != nullptr) {
            on_disconnected_();
        }
    };

    if (auto secure = GetSecureTransport()) {
        secure->OnData(on_data);
        secure->OnDisconnected(on_disconnected);
    } else {
        websocket_->OnData(on_data);
        websocket_->OnDisconnected(on_disconnected);
    }

    ESP_LOGI(TAG, "Connecting with %s WebSocket transport, protocol version %d",
             use_secure_transport ? "verified TLS" : "configured", version_);
    if (auto secure = GetSecureTransport()) {
        if (!StartSecureConnection(secure, transport_generation)) {
            ESP_LOGE(TAG, "Failed to start secure WebSocket connection task");
            reconnect_.RecordFailure();
            ResetTransport();
            SetError(Lang::Strings::SERVER_NOT_CONNECTED);
            return false;
        }
        return true;
    }

    const bool connected = websocket_->Connect(url.c_str());
    if (!connected) {
        ESP_LOGE(TAG, "Failed to connect to websocket server, code=%d", TransportLastError());
        reconnect_.RecordFailure();
        ResetTransport();
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    // Send hello message to describe the client
    auto message = GetHelloMessage();
    if (!SendText(message)) {
        reconnect_.RecordFailure();
        ResetTransport();
        return false;
    }

    // Wait for server hello
    EventBits_t bits =
        xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE,
                            pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        reconnect_.RecordFailure();
        ResetTransport();
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    channel_connected_.store(true);
    reconnect_.RecordSuccess();
    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }
    if (on_connected_ != nullptr) {
        on_connected_();
    }

    return true;
}

bool WebsocketProtocol::StartSecureConnection(
    const std::shared_ptr<SecureWebSocketClient>& transport, uint32_t generation) {
    struct ConnectionTaskContext {
        WebsocketProtocol* protocol;
        std::shared_ptr<SecureWebSocketClient> transport;
        uint32_t generation;
    };

    auto context =
        std::make_unique<ConnectionTaskContext>(ConnectionTaskContext{this, transport, generation});
    xEventGroupClearBits(event_group_handle_, WEBSOCKET_PROTOCOL_CONNECTION_TASK_DONE_EVENT);
    connection_task_active_.store(true);
    const BaseType_t created = xTaskCreate(
        [](void* argument) {
            std::unique_ptr<ConnectionTaskContext> context(
                static_cast<ConnectionTaskContext*>(argument));
            auto* protocol = context->protocol;
            protocol->FinishSecureConnection(context->transport, context->generation);
            // Free the task context explicitly. FreeRTOS task deletion does not
            // unwind the C++ stack or run automatic destructors.
            context.reset();
            xEventGroupSetBits(protocol->event_group_handle_,
                               WEBSOCKET_PROTOCOL_CONNECTION_TASK_DONE_EVENT);
            vTaskDelete(nullptr);
        },
        "secure_ws_connect", 6144, context.get(), 4, nullptr);
    if (created != pdPASS) {
        connection_task_active_.store(false);
        return false;
    }
    context.release();
    return true;
}

bool WebsocketProtocol::StartSecureShutdown(std::shared_ptr<SecureWebSocketClient>& transport) {
    auto context = std::make_unique<std::shared_ptr<SecureWebSocketClient>>(std::move(transport));
    const BaseType_t created = xTaskCreate(
        [](void* argument) {
            std::unique_ptr<std::shared_ptr<SecureWebSocketClient>> context(
                static_cast<std::shared_ptr<SecureWebSocketClient>*>(argument));
            (*context)->Cancel();
            context.reset();
            vTaskDelete(nullptr);
        },
        "secure_ws_close", 4096, context.get(), 3, nullptr);
    if (created != pdPASS) {
        transport = std::move(*context);
        return false;
    }
    context.release();
    return true;
}

void WebsocketProtocol::DeferSecureShutdown(std::shared_ptr<SecureWebSocketClient> transport) {
    std::lock_guard<std::mutex> lock(secure_transport_mutex_);
    deferred_secure_shutdowns_.push_back(std::move(transport));
}

void WebsocketProtocol::RetryDeferredSecureShutdowns() {
    std::vector<std::shared_ptr<SecureWebSocketClient>> deferred;
    {
        std::lock_guard<std::mutex> lock(secure_transport_mutex_);
        deferred.swap(deferred_secure_shutdowns_);
    }
    for (auto& transport : deferred) {
        if (!StartSecureShutdown(transport)) {
            DeferSecureShutdown(std::move(transport));
        }
    }
}

bool WebsocketProtocol::IsSecureConnectionTaskActive() {
    if (!connection_task_active_.load()) {
        return false;
    }
    if (xEventGroupGetBits(event_group_handle_) & WEBSOCKET_PROTOCOL_CONNECTION_TASK_DONE_EVENT) {
        xEventGroupClearBits(event_group_handle_, WEBSOCKET_PROTOCOL_CONNECTION_TASK_DONE_EVENT);
        connection_task_active_.store(false);
        return false;
    }
    return true;
}

void WebsocketProtocol::FinishSecureConnection(
    const std::shared_ptr<SecureWebSocketClient>& transport, uint32_t generation) {
    if (!transport->Connect()) {
        std::lock_guard<std::mutex> lifecycle_lock(secure_connection_lifecycle_mutex_);
        if (generation == transport_generation_.load()) {
            ESP_LOGE(TAG, "Failed to connect to secure websocket server, code=%d",
                     transport->GetLastError());
            reconnect_.RecordFailure();
            ResetTransport();
            SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        }
        return;
    }
    {
        std::lock_guard<std::mutex> lifecycle_lock(secure_connection_lifecycle_mutex_);
        if (generation != transport_generation_.load()) {
            return;
        }

        const auto message = GetHelloMessage();
        if (!transport->Send(message)) {
            reconnect_.RecordFailure();
            ResetTransport();
            SetError(Lang::Strings::SERVER_ERROR);
            return;
        }
    }

    const EventBits_t bits = xEventGroupWaitBits(
        event_group_handle_,
        WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT | WEBSOCKET_PROTOCOL_CONNECTION_FAILED_EVENT, pdTRUE,
        pdFALSE, pdMS_TO_TICKS(10000));
    {
        std::lock_guard<std::mutex> lifecycle_lock(secure_connection_lifecycle_mutex_);
        if (generation != transport_generation_.load()) {
            return;
        }
        if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT) ||
            (bits & WEBSOCKET_PROTOCOL_CONNECTION_FAILED_EVENT)) {
            ESP_LOGE(TAG, "Failed to receive server hello");
            reconnect_.RecordFailure();
            ResetTransport();
            SetError(Lang::Strings::SERVER_TIMEOUT);
            return;
        }

        channel_connected_.store(true);
        reconnect_.RecordSuccess();
        if (on_audio_channel_opened_ != nullptr) {
            on_audio_channel_opened_();
        }
        if (on_connected_ != nullptr) {
            on_connected_();
        }
    }
}

bool WebsocketProtocol::IsTransportConnected() const {
    if (auto secure = GetSecureTransport()) {
        return secure->IsConnected();
    }
    return websocket_ != nullptr && websocket_->IsConnected();
}

bool WebsocketProtocol::SendTransport(const void* data, size_t length, bool binary) {
    if (auto secure = GetSecureTransport()) {
        return secure->Send(data, length, binary);
    }
    return websocket_ != nullptr && websocket_->Send(data, length, binary);
}

int WebsocketProtocol::TransportLastError() const {
    if (auto secure = GetSecureTransport()) {
        return secure->GetLastError();
    }
    return websocket_ == nullptr ? ESP_FAIL : websocket_->GetLastError();
}

std::shared_ptr<SecureWebSocketClient> WebsocketProtocol::GetSecureTransport() const {
    std::lock_guard<std::mutex> lock(secure_transport_mutex_);
    return secure_websocket_;
}

std::shared_ptr<SecureWebSocketClient> WebsocketProtocol::TakeSecureTransport() {
    std::lock_guard<std::mutex> lock(secure_transport_mutex_);
    return std::move(secure_websocket_);
}

void WebsocketProtocol::ResetTransport() {
    {
        std::lock_guard<std::mutex> lock(secure_transport_mutex_);
        secure_websocket_.reset();
    }
    websocket_.reset();
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
    cJSON_AddBoolToObject(features, "mcp", true);
    cJSON_AddItemToObject(root, "features", features);
    AddTextFontCapabilities(root);
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
    if (!cJSON_IsString(transport) || strcmp(transport->valuestring, "websocket") != 0) {
        ESP_LOGE(TAG, "Server hello has an unsupported transport");
        return;
    }

    auto session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id) && session_id->valuestring[0] != '\0') {
        session_id_ = session_id->valuestring;
        synthesized_legacy_session_ = false;
    } else if (strict_session_validation_) {
        ESP_LOGE(TAG, "Server hello is missing its session ID");
        return;
    } else {
        // Legacy servers historically omitted session_id. Use a connection-local
        // identifier so the Application can still reject packets from an older
        // transport without changing the wire contract.
        session_id_ = "legacy-" + std::to_string(transport_generation_.load());
        synthesized_legacy_session_ = true;
    }

    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (strict_session_validation_) {
        if (!cJSON_IsObject(audio_params)) {
            ESP_LOGE(TAG, "Server hello is missing audio parameters");
            return;
        }
        auto format = cJSON_GetObjectItem(audio_params, "format");
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        auto channels = cJSON_GetObjectItem(audio_params, "channels");
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (!cJSON_IsString(format) || strcmp(format->valuestring, "opus") != 0 ||
            !cJSON_IsNumber(sample_rate) || sample_rate->valueint <= 0 ||
            !cJSON_IsNumber(channels) || channels->valueint != 1 ||
            !cJSON_IsNumber(frame_duration) || frame_duration->valueint <= 0) {
            ESP_LOGE(TAG, "Server hello has unsupported audio parameters");
            return;
        }
        server_sample_rate_ = sample_rate->valueint;
        server_frame_duration_ = frame_duration->valueint;
    } else if (cJSON_IsObject(audio_params)) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (cJSON_IsNumber(sample_rate) && sample_rate->valueint > 0) {
            server_sample_rate_ = sample_rate->valueint;
        }
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (cJSON_IsNumber(frame_duration) && frame_duration->valueint > 0) {
            server_frame_duration_ = frame_duration->valueint;
        }
    }

    ESP_LOGI(TAG, "Server hello negotiated a %s session",
             strict_session_validation_ ? "strict" : "legacy-compatible");
    xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT);
}
