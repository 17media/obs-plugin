#include <cstdio>
#include <string>

#include "17live/core/CoreRuntime.hpp"
#include "17live/core/WsMessageQueue.hpp"
#include "17live/utility/Result.hpp"
#include "17live/websocket/WsMessage.hpp"

static int g_failures = 0;

static void check(bool ok, const char* expr, const char* file, int line) {
    if (ok) {
        return;
    }
    std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", expr, file, line);
    g_failures++;
}

#define CHECK(x) check((x), #x, __FILE__, __LINE__)

static void test_wsmessage_parse_dump() {
    WsMessage m;
    CHECK(WsMessage::parse(R"({"type":"action","payload":{"type":"register_chatdock","client":"x"}})", m));
    CHECK(m.is(ws::TypeAction));
    CHECK(m.payloadString("type") == ws::ActionRegisterChatDock);
    const std::string s = m.dump();
    WsMessage m2;
    CHECK(WsMessage::parse(s, m2));
    CHECK(m2.type == "action");
    CHECK(m2.payload.contains("client"));

    WsMessage m3;
    CHECK(WsMessage::parse(R"({"type":"action","payload":"not_object"})", m3));
    CHECK(m3.payload.is_object());
    CHECK(m3.payload.empty());

    WsMessage m4;
    CHECK(!WsMessage::parse("{", m4));
}

static void test_core_runtime_idempotency() {
    bool initialized = false;
    bool shuttingDown = false;
    bool cancel = false;

    int initLocalServersCalls = 0;
    int initConfigAndApiCalls = 0;
    int initMenuCalls = 0;
    int restoreCalls = 0;

    int stopStreamingCalls = 0;
    int saveCloseCalls = 0;
    int shutdownRtmpChatCalls = 0;
    int shutdownServersCalls = 0;
    int cleanupCalls = 0;

    CoreRuntime::State state;
    state.initialized = &initialized;
    state.shuttingDown = &shuttingDown;
    state.setShutdownCancel = [&](bool v) { cancel = v; };

    CoreRuntime::Hooks hooks;
    hooks.initLocalServers = [&]() {
        initLocalServersCalls++;
        return true;
    };
    hooks.initConfigAndApi = [&]() {
        initConfigAndApiCalls++;
        return true;
    };
    hooks.initAuthHandlers = [&]() {};
    hooks.initMenuAndBaseUI = [&]() {
        initMenuCalls++;
        return true;
    };
    hooks.restoreRuntimeStateIfNeeded = [&]() { restoreCalls++; };

    hooks.stopStreamingSafely = [&]() { stopStreamingCalls++; };
    hooks.saveAndCloseUI = [&]() { saveCloseCalls++; };
    hooks.shutdownRtmpAndChat = [&]() { shutdownRtmpChatCalls++; };
    hooks.shutdownLocalServers = [&]() { shutdownServersCalls++; };
    hooks.cleanupTimersAndFlags = [&]() { cleanupCalls++; };

    CoreRuntime rt(state, hooks);

    CHECK(rt.initialize());
    CHECK(initialized);
    CHECK(!shuttingDown);
    CHECK(!cancel);
    CHECK(initLocalServersCalls == 1);
    CHECK(initConfigAndApiCalls == 1);
    CHECK(initMenuCalls == 1);
    CHECK(restoreCalls == 1);

    CHECK(rt.initialize());
    CHECK(initLocalServersCalls == 1);
    CHECK(initConfigAndApiCalls == 1);
    CHECK(initMenuCalls == 1);
    CHECK(restoreCalls == 1);

    rt.shutdown();
    CHECK(!initialized);
    CHECK(!shuttingDown);
    CHECK(cancel);
    CHECK(stopStreamingCalls == 1);
    CHECK(saveCloseCalls == 1);
    CHECK(shutdownRtmpChatCalls == 1);
    CHECK(shutdownServersCalls == 1);
    CHECK(cleanupCalls == 1);

    rt.shutdown();
    CHECK(stopStreamingCalls == 1);
}

static void test_result_semantics() {
    {
        auto r = Result<int>::Ok(42);
        CHECK(r.ok());
        CHECK(static_cast<bool>(r));
        CHECK(r.value() == 42);
    }
    {
        ResultError err;
        err.code = "X";
        err.message = "Y";
        auto r = Result<int>::Err(err);
        CHECK(!r.ok());
        CHECK(!static_cast<bool>(r));
        CHECK(r.error().code == "X");
        CHECK(r.error().message == "Y");
    }
    {
        auto r = Result<void>::Ok();
        CHECK(r.ok());
    }
}

static void test_wsmessage_queue_trim_and_order() {
    WsMessageQueue q(2);
    q.enqueue(WsMessage{"a", nlohmann::json::object({{"i", 1}})});
    q.enqueue(WsMessage{"b", nlohmann::json::object({{"i", 2}})});
    q.enqueue(WsMessage{"c", nlohmann::json::object({{"i", 3}})});
    CHECK(q.size() == 2);
    CHECK(q.front().type == "b");
    q.popFront();
    CHECK(q.front().type == "c");
}

int main() {
    test_wsmessage_parse_dump();
    test_core_runtime_idempotency();
    test_result_semantics();
    test_wsmessage_queue_trim_and_order();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d tests failed\n", g_failures);
        return 1;
    }
    std::printf("All tests passed\n");
    return 0;
}

