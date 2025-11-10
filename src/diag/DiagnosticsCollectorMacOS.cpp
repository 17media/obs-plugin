#include "DiagnosticsCollectorMacOS.hpp"
#include <cstdlib>
#include <memory>
#include <array>
#include <filesystem>

namespace seventeen {
namespace diag {

DiagnosticsCollectorMacOS::DiagnosticsCollectorMacOS() {
}

std::string DiagnosticsCollectorMacOS::getSystemInfoImpl() const {
    std::stringstream ss;
    
    ss << "macOS Version: " << executeCommand("sw_vers -productVersion") << std::endl;
    ss << "Build Version: " << executeCommand("sw_vers -buildVersion") << std::endl;
    
    ss << "\nHardware Information:" << std::endl;
    ss << "Model: " << executeCommand("sysctl -n hw.model") << std::endl;
    ss << "CPU: " << executeCommand("sysctl -n machdep.cpu.brand_string") << std::endl;
    ss << "Memory: " << executeCommand("sysctl -n hw.memsize") << " bytes" << std::endl;
    ss << "Cores: " << executeCommand("sysctl -n hw.ncpu") << std::endl;
    
    ss << "\nGraphics Information:" << std::endl;
    ss << executeCommand("system_profiler SPDisplaysDataType | grep -E '(Chipset Model|VRAM|Metal)'") << std::endl;
    
    ss << "\nDisk Information:" << std::endl;
    ss << executeCommand("df -h /") << std::endl;
    
    return ss.str();
}

std::vector<std::string> DiagnosticsCollectorMacOS::collectOBSLogs() {
    std::vector<std::string> logFiles;
    std::string obsLogDir = getOBSLogDirectory();
    
    if (std::filesystem::exists(obsLogDir)) {
        auto files = getFilesInDirectory(obsLogDir, "*.log");
        std::string tempDir = generateTempDirectory();
        
        for (const auto& file : files) {
            std::string fileName = std::filesystem::path(file).filename().string();
            std::string destPath = std::filesystem::path(tempDir) / ("obs_" + fileName);
            
            if (copyFile(file, destPath)) {
                logFiles.push_back(destPath);
            }
        }
    }
    
    return logFiles;
}

std::vector<std::string> DiagnosticsCollectorMacOS::collectPluginLogs() {
    std::vector<std::string> logFiles;
    std::string pluginLogDir = getPluginLogDirectory();
    
    if (std::filesystem::exists(pluginLogDir)) {
        auto files = getFilesInDirectory(pluginLogDir, "*.log");
        std::string tempDir = generateTempDirectory();
        
        for (const auto& file : files) {
            std::string fileName = std::filesystem::path(file).filename().string();
            std::string destPath = std::filesystem::path(tempDir) / ("plugin_" + fileName);
            
            if (copyFile(file, destPath)) {
                logFiles.push_back(destPath);
            }
        }
    }
    
    return logFiles;
}

std::vector<std::string> DiagnosticsCollectorMacOS::collectNetworkLogs() {
    std::vector<std::string> networkLogs;
    
    std::string tempDir = generateTempDirectory();
    std::string networkInfoPath = std::filesystem::path(tempDir) / "network_info.txt";
    
    std::stringstream ss;
    ss << "Network Interfaces:" << std::endl;
    ss << executeCommand("ifconfig") << std::endl;
    ss << "\nNetwork Statistics:" << std::endl;
    ss << executeCommand("netstat -i") << std::endl;
    ss << "\nRouting Table:" << std::endl;
    ss << executeCommand("netstat -r") << std::endl;
    
    if (writeToFile(networkInfoPath, ss.str())) {
        networkLogs.push_back(networkInfoPath);
    }
    
    return networkLogs;
}

std::vector<std::string> DiagnosticsCollectorMacOS::collectCrashInfo() {
    std::vector<std::string> crashFiles;
    std::string crashDir = getCrashReportsDirectory();
    
    if (std::filesystem::exists(crashDir)) {
        auto files = getFilesInDirectory(crashDir, "*.crash");
        auto diagFiles = getFilesInDirectory(crashDir, "*.diag");
        
        std::string tempDir = generateTempDirectory();
        
        for (const auto& file : files) {
            std::string fileName = std::filesystem::path(file).filename().string();
            std::string destPath = std::filesystem::path(tempDir) / ("crash_" + fileName);
            
            if (copyFile(file, destPath)) {
                crashFiles.push_back(destPath);
            }
        }
        
        for (const auto& file : diagFiles) {
            std::string fileName = std::filesystem::path(file).filename().string();
            std::string destPath = std::filesystem::path(tempDir) / ("diag_" + fileName);
            
            if (copyFile(file, destPath)) {
                crashFiles.push_back(destPath);
            }
        }
    }
    
    return crashFiles;
}

std::vector<std::string> DiagnosticsCollectorMacOS::collectConfigSnapshot() {
    std::vector<std::string> configFiles;
    std::string homeDir = getHomeDirectory();
    
    std::string obsConfigDir = homeDir + "/Library/Application Support/obs-studio";
    std::string pluginConfigDir = homeDir + "/Library/Application Support/17live-obs-plugin";
    
    std::string tempDir = generateTempDirectory();
    
    if (std::filesystem::exists(obsConfigDir)) {
        auto globalIni = obsConfigDir + "/global.ini";
        if (std::filesystem::exists(globalIni)) {
            std::string destPath = std::filesystem::path(tempDir) / "obs_global.ini";
            if (copyFile(globalIni, destPath)) {
                configFiles.push_back(destPath);
            }
        }
        
        auto basicIni = obsConfigDir + "/basic.ini";
        if (std::filesystem::exists(basicIni)) {
            std::string destPath = std::filesystem::path(tempDir) / "obs_basic.ini";
            if (copyFile(basicIni, destPath)) {
                configFiles.push_back(destPath);
            }
        }
    }
    
    if (std::filesystem::exists(pluginConfigDir)) {
        auto pluginFiles = getFilesInDirectory(pluginConfigDir, "*.json");
        for (const auto& file : pluginFiles) {
            std::string fileName = std::filesystem::path(file).filename().string();
            std::string destPath = std::filesystem::path(tempDir) / ("plugin_" + fileName);
            
            if (copyFile(file, destPath)) {
                configFiles.push_back(destPath);
            }
        }
    }
    
    return configFiles;
}

std::vector<std::string> DiagnosticsCollectorMacOS::collectNetworkRequests() {
    std::vector<std::string> requestFiles;
    
    std::string tempDir = generateTempDirectory();
    std::string requestsPath = std::filesystem::path(tempDir) / "network_requests.txt";
    
    std::stringstream ss;
    ss << "Network Request Log (Sanitized)" << std::endl;
    ss << "Generated on: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;
    ss << "Note: Sensitive information has been filtered out" << std::endl;
    ss << "========================================" << std::endl;
    
    if (writeToFile(requestsPath, ss.str())) {
        requestFiles.push_back(requestsPath);
    }
    
    return requestFiles;
}

bool DiagnosticsCollectorMacOS::createZipArchive(const std::string& outputPath, const std::vector<std::string>& files) {
    if (files.empty()) {
        return false;
    }
    
    std::string tempDir = generateTempDirectory();
    std::string fileListPath = std::filesystem::path(tempDir) / "file_list.txt";
    
    std::ofstream fileList(fileListPath);
    if (!fileList.is_open()) {
        setLastError("Failed to create file list for zip");
        return false;
    }
    
    for (const auto& file : files) {
        if (std::filesystem::exists(file)) {
            fileList << file << std::endl;
        }
    }
    fileList.close();
    
    std::string zipCommand = "cd \"" + std::filesystem::path(files[0]).parent_path().string() + 
                            "\" && zip -@ \"" + outputPath + "\" < \"" + fileListPath + "\"";
    
    std::string result = executeCommand(zipCommand);
    
    return std::filesystem::exists(outputPath) && std::filesystem::file_size(outputPath) > 0;
}

std::string DiagnosticsCollectorMacOS::executeCommand(const std::string& command) const {
    std::array<char, 128> buffer;
    std::string result;
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    pclose(pipe);
    return result;
}

std::string DiagnosticsCollectorMacOS::getHomeDirectory() const {
    const char* home = getenv("HOME");
    return home ? std::string(home) : "";
}

std::string DiagnosticsCollectorMacOS::getOBSLogDirectory() const {
    return getHomeDirectory() + "/Library/Application Support/obs-studio/logs";
}

std::string DiagnosticsCollectorMacOS::getPluginLogDirectory() const {
    return getHomeDirectory() + "/Library/Application Support/17live-obs-plugin/logs";
}

std::string DiagnosticsCollectorMacOS::getCrashReportsDirectory() const {
    return getHomeDirectory() + "/Library/DiagnosticReports";
}

std::vector<std::string> DiagnosticsCollectorMacOS::getFilesInDirectory(const std::string& directory, const std::string& pattern) {
    std::vector<std::string> files;
    
    if (!std::filesystem::exists(directory)) {
        return files;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                
                if (pattern == "*.log" && filename.find(".log") != std::string::npos) {
                    files.push_back(entry.path().string());
                } else if (pattern == "*.crash" && filename.find(".crash") != std::string::npos) {
                    files.push_back(entry.path().string());
                } else if (pattern == "*.diag" && filename.find(".diag") != std::string::npos) {
                    files.push_back(entry.path().string());
                } else if (pattern == "*.json" && filename.find(".json") != std::string::npos) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        setLastError(std::string("Error reading directory: ") + e.what());
    }
    
    return files;
}

bool DiagnosticsCollectorMacOS::copyFile(const std::string& source, const std::string& destination) {
    try {
        std::filesystem::create_directories(std::filesystem::path(destination).parent_path());
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to copy file: ") + e.what());
        return false;
    }
}

} // namespace diag
} // namespace seventeen
