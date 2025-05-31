#pragma once

#include <string>

#define OS_WINDOWS "Windows"
#define OS_MAC "macOS"
#define OS_LINUX "Linux"
#define OS_UNKNOWN "Unknown"

std::string GetCurrentOS();
std::string GetCurrentOSVersion();
std::string GetCurrentPlatformUUID();
std::string GetCurrentLanguage();
std::string GetCurrentLocale();

