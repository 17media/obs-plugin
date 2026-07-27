#include "ChatBridgeService.hpp"

#include <obs-module.h>

#include <QMainWindow>
#include <QMetaObject>
#include <algorithm>

#include "../OneSevenLiveCoreManager.hpp"
#include "../chat/OneSevenLiveChatMessageHandler.hpp"
#include "../websocket/OneSevenLiveWebsocketServer.hpp"
#include "plugin-support.h"
#include "rockzone/OneSevenLiveUserDialog.hpp"

namespace {
    bool isEnterAnimationPayload(const WsMessage& m) {
        return m.type == ws::EventAblyChatMessage && m.payload.contains("type") &&
               m.payload["type"].is_number_integer() && m.payload["type"].get<int>() == 27;
    }

    std::string buildEnterAnimationSummary(const nlohmann::json& payload) {
        const auto* enter =
            payload.contains("subscriberEnterMsg") && payload["subscriberEnterMsg"].is_object()
                ? &payload["subscriberEnterMsg"]
            : payload.contains("enterAnimationMsg") && payload["enterAnimationMsg"].is_object()
                ? &payload["enterAnimationMsg"]
                : nullptr;
        if (!enter) {
            return "payload=missing";
        }

        const int animation =
            enter->contains("animation") && (*enter)["animation"].is_number_integer()
                ? (*enter)["animation"].get<int>()
                : -1;
        const std::string userID = enter->contains("userID") && (*enter)["userID"].is_string()
                                       ? (*enter)["userID"].get<std::string>()
                                       : "";
        const std::string displayName =
            enter->contains("displayName") && (*enter)["displayName"].is_string()
                ? (*enter)["displayName"].get<std::string>()
                : "";
        std::string notifAnimationID;
        if (enter->contains("eventNotifMsg") && (*enter)["eventNotifMsg"].is_object()) {
            const auto& notif = (*enter)["eventNotifMsg"];
            if (notif.contains("animationID") && notif["animationID"].is_string()) {
                notifAnimationID = notif["animationID"].get<std::string>();
            }
        }

        return QString("animation=%1 userID=%2 displayName=%3 notifAnimationID=%4")
            .arg(animation)
            .arg(QString::fromStdString(userID))
            .arg(QString::fromStdString(displayName))
            .arg(QString::fromStdString(notifAnimationID))
            .toStdString();
    }
}  // namespace

ChatBridgeService::ChatBridgeService(OneSevenLiveCoreManager* coreManager)
    : coreManager_(coreManager) {}

bool ChatBridgeService::hasAnyConnectedTargetLocked(
    const std::unordered_set<std::string>& targets) const {
    if (targets.empty()) {
        return false;
    }
    auto* ws = coreManager_ ? coreManager_->getWebsocketServer() : nullptr;
    if (!ws || !ws->is_running()) {
        return false;
    }
    const auto ids = ws->getConnectedClientIds();
    for (const auto& id : ids) {
        if (targets.find(id) != targets.end()) {
            return true;
        }
    }
    return false;
}

const std::unordered_set<std::string>& ChatBridgeService::resolveTargetsLocked(
    const WsMessage& m) const {
    if (m.type == ws::EventAblyChatMessage && m.payload.contains("type") &&
        m.payload["type"].is_number_integer() && m.payload["type"].get<int>() == 27) {
        return enterAnimClientIds_;
    }
    return chatDockClientIds_;
}

void ChatBridgeService::sendToTargetsLocked(const WsMessage& m,
                                            const std::unordered_set<std::string>& targets) {
    auto* ws = coreManager_ ? coreManager_->getWebsocketServer() : nullptr;
    if (!ws || !ws->is_running() || targets.empty()) {
        return;
    }
    const auto connected = ws->getConnectedClientIds();
    const auto msg = m.dump();
    size_t deliveredCount = 0;
    for (const auto& id : connected) {
        if (targets.find(id) == targets.end()) {
            continue;
        }
        ws->sendMessageToClient(id, msg);
        deliveredCount++;
    }
    // if (isEnterAnimationPayload(m)) {
    //     obs_log(LOG_INFO,
    //             "[ChatQueue][EnterAnimation] delivered to %zu clients: %s", deliveredCount,
    //             buildEnterAnimationSummary(m.payload).c_str());
    // }
}

