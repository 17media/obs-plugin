#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace seventeen {
namespace diag {

enum class DiagnosticCategory {
    OBS_LOGS,
    PLUGIN_LOGS,
    NETWORK_LOGS,
    SYSTEM_INFO,
    CRASH_INFO,
    CONFIG_SNAPSHOT,
    NETWORK_REQUESTS
};

enum class CollectStatus {
    SUCCESS,
    ERROR,
    CANCELLED,
    PERMISSION_DENIED
};

struct CollectResult {
    CollectStatus status;
    std::string message;
    std::string outputPath;
    std::vector<std::string> collectedFiles;
};

struct DiagnosticConfig {
    std::vector<DiagnosticCategory> categories;
    std::string outputDirectory;
    bool enablePrivacyFilter;
    bool includeSensitiveData;
    std::vector<std::string> customPaths;
};

class IDiagnosticsCollector {
public:
    virtual ~IDiagnosticsCollector() = default;
    
    virtual CollectResult collect(const DiagnosticConfig& config) = 0;
    
    virtual std::vector<DiagnosticCategory> getAvailableCategories() const = 0;
    
    virtual std::string getSystemInfo() const = 0;
    
    virtual bool isSupported() const = 0;
    
    virtual std::string getLastError() const = 0;
    
    using ProgressCallback = std::function<void(const std::string& stage, double progress)>;
    virtual void setProgressCallback(ProgressCallback callback) = 0;
};

std::unique_ptr<IDiagnosticsCollector> createDiagnosticsCollector();

} // namespace diag
} // namespace seventeen
