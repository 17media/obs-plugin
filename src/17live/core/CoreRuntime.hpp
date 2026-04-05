#pragma once

#include <atomic>
#include <functional>

class CoreRuntime {
   public:
    struct State {
        bool* initialized = nullptr;
        bool* shuttingDown = nullptr;
        std::atomic<bool>* cancelFlag = nullptr;
    };

    struct Hooks {
        std::function<bool()> initLocalServers;
        std::function<bool()> initConfigAndApi;
        std::function<void()> initAuthHandlers;
        std::function<bool()> initMenuAndBaseUI;
        std::function<void()> restoreRuntimeStateIfNeeded;
        std::function<void()> stopStreamingSafely;
        std::function<void()> saveAndCloseUI;
        std::function<void()> shutdownRtmpAndChat;
        std::function<void()> shutdownLocalServers;
        std::function<void()> cleanupTimersAndFlags;
    };

    CoreRuntime(State state, Hooks hooks);

    bool initialize();
    void shutdown();

   private:
    State state_;
    Hooks hooks_;
};
