#pragma once

#include "DiagnosticsCollectorBase.hpp"

namespace seventeen {
namespace diag {

class DiagnosticsCollectorWindows : public DiagnosticsCollectorBase {
public:
    DiagnosticsCollectorWindows();
    ~DiagnosticsCollectorWindows() override = default;
    
    bool isSupported() const override;

protected:
    std::string getPlatformName() const override { return "Windows"; }
    std::string getSystemInfoImpl() const override;
    std::vector<std::string> collectOBSLogs() override;
    std::vector<std::string> collectPluginLogs() override;
    std::vector<std::string> collectNetworkLogs() override;
    std::vector<std::string> collectCrashInfo() override;
    std::vector<std::string> collectConfigSnapshot() override;
    std::vector<std::string> collectNetworkRequests() override;
    bool createZipArchive(const std::string& outputPath, const std::vector<std::string>& files) override;

private:
    std::string executePowerShellCommand(const std::string& command);
    std::string getAppDataPath() const;
    std::string getOBSLogDirectory() const;
    std::string getPluginLogDirectory() const;
    std::string getCrashDumpDirectory() const;
    std::vector<std::string> getFilesInDirectory(const std::string& directory, const std::string& extension);
    bool copyFile(const std::string& source, const std::string& destination);
};

} // namespace diag
} // namespace seventeen    std::string getWMIInfo(const std::string& wmiClass, const std::string& property);
};

} // namespace diag
} // namespace seventeen
