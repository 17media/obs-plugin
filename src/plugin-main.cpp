#include <obs-module.h>
#include "17live-stream-settings.hpp"
#include "17live-chat-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-17live", "en-US")

module_t *module;

bool obs_module_load(void)
{
    blog(LOG_INFO, "17LIVE plugin loaded successfully (version %s)",
         PLUGIN_VERSION);

    // Register the stream settings dialog
    SeventeenLiveStreamSettings::Register();

    // Register the chat dock
    SeventeenLiveChatDock::Register();

    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "17LIVE plugin unloaded");
}

module_t *obs_get_module(void)
{
    return module;
}