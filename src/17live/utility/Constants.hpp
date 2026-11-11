#pragma once

#include <cstddef>

namespace ably {
    constexpr int kRealtimeRequestMs = 10000;
}

namespace limits {
    constexpr std::size_t kMaxWebSocketMessageBytes = 16 * 1024 * 1024;
}
