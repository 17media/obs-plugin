#include "ChatBridgeService.hpp"

#include <algorithm>
#include <obs-module.h>
#include "plugin-support.h"

#include "../OneSevenLiveCoreManager.hpp"
#include "../chat/OneSevenLiveChatMessageHandler.hpp"
#include "../websocket/OneSevenLiveWebsocketServer.hpp"

ChatBridgeService::ChatBridgeService(OneSevenLiveCoreManager* coreManager) : coreManager_(coreManager) {}

void ChatBridgeService::onWebsocketMessage(const std::string& clientId, const std::string& message) {
    WsMessage m;
    if (!WsMessage::parse(message, m)) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] JSON parse error in message from %s",
                clientId.c_str());
        return;
    }

    auto* ws = coreManager_ ? coreManager_->getWebsocketServer() : nullptr;
    const bool hasServer = (ws && ws->is_running());
    if (m.type.empty() || !hasServer) {
        return;
    }

    if (m.is(ws::TypeAction)) {
        const std::string actionType = m.payloadString("type");
        if (actionType == ws::ActionRegisterChatDock) {
            {
                std::lock_guard<std::mutex> lock(chatQueueMutex_);
                chatDockClientId_ = clientId;
            }
            obs_log(LOG_INFO, "[ChatQueue] ChatDock registered client=%s", clientId.c_str());
            flushChatEventQueue();
            return;
        }
    } else if (m.is(ws::EventAblyChatMessage)) {
        const std::string roomID = m.payloadString("roomID");
        const std::string data = m.payloadString("data");
        if (roomID.empty() || data.empty()) {
            obs_log(LOG_WARNING, "[17Live WebSocket Server] Missing roomID or data in Ably message");
            return;
        }
        {
            nlohmann::json wrapper;
            wrapper["messages"] = nlohmann::json::array({nlohmann::json{{"data", data}}});
            OneSevenLiveChatMessageHandler handler;
            handler.handleRaw(wrapper.dump());
        }
    }
}

void ChatBridgeService::onWebsocketConnectionChanged(const std::string& clientId, bool connected) {
    if (connected) {
        obs_log(LOG_INFO, "[17Live WebSocket] Client %s connected", clientId.c_str());
        flushChatEventQueue();
    } else {
        obs_log(LOG_INFO, "[17Live WebSocket] Client %s disconnected", clientId.c_str());
        std::lock_guard<std::mutex> lock(chatQueueMutex_);
        if (!chatDockClientId_.empty() && chatDockClientId_ == clientId) {
            chatDockClientId_.clear();
        }
    }
}

bool ChatBridgeService::isChatDockClientConnectedLocked() const {
    if (chatDockClientId_.empty()) {
        return false;
    }
    auto* ws = coreManager_ ? coreManager_->getWebsocketServer() : nullptr;
    if (!ws || !ws->is_running()) {
        return false;
    }
    const auto ids = ws->getConnectedClientIds();
    return std::find(ids.begin(), ids.end(), chatDockClientId_) != ids.end();
}

void ChatBridgeService::enqueueOrBroadcastChatEvent(const QString& type, const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(chatQueueMutex_);
    auto* ws = coreManager_ ? coreManager_->getWebsocketServer() : nullptr;
    if (ws && ws->is_running() && isChatDockClientConnectedLocked()) {
        obs_log(LOG_DEBUG, "Sending chat event to chat dock client %s", chatDockClientId_.c_str());
        ws->sendMessageToClient(chatDockClientId_, WsMessage{type.toStdString(), payload}.dump());
        return;
    }

    chatEventQueue_.push_back(WsMessage{type.toStdString(), payload});
    if (chatEventQueue_.size() > chatQueueMaxSize_) {
        chatEventQueue_.pop_front();
    }
    obs_log(LOG_DEBUG, "[ChatQueue] Enqueued chat event. queueSize=%zu", chatEventQueue_.size());
}

void ChatBridgeService::flushChatEventQueue() {
    std::lock_guard<std::mutex> lock(chatQueueMutex_);

    if (!isChatDockClientConnectedLocked()) {
        return;
    }

    auto* ws = coreManager_ ? coreManager_->getWebsocketServer() : nullptr;
    if (!ws) {
        return;
    }

    obs_log(LOG_INFO, "[ChatQueue] Flushing %zu events to chatDock client=%s", chatEventQueue_.size(),
            chatDockClientId_.c_str());
    while (!chatEventQueue_.empty()) {
        const auto& m = chatEventQueue_.front();
        ws->sendMessageToClient(chatDockClientId_, m.dump());
        chatEventQueue_.pop_front();
    }
}

void ChatBridgeService::clear() {
    std::lock_guard<std::mutex> lock(chatQueueMutex_);
    chatDockClientId_.clear();
    chatEventQueue_.clear();
}
