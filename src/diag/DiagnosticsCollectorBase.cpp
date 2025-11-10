#include "DiagnosticsCollectorBase.hpp"
#include "PrivacyFilter.hpp"
#include <thread>
#include <iomanip>

namespace seventeen {
namespace diag {

DiagnosticsCollectorBase::DiagnosticsCollectorBase() 
    : m_privacyFilter(std::make_unique<PrivacyFilter>()) {
}

CollectResult DiagnosticsCollectorBase::collect(const DiagnosticConfig& config) {
    CollectResult result;
    result.status = CollectStatus::SUCCESS;
    
    try {
        std::string tempDir = generateTempDirectory();
        if (tempDir.empty()) {
            result.status = CollectStatus::ERROR;
            result.message = "Failed to create temporary directory";
            return result;
        }
        
        reportProgress("Initializing collection", 0.0);
        
        std::vector<std::string> allFiles;
        double progressStep = 1.0 / config.categories.size();
        double currentProgress = 0.0;
        
        for (const auto& category : config.categories) {
            std::string categoryName = [category]() {
                switch (category) {
                    case DiagnosticCategory::OBS_LOGS: return "OBS logs";
                    case DiagnosticCategory::PLUGIN_LOGS: return "Plugin logs";
                    case DiagnosticCategory::NETWORK_LOGS: return "Network logs";
                    case DiagnosticCategory::SYSTEM_INFO: return "System information";
                    case DiagnosticCategory::CRASH_INFO: return "Crash information";
                    case DiagnosticCategory::CONFIG_SNAPSHOT: return "Configuration snapshot";
                    case DiagnosticCategory::NETWORK_REQUESTS: return "Network requests";
                    default: return "Unknown";
                }
            }();
            
            reportProgress("Collecting " + categoryName, currentProgress);
            
            auto categoryFiles = collectCategory(category);
            for (const auto& file : categoryFiles) {
                if (config.enablePrivacyFilter && !applyPrivacyFilter(file)) {
                    continue;
                }
                allFiles.push_back(file);
            }
            
            currentProgress += progressStep;
        }
        
        if (allFiles.empty()) {
            result.status = CollectStatus::ERROR;
            result.message = "No files were collected";
            return result;
        }
        
        reportProgress("Creating archive", 0.9);
        
        std::string outputPath = config.outputDirectory;
        if (outputPath.empty()) {
            outputPath = std::filesystem::path(std::filesystem::temp_directory_path()) / 
                        ("diagnostics_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".zip");
        }
        
        if (!createZipArchive(outputPath, allFiles)) {
            result.status = CollectStatus::ERROR;
            result.message = "Failed to create ZIP archive: " + getLastError();
            return result;
        }
        
        result.outputPath = outputPath;
        result.collectedFiles = allFiles;
        result.message = "Diagnostics collection completed successfully";
        
        reportProgress("Collection completed", 1.0);
        
    } catch (const std::exception& e) {
        result.status = CollectStatus::ERROR;
        result.message = std::string("Exception during collection: ") + e.what();
    }
    
    return result;
}

std::vector<DiagnosticCategory> DiagnosticsCollectorBase::getAvailableCategories() const {
    return {
        DiagnosticCategory::OBS_LOGS,
        DiagnosticCategory::PLUGIN_LOGS,
        DiagnosticCategory::NETWORK_LOGS,
        DiagnosticCategory::SYSTEM_INFO,
        DiagnosticCategory::CRASH_INFO,
        DiagnosticCategory::CONFIG_SNAPSHOT,
        DiagnosticCategory::NETWORK_REQUESTS
    };
}

std::vector<std::string> DiagnosticsCollectorBase::collectCategory(DiagnosticCategory category) {
    switch (category) {
        case DiagnosticCategory::OBS_LOGS:
            return collectOBSLogs();
        case DiagnosticCategory::PLUGIN_LOGS:
            return collectPluginLogs();
        case DiagnosticCategory::NETWORK_LOGS:
            return collectNetworkLogs();
        case DiagnosticCategory::SYSTEM_INFO:
            return {writeSystemInfoToFile()};
        case DiagnosticCategory::CRASH_INFO:
            return collectCrashInfo();
        case DiagnosticCategory::CONFIG_SNAPSHOT:
            return collectConfigSnapshot();
        case DiagnosticCategory::NETWORK_REQUESTS:
            return collectNetworkRequests();
        default:
            return {};
    }
}

bool DiagnosticsCollectorBase::applyPrivacyFilter(const std::string& filePath) {
    if (!m_privacyFilter) {
        return true;
    }
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string filteredContent = m_privacyFilter->filterSensitiveData(buffer.str());
    
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        return false;
    }
    
    outFile << filteredContent;
    return true;
}

std::string DiagnosticsCollectorBase::generateTempDirectory() {
    try {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        std::string tempDir = (std::filesystem::temp_directory_path() / 
                              ("17live_diagnostics_" + std::to_string(timestamp))).string();
        
        std::filesystem::create_directories(tempDir);
        return tempDir;
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to create temp directory: ") + e.what());
        return "";
    }
}

std::string DiagnosticsCollectorBase::sanitizeFileName(const std::string& filename) {
    std::string sanitized = filename;
    std::regex invalidChars(R"([<>:"/\\|?*])");
    sanitized = std::regex_replace(sanitized, invalidChars, "_");
    return sanitized;
}

bool DiagnosticsCollectorBase::writeToFile(const std::string& path, const std::string& content) {
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        
        std::ofstream file(path);
        if (!file.is_open()) {
            setLastError("Failed to open file for writing: " + path);
            return false;
        }
        
        file << content;
        return true;
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to write file: ") + e.what());
        return false;
    }
}

void DiagnosticsCollectorBase::reportProgress(const std::string& stage, double progress) {
    if (m_progressCallback) {
        m_progressCallback(stage, progress);
    }
}

std::string DiagnosticsCollectorBase::getSystemInfo() const {
    std::stringstream ss;
    ss << "Platform: " << getPlatformName() << std::endl;
    ss << getSystemInfoImpl();
    return ss.str();
}

std::string DiagnosticsCollectorBase::writeSystemInfoToFile() {
    std::string tempDir = generateTempDirectory();
    if (tempDir.empty()) {
        return "";
    }
    
    std::string filePath = std::filesystem::path(tempDir) / "system_info.txt";
    std::string systemInfo = getSystemInfo();
    
    if (writeToFile(filePath, systemInfo)) {
        return filePath;
    }
    
    return "";
}

} // namespace diag
} // namespace seventeen
