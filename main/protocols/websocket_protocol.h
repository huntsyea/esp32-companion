#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_

#include "protocol.h"
#include "reconnect_controller.h"
#include "secure_websocket_client.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <web_socket.h>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

class WebsocketProtocol : public Protocol {
public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

private:
    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<WebSocket> websocket_;
    std::unique_ptr<SecureWebSocketClient> secure_websocket_;
    ReconnectController reconnect_;
    int version_ = 1;
    bool intentional_close_ = false;
    bool channel_connected_ = false;

    void ParseServerHello(const cJSON* root);
    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
    bool IsTransportConnected() const;
    bool SendTransport(const void* data, size_t length, bool binary);
    int TransportLastError() const;
    void ResetTransport();
};

#endif
