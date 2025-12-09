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
                if (!gunzipBase64ToJson(m["data"].get<std::string>(), decoded))
                    continue;
                int type = decoded.contains("type") && decoded["type"].is_number_integer()
                               ? decoded["type"].get<int>()
                               : -1;
                routeByType(type, decoded);
            }
        }
        return true;
    } catch (...) {
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
        nlohmann::json gm;
        if (decoded.contains("giftMsg") && decoded["giftMsg"].is_object()) {
            gm = decoded["giftMsg"];
            if (gm.contains("giftID") && gm["giftID"].is_string())
                giftID = gm["giftID"].get<std::string>();
        }
        std::optional<nlohmann::json> gift;
        if (!giftID.empty())
            gift = OneSevenLiveCoreManager::getInstance().getGiftByID(giftID);
        if (!gift || !gift->contains("vffURL") || !gift->contains("vffJson") ||
            !(*gift)["vffURL"].is_string() || !(*gift)["vffJson"].is_string()) {
            // inner gift is not a vff gift, check outside gift
            std::string extID;
            if (gm.contains("extID") && gm["extID"].is_string()) extID = gm["extID"].get<std::string>();
            std::optional<nlohmann::json> extGift = OneSevenLiveCoreManager::getInstance().getGiftByID(extID);
            if (!extGift || !extGift->contains("vffURL") || !extGift->contains("vffJson") ||
                !(*extGift)["vffURL"].is_string() || !(*extGift)["vffJson"].is_string()) {
                // no gift found
                obs_log(LOG_WARNING, "Missing VFF fields. message=%s", decoded.dump().c_str());
                return;
            }
            gift = extGift;
        }
        {
            std::string vffURL = (*gift)["vffURL"].get<std::string>();
            std::string vffJson = (*gift)["vffJson"].get<std::string>();
            if (vffURL.empty() || vffJson.empty()) {
                obs_log(LOG_WARNING, "Empty VFF fields. message=%s", decoded.dump().c_str());
                return;
            }
            nlohmann::json playData;
            playData["type"] = "play_vff";
            playData["vffURL"] = vffURL;
            playData["vffJson"] = vffJson;
            try {
                const auto& gm = decoded["giftMsg"];
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
            try {
                obs_log(LOG_DEBUG, "PlayData: %s", playData.dump().c_str());
                auto* ws = OneSevenLiveCoreManager::getInstance().getWebsocketServer();
                if (ws && ws->is_running())
                    ws->broadcastMessage(playData.dump());
            } catch (...) {
            }
        }
    } catch (...) {
    }
}
