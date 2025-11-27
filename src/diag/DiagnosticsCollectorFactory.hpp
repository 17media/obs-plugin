#pragma once

#include <memory>

#include "IDiagnosticsCollector.hpp"

namespace seventeen {
    namespace diag {

        std::unique_ptr<IDiagnosticsCollector> createDiagnosticsCollector();

    }  // namespace diag
}  // namespace seventeen
