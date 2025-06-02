#include "Common.hpp"

#include <obs.h>

#include <cstdio> // For popen, pclose, fgets
#include <array>  // For std::array
#include <string> // For std::string

#ifdef _WIN32
#include <io.h>
#include <process.h>
#endif

#ifdef __linux__
#include <fstream> // For std::ifstream (Linux)
#include <sstream> // For std::stringstream (Linux)
#endif

std::string GetCurrentLanguage() {
  const char *locale = obs_get_locale();
  if (strcmp(locale, "ja-JP") == 0) {
    return "JP";
  } else if (strcmp(locale, "zh-CN") == 0 || strcmp(locale, "zh-TW") == 0) {
    return "TW";
  } else {
    return "US";
  }
}

std::string GetCurrentLocale() {
  const char *locale = obs_get_locale();
  if (strcmp(locale, "ja-JP") == 0) {
    return "ja";
  } else if (strcmp(locale, "zh-CN") == 0 || strcmp(locale, "zh-TW") == 0) {
    return "zh";
  } else {
    return "en";
  }
}

std::string GetCurrentOS() {
#ifdef _WIN32
  return OS_WINDOWS;
#elif defined(__APPLE__)
  return OS_MAC;
#elif defined(__linux__)
  return OS_LINUX;
#else
  return OS_UNKNOWN;
#endif
}

std::string ExecuteCommandAndGetOutput(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  
#ifdef _WIN32
  // Windows平台使用_popen和_pclose
  FILE* pipe = _popen(cmd, "r");
#else
  // macOS和Linux平台使用popen和pclose
  FILE* pipe = popen(cmd, "r");
#endif
  
  if (!pipe) {
      return "Error executing command";
  }
  
#ifdef _WIN32
while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
#else
while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
#endif
    result += buffer.data();
}

#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  
  // Remove trailing newline characters if any
  if (!result.empty() && result[result.length()-1] == '\n') {
      result.erase(result.length()-1);
  }
  if (!result.empty() && result[result.length()-1] == '\r') {
      result.erase(result.length()-1);
  }
  
  return result;
}

std::string GetCurrentOSVersion() {
  std::string version = "Unknown";

#if defined(__APPLE__)  // For macOS, use sw_vers command
  version = ExecuteCommandAndGetOutput("sw_vers -productVersion");
#elif defined(_WIN32)  // For Windows, use 'ver' command
  // For Windows, use 'ver' command. 
  // A more robust way would be to use Windows API like GetVersionEx, 
  // but 'ver' is simpler for this example.
  // The output of 'ver' might need parsing.
  std::string verOutput = ExecuteCommandAndGetOutput("ver");
  // Example parsing: Microsoft Windows [Version 10.0.19042.985]
  // We might want to extract just "10.0.19042.985"
  size_t pos = verOutput.find("[");
  if (pos != std::string::npos) {
      size_t endPos = verOutput.find("]", pos);
      if (endPos != std::string::npos) {
          std::string extracted = verOutput.substr(pos + 1, endPos - pos - 1);
          // Remove "Version " prefix if present
          if (extracted.rfind("Version ", 0) == 0) { 
              version = extracted.substr(8);
          } else {
              version = extracted;
          }
      } else {
          version = verOutput; // Fallback to full output
      }
  } else {
      version = verOutput; // Fallback to full output
  }
#elif defined(__linux__)  // For Linux, use uname -r
  // For Linux, try reading /etc/os-release or use uname
  std::ifstream osReleaseFile("/etc/os-release");
  if (osReleaseFile.is_open()) {
      std::string line;
      while (std::getline(osReleaseFile, line)) {
          if (line.rfind("PRETTY_NAME=", 0) == 0) {
              version = line.substr(13);
              // Remove quotes if present
              if (!version.empty() && version.front() == '"' && version.back() == '"') {
                  version = version.substr(1, version.length() - 2);
              }
              break;
          } else if (line.rfind("VERSION_ID=", 0) == 0 && version == "Unknown") {
                // Fallback to VERSION_ID if PRETTY_NAME not found or preferred
              std::string tempVersion = line.substr(11);
              if (!tempVersion.empty() && tempVersion.front() == '"' && tempVersion.back() == '"') {
                  tempVersion = tempVersion.substr(1, tempVersion.length() - 2);
              }
              version = tempVersion;
          }
      }
      osReleaseFile.close();
  }
  if (version == "Unknown" || version.empty()) {
      // Fallback to uname -r if /etc/os-release doesn't give a good version
      version = ExecuteCommandAndGetOutput("uname -r");
  }
#endif

  return version;
}

std::string GetCurrentPlatformUUID() {
#if defined(__APPLE__)
  return ExecuteCommandAndGetOutput("ioreg -d2 -c IOPlatformExpertDevice | awk -F\\\" '/IOPlatformUUID/{print $(NF-1)}'");
#elif defined(_WIN32)
  // Windows UUID retrieval is more complex and often requires WMI or registry access.
  // For example, using WMIC (might require admin privileges and parsing):
  // return ExecuteCommandAndGetOutput("wmic csproduct get uuid");
  // This will likely return a header and then the UUID on a new line.
  // Proper parsing would be needed.
  return "Windows UUID Not Implemented Yet";
#elif defined(__linux__)
  // Linux can use /sys/class/dmi/id/product_uuid or dmidecode
  // std::ifstream uuidFile("/sys/class/dmi/id/product_uuid");
  // if (uuidFile.is_open()) {
  //     std::string uuid;
  //     std::getline(uuidFile, uuid);
  //     uuidFile.close();
  //     // Remove trailing newline if any
  //     if (!uuid.empty() && uuid[uuid.length()-1] == '\n') {
  //         uuid.erase(uuid.length()-1);
  //     }
  //     return uuid;
  // }
  // return ExecuteCommandAndGetOutput("cat /sys/class/dmi/id/product_uuid"); // Simpler, but ExecuteCommandAndGetOutput handles newline
  return "Linux UUID Not Implemented Yet";
#else
  return "Unsupported OS for UUID";
#endif
}
