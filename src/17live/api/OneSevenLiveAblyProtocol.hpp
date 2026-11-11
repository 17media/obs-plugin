#pragma once

#include <cstdint>
#include <limits>
#include <string>

#include <nlohmann/json.hpp>

#include "../utility/Constants.hpp"

namespace ably {
enum class ErrorKind { Token, Permission, Other };

inline ErrorKind classifyError(int code, int statusCode) {
    if (code >= 40140 && code <= 40149)
        return ErrorKind::Token;
    if (code == 40160 || statusCode == 403)
        return ErrorKind::Permission;
    return ErrorKind::Other;
}

inline bool isAuthUpdate(bool attached, const std::string& oldId, const std::string& newId) {
    return attached && (newId.empty() || (!oldId.empty() && newId == oldId));
}

inline int idleTimeoutMs(const nlohmann::json& details) {
    if (!details.is_object())
        return 0;
    const auto it = details.find("maxIdleInterval");
    if (it == details.end() || !it->is_number_integer())
        return 0;
    if (!it->is_number_unsigned() && it->get<std::int64_t>() <= 0)
        return 0;
    const auto interval = it->get<std::uint64_t>();
    if (interval == 0 ||
        interval > static_cast<std::uint64_t>(std::numeric_limits<int>::max() - kRealtimeRequestMs))
        return 0;
    return static_cast<int>(interval) + kRealtimeRequestMs;
}

class ChannelRecovery {
   public:
    void reset() {
        serial_.clear();
        wasAttached_ = false;
    }

    void updateSerial(const nlohmann::json& message) {
        const auto it = message.find("channelSerial");
        if (it != message.end() && it->is_string()) {
            const auto serial = it->get<std::string>();
            if (!serial.empty())
                serial_ = serial;
        }
    }

    nlohmann::json attach(const std::string& channel) const {
        nlohmann::json message{{"action", 10}, {"channel", channel}};
        if (!serial_.empty())
            message["channelSerial"] = serial_;
        if (wasAttached_)
            message["flags"] = 1 << 5;  // ATTACH_RESUME, per Ably protocol.
        return message;
    }

    bool attached(const nlohmann::json& message) {
        const bool expectedContinuity = wasAttached_;
        const auto flags = message.value("flags", 0);
        updateSerial(message);
        wasAttached_ = true;
        return expectedContinuity && (flags & (1 << 2)) == 0;  // RESUMED
    }

   private:
    std::string serial_;
    bool wasAttached_{false};
};
}  // namespace ably
