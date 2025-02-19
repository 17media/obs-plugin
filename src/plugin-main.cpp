#include <obs-module.h>
#include <obs-frontend-api.h> // Include the header for frontend API
#include "17live-stream-settings.hpp"

static obs_module_t *module;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-17live", "en-US")

const char* PLUGIN_VERSION = "1.0.0";

obs_module_t *obs_get_module(void)
{
    return module;
}

bool obs_module_load(void)
{
    module = obs_current_module();
    blog(LOG_INFO, "17LIVE plugin loaded successfully (version %s)",
         PLUGIN_VERSION);

    // Register the stream settings dialog
    SeventeenLiveStreamSettings::Register();

    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "17LIVE plugin unloaded");
}
