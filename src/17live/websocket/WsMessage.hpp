#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace ws {
    static constexpr const char* TypeTransmit = "transmit";
    static constexpr const char* TypeAction = "action";
    static constexpr const char* ActionRefreshRockzone = "refresh_rockzone";
    static constexpr const char* ActionRegisterChatDock = "register_chatdock";
    static constexpr const char* ActionRegisterEnterAnimationPage = "register_enter_animation_page";
    static constexpr const char* ActionOpenUserDialog = "open_user_dialog";
    static constexpr const char* EventTwitchChatConnected = "twitch_chat_connected";
    static constexpr const char* EventTwitchChatMessage = "twitch_chat_message";
    static constexpr const char* EventYouTubeChatConnected = "youtube_chat_connected";
    static constexpr const char* EventYouTubeChatMessage = "youtube_chat_message";
    static constexpr const char* EventAblyChatConnected = "ably_chat_connected";
    static constexpr const char* EventAblyChatMessage = "ably_chat_message";
}  // namespace ws

struct WsMessage {
    std::string type;
    nlohmann::json payload;

    static bool parse(const std::string& s, WsMessage& out) {
        try {
            nlohmann::json j = nlohmann::json::parse(s);
            if (j.contains("type") && j["type"].is_string())
                out.type = j["type"].get<std::string>();
            if (j.contains("payload") && j["payload"].is_object())
                out.payload = j["payload"];
            else
                out.payload = nlohmann::json::object();
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string dump() const {
        return nlohmann::json{{"type", type}, {"payload", payload}}.dump();
    }

    bool is(const std::string& t) const {
        return type == t;
    }

    bool is(const char* t) const {
        return type == t;
    }

    std::string payloadString(const std::string& key) const {
        if (payload.contains(key) && payload[key].is_string())
            return payload[key].get<std::string>();
        return std::string();
    }
};
