#pragma once

#include <obs.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#define OS_WINDOWS "Windows"
#define OSL_OS_MAC "macOS"
#define OS_LINUX "Linux"
#define OS_UNKNOWN "Unknown"

#include <functional>

// ... existing includes ...

std::string GetCurrentOS();
std::string GetCurrentOSVersion();
std::string GetCurrentPlatformUUID();
std::string GetCurrentLanguage();
std::string GetCurrentLocale();

obs_data_t* ObsDataFromJson(nlohmann::json j);

// OBS module data path helper
std::string get_obs_module_data_path_str();

// Schedule a task on the OBS task thread
void ScheduleOBSTask(std::function<void()> task);

void InitThreadPool();
void DestroyThreadPool();

struct obs_data_deleter {
    void operator()(obs_data_t* p) const {
        if (p)
            obs_data_release(p);
    }
};

using ObsDataPtr = std::unique_ptr<obs_data_t, obs_data_deleter>;
