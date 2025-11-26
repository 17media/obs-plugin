#pragma once
#include <nlohmann/json.hpp>
#include <QByteArray>
#include <QMetaObject>
#include <QString>
#include <zlib.h>
#include "../OneSevenLiveCoreManager.hpp"
#include "websocket/WsMessage.hpp"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
class OneSevenLiveChatMessageHandler {
public:
    bool handleRaw(const std::string& msg) {
        try {
            nlohmann::json j = nlohmann::json::parse(msg);
            if (j.contains("messages") && j["messages"].is_array()) {
                for (auto& m : j["messages"]) {
                    if (!m.contains("data") || !m["data"].is_string()) continue;
                    nlohmann::json decoded;
                    if (!gunzipBase64ToJson(m["data"].get<std::string>(), decoded)) continue;
                    int type = decoded.contains("type") && decoded["type"].is_number_integer() ? decoded["type"].get<int>() : -1;
                    routeByType(type, decoded);
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }
private:
    static bool gunzipBase64ToJson(const std::string& base64Data, nlohmann::json& out) {
        QByteArray raw = QByteArray::fromBase64(QByteArray::fromStdString(base64Data));
        if (raw.isEmpty()) return false;
        QByteArray outBuf;
        z_stream zs{};
        zs.next_in = reinterpret_cast<Bytef*>(raw.data());
        zs.avail_in = raw.size();
        if (inflateInit2(&zs, 15 + 16) != Z_OK) return false;
        char buf[4096];
        int ret;
        do {
            zs.next_out = reinterpret_cast<Bytef*>(buf);
            zs.avail_out = sizeof(buf);
            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) break;
            int have = sizeof(buf) - zs.avail_out;
            if (have > 0) outBuf.append(buf, have);
        } while (ret != Z_STREAM_END);
        inflateEnd(&zs);
        if (ret != Z_STREAM_END) return false;
        try {
            out = nlohmann::json::parse(outBuf.constData());
            return true;
        } catch (...) {
            return false;
        }
    }
    static void routeByType(int type, const nlohmann::json& decoded) {
        using namespace ws;
        if (type == 0 || type == 1 || type == 2 || type == 3 || type == 5 || type == 7) {
            OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(QString::fromUtf8(EventAblyChatMessage), decoded);
            if (type == 1 || type == 3) handleGiftPlayback(decoded);
        } else if (type == 6) {
            QMetaObject::invokeMethod(&OneSevenLiveCoreManager::getInstance(), []() {
                auto& core = OneSevenLiveCoreManager::getInstance();
                core.refreshRockZoneUserList();
            }, Qt::QueuedConnection);
        }
    }
    static void handleGiftPlayback(const nlohmann::json& decoded) {
        try {
            std::string giftID;
            if (decoded.contains("giftMsg") && decoded["giftMsg"].is_object()) {
                const auto& gm = decoded["giftMsg"];
                if (gm.contains("giftID") && gm["giftID"].is_string()) giftID = gm["giftID"].get<std::string>();
            }
            std::optional<nlohmann::json> gift;
            if (!giftID.empty()) gift = OneSevenLiveCoreManager::getInstance().getGiftByID(giftID);
            if (gift && gift->contains("vffURL") && gift->contains("vffJson") && (*gift)["vffURL"].is_string() && (*gift)["vffJson"].is_string()) {
                nlohmann::json playData;
                playData["type"] = "play_vff";
                playData["vffURL"] = (*gift)["vffURL"].get<std::string>();
                playData["vffJson"] = (*gift)["vffJson"].get<std::string>();
                try {
                    const auto& gm = decoded["giftMsg"];
                    if (gm.contains("giftMetas") && gm["giftMetas"].is_array() && !gm["giftMetas"].empty()) {
                        const auto& meta0 = gm["giftMetas"][0];
                        if (meta0.contains("composite") && meta0["composite"].is_array()) {
                            nlohmann::json compositeObj = nlohmann::json::object();
                            for (const auto& item : meta0["composite"]) {
                                if (item.contains("tag") && item.contains("imageURL") && item["tag"].is_string() && item["imageURL"].is_string()) {
                                    compositeObj[item["tag"].get<std::string>()] = item["imageURL"].get<std::string>();
                                }
                            }
                            if (!compositeObj.empty()) playData["compositeData"] = compositeObj;
                        }
                    }
                } catch (...) {}
                try {
                    auto* ws = OneSevenLiveCoreManager::getInstance().getWebsocketServer();
                    if (ws && ws->is_running()) ws->broadcastMessage(playData.dump());
                } catch (...) {}
            }
        } catch (...) {}
    }
};
