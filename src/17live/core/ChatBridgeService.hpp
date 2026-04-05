#pragma once

#include <mutex>
#include <string>

#include <QString>
#include <nlohmann/json.hpp>

#include "../websocket/WsMessage.hpp"
#include "WsMessageQueue.hpp"

class OneSevenLiveCoreManager;

class ChatBridgeService {
public:
    explicit ChatBridgeService(OneSevenLiveCoreManager* coreManager);
    ~ChatBridgeService() = default;

    void onWebsocketMessage(const std::string& clientId, const std::string& message);
    void onWebsocketConnectionChanged(const std::string& clientId, bool connected);

    void enqueueOrBroadcastChatEvent(const QString& type, const nlohmann::json& payload);
    void flushChatEventQueue();
    void clear();

private:
    bool isChatDockClientConnectedLocked() const;

    OneSevenLiveCoreManager* coreManager_;
    WsMessageQueue chatEventQueue_{5000};
    std::string chatDockClientId_;
    mutable std::mutex chatQueueMutex_;
};
