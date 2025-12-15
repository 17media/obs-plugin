#include "IDiagnosticsCollector.hpp"

#ifdef __APPLE__
#include "DiagnosticsCollectorMacOS.hpp"
#elif defined(_WIN32)
#include "DiagnosticsCollectorWindows.hpp"
#endif

namespace seventeen {
    namespace diag {

        std::unique_ptr<IDiagnosticsCollector> createDiagnosticsCollector() {
#ifdef __APPLE__
            return std::make_unique<DiagnosticsCollectorMacOS>();
#elif defined(_WIN32)
            return std::make_unique<DiagnosticsCollectorWindows>();
#else
            return nullptr;
#endif
        }

    }  // namespace diag
}  // namespace seventeen
