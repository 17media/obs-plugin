#pragma once

#include <QString>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

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
    bool isRegisteredChatDockClient(const std::string& clientId) const;
    bool handleRegisterAction(const std::string& clientId, const std::string& actionType);
    void handleActionMessage(const std::string& clientId, const WsMessage& m);
    void handleUserDialogAction(const WsMessage& m);
    void handleIncomingAblyMessage(const WsMessage& m);

    OneSevenLiveCoreManager* coreManager_;
    WsMessageQueue chatEventQueue_{5000};
    std::unordered_set<std::string> chatDockClientIds_;
    std::unordered_set<std::string> enterAnimClientIds_;
    mutable std::mutex chatQueueMutex_;
};
