#pragma once

struct OneSevenLivePropertyRefreshHandler {
    virtual ~OneSevenLivePropertyRefreshHandler() = default;
    virtual void RefreshUI() = 0;
};
