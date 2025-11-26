#pragma once
#include <nlohmann/json.hpp>
#include <string>
class OneSevenLiveChatMessageHandler {
public:
    bool handleRaw(const std::string& msg);
private:
    static bool gunzipBase64ToJson(const std::string& base64Data, nlohmann::json& out);
    static void routeByType(int type, const nlohmann::json& decoded);
    static void handleGiftPlayback(const nlohmann::json& decoded);
};
