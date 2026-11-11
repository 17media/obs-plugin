#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "17live/api/OneSevenLiveAblyProtocol.hpp"
#include "17live/core/CoreRuntime.hpp"
#include "17live/core/WsMessageQueue.hpp"
#include "17live/utility/Result.hpp"
#include "17live/websocket/OneSevenLiveWebsocketMessageAssembler.hpp"
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

static void test_wsmessage_queue_drains_only_available_targets() {
    WsMessageQueue q(10);
    q.enqueue(WsMessage{"chat", {{"i", 1}}});
    q.enqueue(WsMessage{"animation", {{"i", 2}}});
    q.enqueue(WsMessage{"chat", {{"i", 3}}});
    q.enqueue(WsMessage{"animation", {{"i", 4}}});

    std::vector<int> delivered;
    auto deliver = [&delivered](const WsMessage& m) {
        delivered.push_back(m.payload.at("i").get<int>());
    };
    q.drainIf([](const WsMessage& m) { return m.type == "animation"; }, deliver);
    CHECK(delivered == std::vector<int>({2, 4}));
    CHECK(q.size() == 2);
    CHECK(q.front().payload.at("i") == 1);

    q.drainIf([](const WsMessage&) { return false; }, deliver);
    CHECK(q.size() == 2);
    q.drainIf([](const WsMessage& m) { return m.type == "chat"; }, deliver);
    CHECK(delivered == std::vector<int>({2, 4, 1, 3}));
    CHECK(q.empty());
}

static void test_websocket_message_assembler() {
    using Assembler = OneSevenLiveWebsocketMessageAssembler;
    using Result = Assembler::Result;
    Assembler assembler(8);
    std::string message;

    CHECK(assembler.append("hello", true, message) == Result::Complete);
    CHECK(message == "hello");
    CHECK(!assembler.hasIncompleteMessage());

    CHECK(assembler.append("ab", false, message) == Result::Incomplete);
    CHECK(assembler.hasIncompleteMessage());
    CHECK(assembler.append("cd", false, message) == Result::Incomplete);
    CHECK(assembler.append("ef", true, message) == Result::Complete);
    CHECK(message == "abcdef");
    CHECK(!assembler.hasIncompleteMessage());

    CHECK(assembler.append("", false, message) == Result::Incomplete);
    CHECK(assembler.hasIncompleteMessage());
    CHECK(assembler.append("", true, message) == Result::Complete);
    CHECK(message.empty());

    CHECK(assembler.append("1234", false, message) == Result::Incomplete);
    CHECK(assembler.append("5678", true, message) == Result::Complete);
    CHECK(message == "12345678");
    CHECK(assembler.append("12345678", false, message) == Result::Incomplete);
    CHECK(assembler.append("9", true, message) == Result::TooLarge);
    CHECK(!assembler.hasIncompleteMessage());
    CHECK(assembler.append("fresh", true, message) == Result::Complete);
    CHECK(message == "fresh");
    CHECK(assembler.append("123456789", true, message) == Result::TooLarge);

    Assembler largeAssembler(131072);
    const std::string first(65536, 'a');
    CHECK(largeAssembler.append(first, false, message) == Result::Incomplete);
    CHECK(largeAssembler.append("end", true, message) == Result::Complete);
    CHECK(message == first + "end");
}

static void test_ably_channel_recovery() {
    ably::ChannelRecovery recovery;
    auto attach = recovery.attach("room");
    CHECK(attach["action"] == 10);
    CHECK(attach["channel"] == "room");
    CHECK(!attach.contains("channelSerial"));
    CHECK(!attach.contains("flags"));

    CHECK(!recovery.attached(nlohmann::json{{"channelSerial", "attach-1"}}));
    recovery.updateSerial(nlohmann::json{{"channelSerial", "message-2"}});
    recovery.updateSerial(nlohmann::json::object());
    attach = recovery.attach("room");
    CHECK(attach["channelSerial"] == "message-2");
    CHECK(attach["flags"] == 32);
    CHECK(!recovery.attached(nlohmann::json{{"flags", 4}, {"channelSerial", "message-3"}}));
    CHECK(recovery.attach("room")["channelSerial"] == "message-3");
    CHECK(recovery.attached(nlohmann::json{{"flags", 0}}));
    recovery.reset();
    CHECK(!recovery.attach("other").contains("channelSerial"));
    CHECK(!recovery.attach("other").contains("flags"));
}

static void test_ably_error_classification() {
    CHECK(ably::classifyError(40140, 401) == ably::ErrorKind::Token);
    CHECK(ably::classifyError(40149, 401) == ably::ErrorKind::Token);
    CHECK(ably::classifyError(40160, 401) == ably::ErrorKind::Permission);
    CHECK(ably::classifyError(40300, 403) == ably::ErrorKind::Permission);
    CHECK(ably::classifyError(50000, 500) == ably::ErrorKind::Other);
}

static void test_ably_idle_timeout() {
    CHECK(ably::idleTimeoutMs(nlohmann::json::object()) == 0);
    CHECK(ably::idleTimeoutMs(nullptr) == 0);
    CHECK(ably::idleTimeoutMs({{"maxIdleInterval", "15000"}}) == 0);
    CHECK(ably::idleTimeoutMs({{"maxIdleInterval", 0}}) == 0);
    CHECK(ably::idleTimeoutMs({{"maxIdleInterval", -1}}) == 0);
    CHECK(ably::idleTimeoutMs({{"maxIdleInterval", 15000}}) == 25000);
    CHECK(ably::idleTimeoutMs({{"maxIdleInterval", std::numeric_limits<int>::max() - 10000}}) ==
          std::numeric_limits<int>::max());
    CHECK(ably::idleTimeoutMs({{"maxIdleInterval", std::numeric_limits<int>::max()}}) == 0);
    CHECK(ably::idleTimeoutMs({{"maxIdleInterval", std::numeric_limits<std::uint64_t>::max()}}) ==
          0);
}

static void test_ably_auth_update() {
    CHECK(ably::isAuthUpdate(true, "old", "old"));
    CHECK(ably::isAuthUpdate(true, "old", ""));
    CHECK(!ably::isAuthUpdate(true, "old", "new"));
    CHECK(!ably::isAuthUpdate(false, "old", "old"));
}

int main() {
    test_wsmessage_parse_dump();
    test_core_runtime_idempotency();
    test_result_semantics();
    test_wsmessage_queue_trim_and_order();
    test_wsmessage_queue_drains_only_available_targets();
    test_websocket_message_assembler();
    test_ably_channel_recovery();
    test_ably_error_classification();
    test_ably_idle_timeout();
    test_ably_auth_update();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d tests failed\n", g_failures);
        return 1;
    }
    std::printf("All tests passed\n");
    return 0;
}
