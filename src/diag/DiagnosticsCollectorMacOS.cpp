#include "DiagnosticsCollectorMacOS.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace seventeen {
    namespace diag {

        DiagnosticsCollectorMacOS::DiagnosticsCollectorMacOS() {}

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
            ss << executeCommand(
                      "system_profiler SPDisplaysDataType | grep -E '(Chipset Model|VRAM|Metal)'")
               << std::endl;

            ss << "\nDisk Information:" << std::endl;
            ss << executeCommand("df -h /") << std::endl;

            return ss.str();
        }

        std::vector<std::string> DiagnosticsCollectorMacOS::collectOBSLogs() {
            std::vector<std::string> logFiles;
            std::string obsLogDir = getOBSLogDirectory();

            if (std::filesystem::exists(obsLogDir)) {
                auto files = getFilesInDirectory(obsLogDir, "*.txt");
                std::sort(files.begin(), files.end(),
                          [](const std::string& a, const std::string& b) {
                              return std::filesystem::last_write_time(a) >
                                     std::filesystem::last_write_time(b);
                          });
                std::string tempDir = generateTempDirectory();
                double total = files.size();
                for (size_t i = 0; i < files.size(); ++i) {
                    const auto& file = files[i];
                    std::string fileName = std::filesystem::path(file).filename().string();
                    reportSubProgress("Collecting OBS log: " + fileName, (double) i / total);
                    std::string destPath = std::filesystem::path(tempDir) / ("obs_" + fileName);
                    if (copyWithSizeLimit(file, destPath)) {
                        logFiles.push_back(destPath);
                    }
                }
            }

            return logFiles;
        }

        std::vector<std::string> DiagnosticsCollectorMacOS::collectPluginLogs() {
            std::vector<std::string> logFiles;

            // Candidate plugin log directories under OBS plugin_config and legacy ~/.17Live/logs
            std::string homeDir = getHomeDirectory();
            std::vector<std::string> candidates = {
                homeDir + "/Library/Application Support/obs-studio/plugin_config/17live/logs",
                homeDir + "/Library/Application Support/obs-studio/plugin_config/obs-17live/logs",
                homeDir + "/.17Live/logs"};

            std::vector<std::string> files;
            for (const auto& dir : candidates) {
                if (!std::filesystem::exists(dir))
                    continue;
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                        if (!entry.is_regular_file())
                            continue;
                        std::string name = entry.path().filename().string();
                        if (name.find(".log") != std::string::npos) {
                            files.push_back(entry.path().string());
                        }
                    }
                } catch (const std::exception& e) {
                    setLastError(std::string("Error reading plugin logs: ") + e.what());
                }
            }

            // Sort by modification time desc and choose latest 5
            std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
                return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
            });
            if (files.size() > 5)
                files.resize(5);

            std::string tempDir = generateTempDirectory();
            double total = files.size();
            for (size_t i = 0; i < files.size(); ++i) {
                const auto& file = files[i];
                std::string fileName = std::filesystem::path(file).filename().string();
                reportSubProgress("Collecting plugin log: " + fileName, (double) i / total);
                std::string destPath = std::filesystem::path(tempDir) / ("plugin_" + fileName);
                if (copyWithSizeLimit(file, destPath)) {
                    logFiles.push_back(destPath);
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
            std::string homeDir = getHomeDirectory();
            // macOS crash reports locations
            std::vector<std::string> crashDirs = {homeDir + "/Library/Logs/DiagnosticReports",
                                                  std::string("/Library/Logs/DiagnosticReports")};

            auto now = std::chrono::system_clock::now();
            auto cutoff = now - std::chrono::hours(24 * 30);  // 30 days
            auto cutoff_fs = std::filesystem::file_time_type::clock::now() -
                             (std::chrono::system_clock::now() - cutoff);

            std::string tempDir = generateTempDirectory();

            // Temporary vector to store valid crash files with their modification times
            struct CrashFileEntry {
                std::filesystem::path path;
                std::filesystem::file_time_type mtime;
            };

            std::vector<CrashFileEntry> foundCrashes;

            for (const auto& dir : crashDirs) {
                if (!std::filesystem::exists(dir))
                    continue;
                try {
                    // Use recursive iterator to support subdirectories like "Retired"
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                        if (!entry.is_regular_file())
                            continue;

                        auto name = entry.path().filename().string();
                        // Check for .crash or .ips extensions
                        bool isCrashFile = (entry.path().extension() == ".crash" ||
                                            entry.path().extension() == ".ips");
                        // Check for obs or OBS prefix (without underscore)
                        bool isOBS = (name.rfind("obs", 0) == 0 || name.rfind("OBS", 0) == 0);

                        if (isCrashFile && isOBS) {
                            try {
                                auto mtime = std::filesystem::last_write_time(entry.path());
                                if (mtime >= cutoff_fs) {
                                    foundCrashes.push_back({entry.path(), mtime});
                                }
                            } catch (...) {
                                // Skip file if mtime cannot be read
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    setLastError(std::string("Error reading crash reports: ") + e.what());
                }
            }

            // Sort by modification time descending
            std::sort(
                foundCrashes.begin(), foundCrashes.end(),
                [](const CrashFileEntry& a, const CrashFileEntry& b) { return a.mtime > b.mtime; });

            // Keep top 5 latest
            if (foundCrashes.size() > 5) {
                foundCrashes.resize(5);
            }

            // Copy selected files
            double total = foundCrashes.size();
            for (size_t i = 0; i < foundCrashes.size(); ++i) {
                const auto& entry = foundCrashes[i];
                std::string fileName = entry.path.filename().string();
                reportSubProgress("Collecting crash info: " + fileName, (double) i / total);
                std::string destPath = std::filesystem::path(tempDir) / ("crash_" + fileName);
                if (copyWithSizeLimit(entry.path.string(), destPath)) {
                    crashFiles.push_back(destPath);
                }
            }

            // Plugin custom crash dumps under plugin_config/<Plugin>/crash/*.dmp
            std::vector<std::string> pluginCrashDirs = {
                homeDir + "/Library/Application Support/obs-studio/plugin_config/17live/crash",
                homeDir + "/Library/Application Support/obs-studio/plugin_config/obs-17live/crash"};
            for (const auto& dir : pluginCrashDirs) {
                if (!std::filesystem::exists(dir))
                    continue;
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                        if (!entry.is_regular_file())
                            continue;
                        if (entry.path().extension() == ".dmp") {
                            std::string destPath = std::filesystem::path(tempDir) /
                                                   ("crash_" + entry.path().filename().string());
                            if (copyWithSizeLimit(entry.path().string(), destPath)) {
                                crashFiles.push_back(destPath);
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    setLastError(std::string("Error reading plugin crash dumps: ") + e.what());
                }
            }

            return crashFiles;
        }

        std::vector<std::string> DiagnosticsCollectorMacOS::collectConfigSnapshot() {
            std::vector<std::string> configFiles;
            std::string homeDir = getHomeDirectory();

            std::string obsConfigDir = homeDir + "/Library/Application Support/obs-studio";
            // Use correct plugin config path defined by OneSevenLiveConfigManager: ~/.17Live
            std::string pluginConfigDir = homeDir + "/.17Live";

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
                // Recursively copy all files under ~/.17Live to staging temp with preserved
                // structure
                std::vector<std::filesystem::path> filesToCopy;
                try {
                    for (auto const& entry :
                         std::filesystem::recursive_directory_iterator(pluginConfigDir)) {
                        if (entry.is_regular_file()) {
                            filesToCopy.push_back(entry.path());
                        }
                    }
                } catch (...) {
                }

                double total = filesToCopy.size();
                for (size_t i = 0; i < filesToCopy.size(); ++i) {
                    const auto& srcPath = filesToCopy[i];
                    std::filesystem::path rel = std::filesystem::relative(srcPath, pluginConfigDir);
                    std::filesystem::path destPath =
                        std::filesystem::path(tempDir) / "plugin_config" / rel;

                    reportSubProgress("Collecting config: " + srcPath.filename().string(),
                                      (double) i / total);

                    if (copyFile(srcPath.string(), destPath.string())) {
                        configFiles.push_back(destPath.string());
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
            ss << "Generated on: "
               << std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count()
               << std::endl;
            ss << "Note: Sensitive information has been filtered out" << std::endl;
            ss << "========================================" << std::endl;

            if (writeToFile(requestsPath, ss.str())) {
                requestFiles.push_back(requestsPath);
            }

            return requestFiles;
        }

        bool DiagnosticsCollectorMacOS::createZipArchive(const std::string& outputPath,
                                                         const std::vector<std::string>& files) {
            if (files.empty()) {
                return false;
            }

            // Create a staging directory where files are organized into category subdirectories
            std::string stagingDir = generateTempDirectory();
            if (stagingDir.empty()) {
                setLastError("Failed to create staging directory");
                return false;
            }

            // Helper to determine category folder name based on filename pattern
            auto determineCategory = [](const std::string& path) -> std::string {
                std::string name = std::filesystem::path(path).filename().string();
                // OBS logs
                if (name.rfind("obs_", 0) == 0 && name.find(".txt") != std::string::npos) {
                    return "obs_logs";
                }
                // Plugin logs
                if (name.rfind("plugin_", 0) == 0 && name.find(".log") != std::string::npos) {
                    return "plugin_logs";
                }
                // Crash information
                if (name.rfind("crash_", 0) == 0 || name.rfind("diag_", 0) == 0) {
                    return "crash_reports";
                }
                // Configuration snapshot
                if (name == "obs_global.ini" || name == "obs_basic.ini") {
                    return "Configuration snapshot";
                }
                if (name.rfind("plugin_", 0) == 0 && name.find(".json") != std::string::npos) {
                    return "Configuration snapshot";
                }
                // Any files under plugin_config directory should be treated as configuration
                // snapshot
                if (path.find("plugin_config") != std::string::npos) {
                    return "Configuration snapshot";
                }
                // System information goes to root
                if (name == "systeminfo.txt") {
                    return "ROOT";
                }
                // Network requests
                if (name == "network_requests.txt") {
                    return "Network requests";
                }
                // Fallback
                return "Misc";
            };

            std::vector<std::string> collectedFiles;
            std::vector<std::string> indexLines;

            try {
                double total = files.size();
                for (size_t i = 0; i < files.size(); ++i) {
                    const auto& file = files[i];
                    if (!std::filesystem::exists(file)) {
                        continue;
                    }

                    std::string relativePath;
                    std::string category = determineCategory(file);

                    std::filesystem::path destPath;
                    std::filesystem::path fileName = std::filesystem::path(file).filename();

                    reportSubProgress("Archiving: " + fileName.string(), (double) i / total);

                    if (category == "ROOT") {
                        destPath = std::filesystem::path(stagingDir) / fileName;
                        relativePath = fileName.string();
                        std::filesystem::create_directories(std::filesystem::path(stagingDir));
                    } else {
                        std::filesystem::path categoryDir =
                            std::filesystem::path(stagingDir) / category;
                        std::filesystem::create_directories(categoryDir);
                        destPath = categoryDir / fileName;
                        relativePath = (std::filesystem::path(category) / fileName).string();
                    }

                    // Check file size limit (2MB) for crash reports
                    bool isLargeCrash = false;
                    if (category == "crash_reports") {
                        try {
                            auto fileSize = std::filesystem::file_size(file);
                            if (fileSize > 2 * 1024 * 1024) {  // 2MB
                                isLargeCrash = true;
                            }
                        } catch (...) {
                        }
                    }

                    if (isLargeCrash) {
                        indexLines.push_back(relativePath + " (文件过大，未采集)");
                    } else {
                        try {
                            std::filesystem::copy_file(
                                file, destPath, std::filesystem::copy_options::overwrite_existing);
                            collectedFiles.push_back(
                                file);  // Keep track of what we actually copied
                            indexLines.push_back(relativePath);
                        } catch (const std::exception& e) {
                            setLastError(std::string("Failed to copy file: ") + e.what());
                            indexLines.push_back(relativePath + " (Copy failed: " + e.what() + ")");
                        }
                    }
                }

                // Generate index.txt
                try {
                    std::filesystem::path indexPath =
                        std::filesystem::path(stagingDir) / "index.txt";
                    std::ofstream indexFile(indexPath);
                    if (indexFile.is_open()) {
                        indexFile << "Diagnostics Package Content Index\n";
                        indexFile << "Generated on: " << executeCommand("date") << "\n";
                        indexFile << "========================================\n\n";
                        for (const auto& line : indexLines) {
                            indexFile << line << "\n";
                        }
                        indexFile.close();
                    }
                } catch (...) {
                    // Ignore index generation errors
                }

            } catch (const std::exception& e) {
                setLastError(std::string("Failed to prepare staging files: ") + e.what());
                return false;
            }

            // Zip from the parent of the staging directory so the ZIP contains
            // a top-level diagnostics folder with categorized subdirectories
            std::filesystem::path stagingPath(stagingDir);
            std::string parentDir = stagingPath.parent_path().string();
            std::string baseName = stagingPath.filename().string();
            std::string zipCommand =
                "cd \"" + parentDir + "\" && zip -r \"" + outputPath + "\" \"" + baseName + "\"";
            std::string result = executeCommand(zipCommand);
            (void) result;  // Suppress unused variable warning

            return std::filesystem::exists(outputPath) &&
                   std::filesystem::file_size(outputPath) > 0;
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
            // Prefer OBS plugin_config path; fallback to legacy ~/.17Live/logs
            std::string home = getHomeDirectory();
            std::string primary =
                home + "/Library/Application Support/obs-studio/plugin_config/17live/logs";
            if (std::filesystem::exists(primary))
                return primary;
            std::string alt =
                home + "/Library/Application Support/obs-studio/plugin_config/obs-17live/logs";
            if (std::filesystem::exists(alt))
                return alt;
            return home + "/.17Live/logs";
        }

        std::string DiagnosticsCollectorMacOS::getCrashReportsDirectory() const {
            return getHomeDirectory() + "/Library/Logs/DiagnosticReports";
        }

        std::vector<std::string> DiagnosticsCollectorMacOS::getFilesInDirectory(
            const std::string& directory, const std::string& pattern) {
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
                        } else if (pattern == "*.crash" &&
                                   filename.find(".crash") != std::string::npos) {
                            files.push_back(entry.path().string());
                        } else if (pattern == "*.diag" &&
                                   filename.find(".diag") != std::string::npos) {
                            files.push_back(entry.path().string());
                        } else if (pattern == "*.json" &&
                                   filename.find(".json") != std::string::npos) {
                            files.push_back(entry.path().string());
                        } else if (pattern == "*.txt" &&
                                   filename.find(".txt") != std::string::npos) {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            } catch (const std::exception& e) {
                setLastError(std::string("Error reading directory: ") + e.what());
            }

            return files;
        }

        bool DiagnosticsCollectorMacOS::copyFile(const std::string& source,
                                                 const std::string& destination) {
            try {
                std::filesystem::create_directories(
                    std::filesystem::path(destination).parent_path());
                std::filesystem::copy_file(source, destination,
                                           std::filesystem::copy_options::overwrite_existing);
                return true;
            } catch (const std::exception& e) {
                setLastError(std::string("Failed to copy file: ") + e.what());
                return false;
            }
        }

    }  // namespace diag
}  // namespace seventeen
