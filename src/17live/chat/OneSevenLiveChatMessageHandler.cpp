#include "OneSevenLiveChatMessageHandler.hpp"

#include <obs-module.h>
#include <zlib.h>

#include <QByteArray>
#include <QMetaObject>
#include <QString>

#include "../OneSevenLiveCoreManager.hpp"
#include "api/OneSevenLiveAblyChatClient.hpp"
#include "plugin-support.h"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
#include "websocket/WsMessage.hpp"

bool OneSevenLiveChatMessageHandler::handleRaw(const std::string& msg) {
    try {
        nlohmann::json j = nlohmann::json::parse(msg);
        if (j.contains("messages") && j["messages"].is_array()) {
            for (auto& m : j["messages"]) {
                if (!m.contains("data") || !m["data"].is_string())
                    continue;
                nlohmann::json decoded;
                if (!gunzipBase64ToJson(m["data"].get<std::string>(), decoded)) {
                    obs_log(LOG_WARNING, "Failed to decode/gunzip message data");
                    continue;
                }
                int type = decoded.contains("type") && decoded["type"].is_number_integer()
                               ? decoded["type"].get<int>()
                               : -1;
                routeByType(type, decoded);
            }
        }
        return true;
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "OneSevenLiveChatMessageHandler::handleRaw exception: %s", e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "OneSevenLiveChatMessageHandler::handleRaw unknown exception");
        return false;
    }
}

bool OneSevenLiveChatMessageHandler::gunzipBase64ToJson(const std::string& base64Data,
                                                        nlohmann::json& out) {
    QByteArray raw = QByteArray::fromBase64(QByteArray::fromStdString(base64Data));
    if (raw.isEmpty())
        return false;
    QByteArray outBuf;
    z_stream zs{};
    zs.next_in = reinterpret_cast<Bytef*>(raw.data());
    zs.avail_in = raw.size();
    if (inflateInit2(&zs, 15 + 16) != Z_OK)
        return false;
    char buf[4096];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END)
            break;
        int have = sizeof(buf) - zs.avail_out;
        if (have > 0)
            outBuf.append(buf, have);
    } while (ret != Z_STREAM_END);
    inflateEnd(&zs);
    if (ret != Z_STREAM_END)
        return false;
    try {
        out = nlohmann::json::parse(outBuf.constData());
        return true;
    } catch (...) {
        return false;
    }
}

void OneSevenLiveChatMessageHandler::routeByType(int type, const nlohmann::json& decoded) {
    using namespace ws;
    switch (type) {
    case ably::MsgType_JOIN_ROOM:
    case ably::MsgType_NEW_GIFT:
    case ably::MsgType_NEW_LUCKYBAG:
    case ably::MsgType_POKE:
    case ably::MsgType_AI_COHOST_MESSAGE:
    case ably::MsgType_COMMENT:
        OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
            QString::fromUtf8(EventAblyChatMessage), decoded);
        if (type == ably::MsgType_NEW_LUCKYBAG || type == ably::MsgType_NEW_GIFT)
            handleGiftPlayback(decoded);
        break;
    case ably::MsgType_ROCKZONE:
        obs_log(LOG_DEBUG, "ROCKZONE");
        QMetaObject::invokeMethod(
            &OneSevenLiveCoreManager::getInstance(),
            []() {
                auto& core = OneSevenLiveCoreManager::getInstance();
                core.refreshRockZoneUserList();
            },
            Qt::QueuedConnection);
        break;
    default:
        obs_log(LOG_DEBUG, "Unknown chat message type: %d", type);
        break;
    }
}

void OneSevenLiveChatMessageHandler::handleGiftPlayback(const nlohmann::json& decoded) {
    obs_log(LOG_DEBUG, "Gift playback message received %s", decoded.dump().c_str());

    try {
        std::string giftID;
        std::string extID;
        nlohmann::json gm;
        if (decoded.contains("giftMsg") && decoded["giftMsg"].is_object()) {
            gm = decoded["giftMsg"];
            if (gm.contains("giftID") && gm["giftID"].is_string())
                giftID = gm["giftID"].get<std::string>();
            if (gm.contains("extID") && gm["extID"].is_string())
                extID = gm["extID"].get<std::string>();
        }

        auto& core = OneSevenLiveCoreManager::getInstance();

        auto hasVFF = [](const std::optional<nlohmann::json>& g) -> bool {
            return g && g->contains("vffURL") && g->contains("vffJson") &&
                   (*g)["vffURL"].is_string() && !(*g)["vffURL"].get<std::string>().empty() &&
                   (*g)["vffJson"].is_string() && !(*g)["vffJson"].get<std::string>().empty();
        };

        auto sendById = [&](const std::string& id, bool attachComposite) {
            if (id.empty())
                return false;
            auto gift = core.getGiftByID(id);
            if (!hasVFF(gift))
                return false;
            nlohmann::json playData;
            playData["type"] = "play_vff";
            playData["vffURL"] = (*gift)["vffURL"].get<std::string>();
            playData["vffJson"] = (*gift)["vffJson"].get<std::string>();
            if (attachComposite) {
                try {
                    if (gm.contains("giftMetas") && gm["giftMetas"].is_array() &&
                        !gm["giftMetas"].empty()) {
                        const auto& meta0 = gm["giftMetas"][0];
                        if (meta0.contains("composite") && meta0["composite"].is_array()) {
                            nlohmann::json compositeObj = nlohmann::json::object();
                            for (const auto& item : meta0["composite"]) {
                                if (item.contains("tag") && item.contains("imageURL") &&
                                    item["tag"].is_string() && item["imageURL"].is_string()) {
                                    compositeObj[item["tag"].get<std::string>()] =
                                        item["imageURL"].get<std::string>();
                                }
                            }
                            if (!compositeObj.empty())
                                playData["compositeData"] = compositeObj;
                        }
                    }
                } catch (...) {
                }
            }
            auto* ws = core.getWebsocketServer();
            if (ws && ws->is_running()) {
                ws->broadcastMessage(playData.dump());
                return true;
            }
            return false;
        };

        bool sentExt = sendById(extID, false);
        bool sentGift = sendById(giftID, true);
        if (!sentExt && !sentGift) {
            obs_log(LOG_WARNING, "Missing VFF fields for giftID=%s and extID=%s. message=%s",
                    giftID.c_str(), extID.c_str(), decoded.dump().c_str());
        }

    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "Exception in handleGiftPlayback: %s", e.what());
    } catch (...) {
        obs_log(LOG_ERROR, "Unknown exception in handleGiftPlayback");
    }
}
