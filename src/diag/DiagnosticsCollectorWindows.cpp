#include "DiagnosticsCollectorWindows.hpp"

#include <windows.h>

#include <filesystem>

namespace seventeen {
    namespace diag {

        DiagnosticsCollectorWindows::DiagnosticsCollectorWindows() {}

        bool DiagnosticsCollectorWindows::isSupported() const {
#ifdef _WIN32
            return true;
#else
            return false;
#endif
        }

        std::string DiagnosticsCollectorWindows::getSystemInfoImpl() const {
            std::stringstream ss;

            ss << "Windows Version: "
               << executePowerShellCommand("(Get-CimInstance Win32_OperatingSystem).Caption")
               << std::endl;
            ss << "Build Number: "
               << executePowerShellCommand("(Get-CimInstance Win32_OperatingSystem).BuildNumber")
               << std::endl;
            ss << "Architecture: "
               << executePowerShellCommand("(Get-CimInstance Win32_OperatingSystem).OSArchitecture")
               << std::endl;

            ss << "\nHardware Information:" << std::endl;
            ss << "CPU: " << executePowerShellCommand("(Get-CimInstance Win32_Processor).Name")
               << std::endl;
            ss << "Memory: "
               << executePowerShellCommand(
                      "(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory")
               << " bytes" << std::endl;
            ss << "Manufacturer: "
               << executePowerShellCommand("(Get-CimInstance Win32_ComputerSystem).Manufacturer")
               << std::endl;
            ss << "Model: "
               << executePowerShellCommand("(Get-CimInstance Win32_ComputerSystem).Model")
               << std::endl;

            ss << "\nGraphics Information:" << std::endl;
            ss << executePowerShellCommand(
                      "Get-CimInstance Win32_VideoController | Select-Object Name, AdapterRAM, "
                      "DriverVersion | Format-Table -AutoSize")
               << std::endl;

            ss << "\nDisk Information:" << std::endl;
            ss << executePowerShellCommand(
                      "Get-CimInstance Win32_LogicalDisk | Select-Object DeviceID, Size, "
                      "FreeSpace, FileSystem | Format-Table -AutoSize")
               << std::endl;

            return ss.str();
        }

        std::vector<std::string> DiagnosticsCollectorWindows::collectOBSLogs() {
            std::vector<std::string> logFiles;
            std::string obsLogDir = getOBSLogDirectory();

            if (std::filesystem::exists(obsLogDir)) {
                auto files = getFilesInDirectory(obsLogDir, ".txt");
                std::sort(files.begin(), files.end(),
                          [](const std::string& a, const std::string& b) {
                              return std::filesystem::last_write_time(a) >
                                     std::filesystem::last_write_time(b);
                          });
                if (files.size() > 5)
                    files.resize(5);
                std::string tempDir = generateTempDirectory();

                for (const auto& file : files) {
                    std::string fileName = std::filesystem::path(file).filename().string();
                    std::string destPath =
                        (std::filesystem::path(tempDir) / ("obs_" + fileName)).string();
                    if (copyWithSizeLimit(file, destPath)) {
                        logFiles.push_back(destPath);
                    }
                }
            }

            return logFiles;
        }

        std::vector<std::string> DiagnosticsCollectorWindows::collectPluginLogs() {
            std::vector<std::string> logFiles;
            std::string pluginLogDir = getPluginLogDirectory();

            if (std::filesystem::exists(pluginLogDir)) {
                auto files = getFilesInDirectory(pluginLogDir, ".log");
                std::sort(files.begin(), files.end(),
                          [](const std::string& a, const std::string& b) {
                              return std::filesystem::last_write_time(a) >
                                     std::filesystem::last_write_time(b);
                          });
                if (files.size() > 5)
                    files.resize(5);
                std::string tempDir = generateTempDirectory();

                for (const auto& file : files) {
                    std::string fileName = std::filesystem::path(file).filename().string();
                    std::string destPath =
                        (std::filesystem::path(tempDir) / ("plugin_" + fileName)).string();
                    if (copyWithSizeLimit(file, destPath)) {
                        logFiles.push_back(destPath);
                    }
                }
            }

            return logFiles;
        }

