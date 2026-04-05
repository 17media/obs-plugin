#pragma once

#include <deque>
#include <mutex>
#include <string>

#include <QString>
#include <nlohmann/json.hpp>

#include "../websocket/WsMessage.hpp"

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
    std::deque<WsMessage> chatEventQueue_;
    size_t chatQueueMaxSize_{5000};
    std::string chatDockClientId_;
    mutable std::mutex chatQueueMutex_;
};

