#include "WebsocketUtils.hpp"

#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveWebsocketServer.hpp"
#include "WsMessage.hpp"

void wsBroadcast(OneSevenLiveWebsocketServer* server, const WsMessage& msg) {
    if (!server || !server->is_running())
        return;
    server->broadcastMessage(msg.dump());
}

void wsBroadcast(const QString& type, const nlohmann::json& payload) {
    auto& core = OneSevenLiveCoreManager::getInstance();
    auto* ws = core.getWebsocketServer();
    if (!ws || !ws->is_running())
        return;
    wsBroadcast(ws, WsMessage{type.toStdString(), payload});
}

std::string generateWebSocketKey() {
    unsigned char randomBytes[16];
    for (int i = 0; i < 16; i++) {
        randomBytes[i] = static_cast<unsigned char>(rand() & 0xFF);
    }
    static const char* base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    for (int i = 0; i < 16; i += 3) {
        int val = (randomBytes[i] << 16) | ((i + 1 < 16 ? randomBytes[i + 1] : 0) << 8) |
                  (i + 2 < 16 ? randomBytes[i + 2] : 0);
        encoded.push_back(base64_chars[(val >> 18) & 0x3F]);
        encoded.push_back(base64_chars[(val >> 12) & 0x3F]);
        encoded.push_back(i + 1 < 16 ? base64_chars[(val >> 6) & 0x3F] : '=');
        encoded.push_back(i + 2 < 16 ? base64_chars[val & 0x3F] : '=');
    }
    return encoded;
}