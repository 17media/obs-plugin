#pragma once

#include "IDiagnosticsCollector.hpp"
#include <memory>

namespace seventeen {
namespace diag {

std::unique_ptr<IDiagnosticsCollector> createDiagnosticsCollector();

} // namespace diag
} // namespace seventeen
