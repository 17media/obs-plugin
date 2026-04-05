#include "CoreRuntime.hpp"

CoreRuntime::CoreRuntime(State state, Hooks hooks) : state_(state), hooks_(std::move(hooks)) {}

bool CoreRuntime::initialize() {
    if (!state_.initialized || !state_.cancelFlag) {
        return false;
    }
    if (*state_.initialized) {
        return true;
    }

    state_.cancelFlag->store(false);

    if (!hooks_.initLocalServers || !hooks_.initConfigAndApi || !hooks_.initMenuAndBaseUI ||
        !hooks_.initAuthHandlers || !hooks_.restoreRuntimeStateIfNeeded) {
        return false;
    }

    if (!hooks_.initLocalServers()) {
        return false;
    }
    if (!hooks_.initConfigAndApi()) {
        return false;
    }

    hooks_.initAuthHandlers();

    if (!hooks_.initMenuAndBaseUI()) {
        return false;
    }

    hooks_.restoreRuntimeStateIfNeeded();
    *state_.initialized = true;
    return true;
}

void CoreRuntime::shutdown() {
    if (!state_.initialized || !state_.shuttingDown || !state_.cancelFlag) {
        return;
    }

    state_.cancelFlag->store(true);
    if (*state_.shuttingDown || !*state_.initialized) {
        return;
    }
    *state_.shuttingDown = true;

    if (hooks_.stopStreamingSafely) {
        hooks_.stopStreamingSafely();
    }
    if (hooks_.saveAndCloseUI) {
        hooks_.saveAndCloseUI();
    }
    if (hooks_.shutdownRtmpAndChat) {
        hooks_.shutdownRtmpAndChat();
    }
    if (hooks_.shutdownLocalServers) {
        hooks_.shutdownLocalServers();
    }
    if (hooks_.cleanupTimersAndFlags) {
        hooks_.cleanupTimersAndFlags();
    }

    *state_.initialized = false;
    *state_.shuttingDown = false;
}