        std::vector<std::string> DiagnosticsCollectorWindows::collectNetworkLogs() {
            std::vector<std::string> networkLogs;

            std::string tempDir = generateTempDirectory();
            std::string networkInfoPath =
                (std::filesystem::path(tempDir) / "network_info.txt").string();

            std::stringstream ss;
            ss << "Network Interfaces:" << std::endl;
            ss << executePowerShellCommand(
                      "Get-CimInstance Win32_NetworkAdapterConfiguration | Where-Object "
                      "{$_.IPEnabled -eq $true} | Select-Object Description, IPAddress, MACAddress "
                      "| Format-Table -AutoSize")
               << std::endl;
            ss << "\nNetwork Statistics:" << std::endl;
            ss << executePowerShellCommand(
                      "Get-CimInstance Win32_PerfRawData_Tcpip_NetworkInterface | Select-Object "
                      "Name, BytesReceivedPersec, BytesSentPersec | Format-Table -AutoSize")
               << std::endl;

            if (writeToFile(networkInfoPath, ss.str())) {
                networkLogs.push_back(networkInfoPath);
            }

            return networkLogs;
        }

        std::vector<std::string> DiagnosticsCollectorWindows::collectCrashInfo() {
            std::vector<std::string> crashFiles;
            std::string crashDir = getCrashDumpDirectory();

            if (std::filesystem::exists(crashDir)) {
                auto files = getFilesInDirectory(crashDir, ".dmp");
                // Filter to obs*.dmp only
                files.erase(std::remove_if(files.begin(), files.end(),
                                           [](const std::string& path) {
                                               std::string name =
                                                   std::filesystem::path(path).filename().string();
                                               return name.rfind("obs", 0) !=
                                                      0;  // keep names starting with 'obs'
                                           }),
                            files.end());
                std::sort(files.begin(), files.end(),
                          [](const std::string& a, const std::string& b) {
                              return std::filesystem::last_write_time(a) >
                                     std::filesystem::last_write_time(b);
                          });
                if (files.size() > 3)
                    files.resize(3);
                std::string tempDir = generateTempDirectory();

                for (const auto& file : files) {
                    std::string fileName = std::filesystem::path(file).filename().string();
                    std::string destPath =
                        (std::filesystem::path(tempDir) / ("crash_" + fileName)).string();
                    if (copyWithSizeLimit(file, destPath)) {
                        crashFiles.push_back(destPath);
                    }
                }
            }

            std::string tempDir = generateTempDirectory();
            std::string eventLogPath =
                (std::filesystem::path(tempDir) / "application_events.txt").string();

            std::stringstream ss;
            ss << "Application Error Events:" << std::endl;
            ss << executePowerShellCommand(
                      "Get-EventLog -LogName Application -EntryType Error -Newest 50 | "
                      "Select-Object TimeGenerated, Source, Message | Format-Table -AutoSize")
               << std::endl;

            if (writeToFile(eventLogPath, ss.str())) {
                crashFiles.push_back(eventLogPath);
            }

            return crashFiles;
        }

        std::vector<std::string> DiagnosticsCollectorWindows::collectConfigSnapshot() {
            std::vector<std::string> configFiles;
            std::string appDataPath = getAppDataPath();

            std::string obsConfigDir = appDataPath + "\\obs-studio";
            std::string pluginConfigDir = appDataPath + "\\17live-obs-plugin";

            std::string tempDir = generateTempDirectory();

            if (std::filesystem::exists(obsConfigDir)) {
                auto globalIni = obsConfigDir + "\\global.ini";
                if (std::filesystem::exists(globalIni)) {
                    std::string destPath =
                        (std::filesystem::path(tempDir) / "obs_global.ini").string();
                    if (copyFile(globalIni, destPath)) {
                        configFiles.push_back(destPath);
                    }
                }

                auto basicIni = obsConfigDir + "\\basic.ini";
                if (std::filesystem::exists(basicIni)) {
                    std::string destPath =
                        (std::filesystem::path(tempDir) / "obs_basic.ini").string();
                    if (copyFile(basicIni, destPath)) {
                        configFiles.push_back(destPath);
                    }
                }
            }

            if (std::filesystem::exists(pluginConfigDir)) {
                auto pluginFiles = getFilesInDirectory(pluginConfigDir, ".json");
                for (const auto& file : pluginFiles) {
                    std::string fileName = std::filesystem::path(file).filename().string();
                    std::string destPath =
                        (std::filesystem::path(tempDir) / ("plugin_" + fileName)).string();

                    if (copyFile(file, destPath)) {
                        configFiles.push_back(destPath);
                    }
                }
            }

            return configFiles;
        }

