#include "OneSevenLiveAblyChatClient.hpp"

#include <QMetaObject>
#include <QUrl>
#include <obs-module.h>
#include <QTimer>

OneSevenLiveAblyChatClient::OneSevenLiveAblyChatClient(QObject* parent)
    : QObject(parent), m_wsClient(std::make_unique<OneSevenLiveWebsocketClient>()) {
    m_hosts = {
        "wss://17-media-a-fallback.ably-realtime.com",
        "wss://17-media-b-fallback.ably-realtime.com",
        "wss://17-media-c-fallback.ably-realtime.com",
    };

    m_wsClient->setOpenCallback([this]() {
        if (m_onOpen) QMetaObject::invokeMethod(this, [this]() { m_onOpen(); }, Qt::QueuedConnection);
        m_reconnectAttempts = 0;
    });
    m_wsClient->setMessageCallback([this](const std::string& msg) {
        // Parse Ably protocol message and attach after CONNECTED
        try {
            nlohmann::json j = nlohmann::json::parse(msg);
            if (j.contains("action")) {
                int action = -1;
                if (j["action"].is_number_integer()) action = j["action"].get<int>();
                else if (j["action"].is_string()) {
                    // Fallback: map common strings
                    std::string a = j["action"].get<std::string>();
                    if (a == "connected") action = 4;
                    else if (a == "message") action = 15;
                }
                if (action == 4) {
                    QTimer::singleShot(100, this, [this]() { attachChannel(); });
                }
            }
        } catch (...) {
        }
        if (m_onMessage) QMetaObject::invokeMethod(this, [this, msg]() { m_onMessage(msg); }, Qt::QueuedConnection);
    });
    m_wsClient->setCloseCallback([this]() {
        if (m_onClose) QMetaObject::invokeMethod(this, [this]() { m_onClose(); }, Qt::QueuedConnection);
        if (!m_closing) scheduleReconnect();
    });
    m_wsClient->setErrorCallback([this](const std::string& err) {
        if (m_onError) QMetaObject::invokeMethod(this, [this, err]() { m_onError(err); }, Qt::QueuedConnection);
        if (m_hostIndex + 1 < (int)m_hosts.size()) {
            m_hostIndex++;
            tryConnectWithFallbackHosts();
        } else {
            scheduleReconnect();
        }
    });
}

OneSevenLiveAblyChatClient::~OneSevenLiveAblyChatClient() { disconnect(); }

void OneSevenLiveAblyChatClient::setRoomId(const QString& roomId) { m_roomId = roomId; }
void OneSevenLiveAblyChatClient::setAblyToken(const QString& token) { m_token = token; }

void OneSevenLiveAblyChatClient::setOnOpen(const std::function<void()>& cb) { m_onOpen = cb; }
void OneSevenLiveAblyChatClient::setOnMessage(const std::function<void(const std::string&)>& cb) {
    m_onMessage = cb;
}
void OneSevenLiveAblyChatClient::setOnClose(const std::function<void()>& cb) { m_onClose = cb; }
void OneSevenLiveAblyChatClient::setOnError(const std::function<void(const std::string&)>& cb) {
    m_onError = cb;
}

bool OneSevenLiveAblyChatClient::connect() {
    if (m_roomId.isEmpty()) return false;

    // If token is empty, fetch via API
    if (m_token.isEmpty()) {
        OneSevenLiveApiWrappers api;
        nlohmann::json resp;
        if (!api.GetAblyToken(m_roomId.toStdString(), resp)) {
            return false;
        }
        if (resp.contains("token") && resp["token"].is_string()) {
            m_token = QString::fromStdString(resp["token"].get<std::string>());
        }
        if (m_token.isEmpty()) return false;
    }

    m_hostIndex = 0;
    m_closing = false;
    m_reconnectAttempts = 0;
    cancelReconnect();
    tryConnectWithFallbackHosts();
    return true;
}

void OneSevenLiveAblyChatClient::disconnect() {
    m_closing = true;
    cancelReconnect();
    if (m_wsClient) m_wsClient->disconnect();
}

bool OneSevenLiveAblyChatClient::isConnected() const {
    return m_wsClient && m_wsClient->isConnected();
}

void OneSevenLiveAblyChatClient::tryConnectWithFallbackHosts() {
    if (m_hostIndex < 0 || m_hostIndex >= (int)m_hosts.size()) return;
    const QString host = m_hosts[m_hostIndex];
    // Build Ably websocket URL with token auth (JSON protocol, echo off)
    QUrl url(QString("%1?protocol=json&echo=false&access_token=%2").arg(host, m_token));
    m_wsClient->connectUrl(url.toString());
}

void OneSevenLiveAblyChatClient::attachChannel() {
    if (!isConnected()) return;
    // Attach/subscribe to channel per Ably protocol (JSON protocol message)
    // Send ATTACH (numeric action per Ably protocol)
    nlohmann::json attachMsg;
    attachMsg["action"] = 10; // ATTACH
    attachMsg["channel"] = m_roomId.toStdString();
    m_wsClient->sendText(QString::fromStdString(attachMsg.dump()));
}

void OneSevenLiveAblyChatClient::scheduleReconnect() {
    if (m_closing) return;
    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        QObject::connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
            if (m_closing) return;
            m_hostIndex = 0;
            tryConnectWithFallbackHosts();
        });
    }
    if (m_reconnectAttempts >= m_maxReconnectAttempts) m_reconnectAttempts = m_maxReconnectAttempts;
    int delay = m_baseReconnectDelayMs;
    for (int i = 0; i < m_reconnectAttempts; ++i) {
        delay = std::min(delay * 2, 15000);
    }
    m_reconnectAttempts = std::min(m_reconnectAttempts + 1, m_maxReconnectAttempts);
    m_reconnectTimer->start(delay);
}

void OneSevenLiveAblyChatClient::cancelReconnect() {
    if (m_reconnectTimer) m_reconnectTimer->stop();
}