bool ChatBridgeService::isRegisteredChatDockClient(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(chatQueueMutex_);
    return chatDockClientIds_.find(clientId) != chatDockClientIds_.end();
}

bool ChatBridgeService::handleRegisterAction(const std::string& clientId,
                                             const std::string& actionType) {
    std::unordered_set<std::string>* targets = nullptr;
    if (actionType == ws::ActionRegisterChatDock) {
        targets = &chatDockClientIds_;
    } else if (actionType == ws::ActionRegisterEnterAnimationPage) {
        targets = &enterAnimClientIds_;
    }
    if (!targets) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(chatQueueMutex_);
        targets->insert(clientId);
    }
    if (actionType == ws::ActionRegisterChatDock) {
        obs_log(LOG_INFO, "[ChatQueue] ChatDock registered client=%s", clientId.c_str());
    }
    flushChatEventQueue();
    return true;
}

void ChatBridgeService::handleUserDialogAction(const WsMessage& m) {
    const std::string userID = m.payloadString("userID");
    if (userID.empty()) {
        return;
    }

    std::string displayName = m.payloadString("displayName");
    const std::string picture = m.payloadString("picture");
    int level = 0;
    if (m.payload.contains("level") && m.payload["level"].is_number()) {
        level = m.payload["level"].get<int>();
    }

    QMainWindow* mainWindow = coreManager_ ? coreManager_->getMainWindow() : nullptr;
    OneSevenLiveApiWrappers* apiWrapper = coreManager_ ? coreManager_->getApiWrapper() : nullptr;
    OneSevenLiveConfigManager* configManager =
        coreManager_ ? coreManager_->getConfigManager() : nullptr;
    if (!mainWindow || !apiWrapper || !configManager) {
        return;
    }

    QMetaObject::invokeMethod(
        mainWindow,
        [mainWindow, apiWrapper, configManager, userID, displayName, picture, level]() {
            OneSevenLiveRockZoneViewer viewer;
            viewer.displayUser.userID = QString::fromStdString(userID);
            viewer.displayUser.displayName =
                QString::fromStdString(displayName.empty() ? userID : displayName);
            viewer.displayUser.picture = QString::fromStdString(picture);
            viewer.displayUser.level = level;

            auto* dialog = new OneSevenLiveUserDialog(mainWindow, apiWrapper, configManager);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setUserInfo(viewer);
            dialog->show();
        },
        Qt::QueuedConnection);
}

void ChatBridgeService::handleIncomingAblyMessage(const WsMessage& m) {
    const std::string roomID = m.payloadString("roomID");
    const std::string data = m.payloadString("data");
    if (roomID.empty() || data.empty()) {
        obs_log(LOG_WARNING, "[17Live WebSocket Server] Missing roomID or data in Ably message");
        return;
    }

    nlohmann::json wrapper;
    wrapper["messages"] = nlohmann::json::array({nlohmann::json{{"data", data}}});
    OneSevenLiveChatMessageHandler handler;
    handler.handleRaw(wrapper.dump());
}

void ChatBridgeService::handleActionMessage(const std::string& clientId, const WsMessage& m) {
    const std::string actionType = m.payloadString("type");
    if (handleRegisterAction(clientId, actionType)) {
        return;
    }
    if (!isRegisteredChatDockClient(clientId)) {
        return;
    }
    if (actionType == ws::ActionOpenUserDialog) {
        handleUserDialogAction(m);
    }
}