        std::vector<std::string> DiagnosticsCollectorWindows::collectNetworkRequests() {
            std::vector<std::string> requestFiles;

            std::string tempDir = generateTempDirectory();
            std::string requestsPath =
                (std::filesystem::path(tempDir) / "network_requests.txt").string();

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

        bool DiagnosticsCollectorWindows::createZipArchive(const std::string& outputPath,
                                                           const std::vector<std::string>& files) {
            if (files.empty()) {
                return false;
            }

            // Create staging directory and categorize similar to macOS
            std::string stagingDir = generateTempDirectory();
            if (stagingDir.empty()) {
                setLastError("Failed to create staging directory");
                return false;
            }

            auto determineCategory = [](const std::string& path) -> std::string {
                std::string name = std::filesystem::path(path).filename().string();
                if (name.rfind("obs_", 0) == 0 && name.find(".txt") != std::string::npos) {
                    return "obs_logs";
                }
                if (name.rfind("plugin_", 0) == 0 && name.find(".log") != std::string::npos) {
                    return "plugin_logs";
                }
                if (name.rfind("crash_", 0) == 0) {
                    return "crash_reports";
                }
                if (name == "systeminfo.txt") {
                    return "ROOT";
                }
                if (name == "network_requests.txt") {
                    return "Network requests";
                }
                return "Misc";
            };

            try {
                for (const auto& file : files) {
                    if (!std::filesystem::exists(file))
                        continue;
                    std::string category = determineCategory(file);
                    std::filesystem::path destPath;
                    if (category == "ROOT") {
                        destPath = std::filesystem::path(stagingDir) /
                                   std::filesystem::path(file).filename();
                        std::filesystem::create_directories(std::filesystem::path(stagingDir));
                    } else {
                        std::filesystem::path categoryDir =
                            std::filesystem::path(stagingDir) / category;
                        std::filesystem::create_directories(categoryDir);
                        destPath = categoryDir / std::filesystem::path(file).filename();
                    }
                    std::filesystem::copy_file(file, destPath,
                                               std::filesystem::copy_options::overwrite_existing);
                }
            } catch (const std::exception& e) {
                setLastError(std::string("Failed to prepare staging files: ") + e.what());
                return false;
            }

            // Compress the staging directory
            std::string zipCommand = "Compress-Archive -Path \"" + stagingDir +
                                     "\" -DestinationPath \"" + outputPath + "\" -Force";
            std::string result = executePowerShellCommand(zipCommand);
            return std::filesystem::exists(outputPath) &&
                   std::filesystem::file_size(outputPath) > 0;
        }

        std::string DiagnosticsCollectorWindows::executePowerShellCommand(
            const std::string& command) const {
            std::string fullCommand = "powershell -Command \"" + command + "\"";

            FILE* pipe = _popen(fullCommand.c_str(), "r");
            if (!pipe) {
                return "";
            }

            char buffer[128];
            std::string result;

            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }

            _pclose(pipe);
            return result;
        }

        std::string DiagnosticsCollectorWindows::getAppDataPath() const {
            char* appData = nullptr;
            size_t len = 0;

            if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData != nullptr) {
                std::string result(appData);
                free(appData);
                return result;
            }

            return "";
        }

        std::string DiagnosticsCollectorWindows::getOBSLogDirectory() const {
            return getAppDataPath() + "\\obs-studio\\logs";
        }

        std::string DiagnosticsCollectorWindows::getPluginLogDirectory() const {
            std::string path = getAppDataPath() + "\\obs-studio\\plugin_config\\17live\\logs";
            if (std::filesystem::exists(path))
                return path;
            path = getAppDataPath() + "\\obs-studio\\plugin_config\\obs-17live\\logs";
            return path;
        }

        std::string DiagnosticsCollectorWindows::getCrashDumpDirectory() const {
            char* localAppData = nullptr;
            size_t len = 0;
            if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") == 0 && localAppData != nullptr) {
                std::string result(localAppData);
                free(localAppData);
                return result + "\\CrashDumps";
            }
            return std::string();
        }

        std::vector<std::string> DiagnosticsCollectorWindows::getFilesInDirectory(
            const std::string& directory, const std::string& extension) {
            std::vector<std::string> files;

            if (!std::filesystem::exists(directory)) {
                return files;
            }

            try {
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_regular_file() && entry.path().extension() == extension) {
                        files.push_back(entry.path().string());
                    }
                }
            } catch (const std::exception& e) {
                setLastError(std::string("Error reading directory: ") + e.what());
            }

            return files;
        }

        bool DiagnosticsCollectorWindows::copyFile(const std::string& source,
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
