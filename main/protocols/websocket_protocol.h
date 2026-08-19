#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_

#include "protocol.h"
#include "reconnect_controller.h"
#include "secure_websocket_client.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <web_socket.h>

#include <atomic>
#include <memory>
#include <mutex>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)
#define WEBSOCKET_PROTOCOL_CONNECTION_FAILED_EVENT (1 << 1)
#define WEBSOCKET_PROTOCOL_CONNECTION_TASK_DONE_EVENT (1 << 2)

class WebsocketProtocol : public Protocol {
public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;
    bool RequiresConversationSessionId() const override { return strict_session_validation_; }

private:
    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<WebSocket> websocket_;
    std::shared_ptr<SecureWebSocketClient> secure_websocket_;
    std::vector<std::shared_ptr<SecureWebSocketClient>> deferred_secure_shutdowns_;
    mutable std::mutex secure_transport_mutex_;
    std::mutex secure_connection_lifecycle_mutex_;
    ReconnectController reconnect_;
    int version_ = 1;
    std::atomic<bool> intentional_close_{false};
    std::atomic<bool> channel_connected_{false};
    std::atomic<bool> connection_task_active_{false};
    std::atomic<uint32_t> transport_generation_{0};
    bool strict_session_validation_ = false;
    bool synthesized_legacy_session_ = false;

    void ParseServerHello(const cJSON* root);
    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
    bool IsTransportConnected() const;
    bool SendTransport(const void* data, size_t length, bool binary);
    int TransportLastError() const;
    bool StartSecureConnection(const std::shared_ptr<SecureWebSocketClient>& transport,
                               uint32_t generation);
    bool StartSecureShutdown(std::shared_ptr<SecureWebSocketClient>& transport);
    void DeferSecureShutdown(std::shared_ptr<SecureWebSocketClient> transport);
    void RetryDeferredSecureShutdowns();
    bool IsSecureConnectionTaskActive();
    void FinishSecureConnection(const std::shared_ptr<SecureWebSocketClient>& transport,
                                uint32_t generation);
    std::shared_ptr<SecureWebSocketClient> GetSecureTransport() const;
    std::shared_ptr<SecureWebSocketClient> TakeSecureTransport();
    void ResetTransport();
};

#endif