void ChatBridgeService::onWebsocketMessage(const std::string& clientId,
                                           const std::string& message) {
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
        handleActionMessage(clientId, m);
        return;
    }
    if (m.is(ws::EventAblyChatMessage)) {
        handleIncomingAblyMessage(m);
    }
}

void ChatBridgeService::onWebsocketConnectionChanged(const std::string& clientId, bool connected) {
    if (connected) {
        obs_log(LOG_INFO, "[17Live WebSocket] Client %s connected", clientId.c_str());
        flushChatEventQueue();
    } else {
        obs_log(LOG_INFO, "[17Live WebSocket] Client %s disconnected", clientId.c_str());
        std::lock_guard<std::mutex> lock(chatQueueMutex_);
        chatDockClientIds_.erase(clientId);
        enterAnimClientIds_.erase(clientId);
    }
}

void ChatBridgeService::enqueueOrBroadcastChatEvent(const QString& type,
                                                    const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(chatQueueMutex_);
    const WsMessage m{type.toStdString(), payload};
    const bool isEnterAnimation = isEnterAnimationPayload(m);
    if (m.type == ws::EventAblyChatConnected) {
        if (hasAnyConnectedTargetLocked(chatDockClientIds_) ||
            hasAnyConnectedTargetLocked(enterAnimClientIds_)) {
            sendToTargetsLocked(m, chatDockClientIds_);
            sendToTargetsLocked(m, enterAnimClientIds_);
            return;
        }
    } else {
        const auto& targets = resolveTargetsLocked(m);
        if (hasAnyConnectedTargetLocked(targets)) {
            // if (isEnterAnimation) {
            //     obs_log(LOG_INFO,
            //             "[ChatQueue][EnterAnimation] broadcasting immediately to registered "
            //             "clients=%zu: %s",
            //             targets.size(), buildEnterAnimationSummary(payload).c_str());
            // }
            sendToTargetsLocked(m, targets);
            return;
        }
    }

    chatEventQueue_.enqueue(m);
    if (isEnterAnimation) {
        // obs_log(LOG_INFO,
        //         "[ChatQueue][EnterAnimation] enqueued queueSize=%zu enterAnimClients=%zu "
        //         "chatDockClients=%zu: %s",
        //         chatEventQueue_.size(), enterAnimClientIds_.size(), chatDockClientIds_.size(),
        //         buildEnterAnimationSummary(payload).c_str());
    } else {
        obs_log(LOG_DEBUG, "[ChatQueue] Enqueued chat event. queueSize=%zu",
                chatEventQueue_.size());
    }
}

void ChatBridgeService::flushChatEventQueue() {
    std::lock_guard<std::mutex> lock(chatQueueMutex_);
    if (!hasAnyConnectedTargetLocked(chatDockClientIds_) &&
        !hasAnyConnectedTargetLocked(enterAnimClientIds_)) {
        return;
    }

    obs_log(LOG_INFO, "[ChatQueue] Flushing %zu events", chatEventQueue_.size());
    while (!chatEventQueue_.empty()) {
        const auto m = chatEventQueue_.front();
        if (m.type == ws::EventAblyChatConnected) {
            sendToTargetsLocked(m, chatDockClientIds_);
            sendToTargetsLocked(m, enterAnimClientIds_);
        } else {
            const auto& targets = resolveTargetsLocked(m);
            // if (isEnterAnimationPayload(m)) {
            //     obs_log(LOG_INFO,
            //             "[ChatQueue][EnterAnimation] flushing queued event to targets=%zu: %s",
            //             targets.size(), buildEnterAnimationSummary(m.payload).c_str());
            // }
            sendToTargetsLocked(m, targets);
        }
        chatEventQueue_.popFront();
    }
}

void ChatBridgeService::clear() {
    std::lock_guard<std::mutex> lock(chatQueueMutex_);
    chatDockClientIds_.clear();
    enterAnimClientIds_.clear();
    chatEventQueue_.clear();
}
