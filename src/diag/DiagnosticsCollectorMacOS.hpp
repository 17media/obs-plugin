#pragma once

#include "DiagnosticsCollectorBase.hpp"

namespace seventeen {
namespace diag {

class DiagnosticsCollectorMacOS : public DiagnosticsCollectorBase {
public:
    DiagnosticsCollectorMacOS();
    ~DiagnosticsCollectorMacOS() override = default;
    
    bool isSupported() const override { return true; }

protected:
    std::string getPlatformName() const override { return "macOS"; }
    std::string getSystemInfoImpl() const override;
    std::vector<std::string> collectOBSLogs() override;
    std::vector<std::string> collectPluginLogs() override;
    std::vector<std::string> collectNetworkLogs() override;
    std::vector<std::string> collectCrashInfo() override;
    std::vector<std::string> collectConfigSnapshot() override;
    std::vector<std::string> collectNetworkRequests() override;
    bool createZipArchive(const std::string& outputPath, const std::vector<std::string>& files) override;

private:
    std::string executeCommand(const std::string& command) const;
    std::string getHomeDirectory() const;
    std::string getOBSLogDirectory() const;
    std::string getPluginLogDirectory() const;
    std::string getCrashReportsDirectory() const;
    std::vector<std::string> getFilesInDirectory(const std::string& directory, const std::string& pattern);
    bool copyFile(const std::string& source, const std::string& destination);
};

} // namespace diag
} // namespace seventeen
