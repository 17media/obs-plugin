#pragma once

#include <string>

namespace seventeen {
    namespace utility {
        class CrashSentinel {
           public:
            static void Initialize();
            static bool PreviousRunClean();
            static void Shutdown();
        };
    }  // namespace utility
}  // namespace seventeen

