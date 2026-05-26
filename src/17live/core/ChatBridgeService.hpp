#pragma once

#include <mutex>
#include <string>
#include <unordered_set>

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
    bool hasAnyConnectedTargetLocked(const std::unordered_set<std::string>& targets) const;
    void sendToTargetsLocked(const WsMessage& m, const std::unordered_set<std::string>& targets);
    const std::unordered_set<std::string>& resolveTargetsLocked(const WsMessage& m) const;

    OneSevenLiveCoreManager* coreManager_;
    WsMessageQueue chatEventQueue_{5000};
    std::unordered_set<std::string> chatDockClientIds_;
    std::unordered_set<std::string> enterAnimClientIds_;
    mutable std::mutex chatQueueMutex_;
};
