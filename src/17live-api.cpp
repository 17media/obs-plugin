#include <obs-module.h>
#include "17live-api.hpp"

// Basic implementation of the 17LIVE API interface

bool SeventeenLiveAPI::Initialize()
{
    blog(LOG_INFO, "Initializing 17LIVE API");
    return true;
}

void SeventeenLiveAPI::Shutdown()
{
    blog(LOG_INFO, "Shutting down 17LIVE API");
}

bool SeventeenLiveAPI::Connect(const std::string& streamKey)
{
    blog(LOG_INFO, "Connecting to 17LIVE with stream key: %s", streamKey.c_str());
    return true;
}

void SeventeenLiveAPI::Disconnect()
{
    blog(LOG_INFO, "Disconnecting from 17LIVE");
}

bool SeventeenLiveAPI::IsConnected() const
{
    return false; // Dummy implementation
}

bool SeventeenLiveAPI::SendMessage(const std::string& message)
{
    blog(LOG_INFO, "Sending message to 17LIVE: %s", message.c_str());
    return true;
}