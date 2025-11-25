#pragma once

#include <obs.h>

#include <nlohmann/json.hpp>
#include <string>
#include <memory>

#define OS_WINDOWS "Windows"
#define OSL_OS_MAC "macOS"
#define OS_LINUX "Linux"
#define OS_UNKNOWN "Unknown"

std::string GetCurrentOS();
std::string GetCurrentOSVersion();
std::string GetCurrentPlatformUUID();
std::string GetCurrentLanguage();
std::string GetCurrentLocale();

obs_data_t* ObsDataFromJson(nlohmann::json j);

// OBS module data path helper
std::string get_obs_module_data_path_str();
struct obs_data_deleter {
    void operator()(obs_data_t* p) const { if (p) obs_data_release(p); }
};
using ObsDataPtr = std::unique_ptr<obs_data_t, obs_data_deleter>;
