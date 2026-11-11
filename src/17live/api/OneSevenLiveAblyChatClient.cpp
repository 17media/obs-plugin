#include "OneSevenLiveAblyChatClient.hpp"

#include <obs-module.h>
#include <zlib.h>

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <optional>

#include "../OneSevenLiveCoreManager.hpp"
#include "chat/OneSevenLiveChatMessageHandler.hpp"
#include "plugin-support.h"
#include "streaming/OneSevenLiveStreamManager.hpp"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
#include "websocket/WebsocketUtils.hpp"
#include "websocket/WsMessage.hpp"

OneSevenLiveAblyChatClient::OneSevenLiveAblyChatClient(QObject* parent)
    : QObject(parent), m_wsClient(std::make_unique<OneSevenLiveWebsocketClient>()) {
    m_hosts = {
        "wss://17-media-a-fallback.ably-realtime.com/realtime",
        "wss://17-media-b-fallback.ably-realtime.com/realtime",
        "wss://17-media-c-fallback.ably-realtime.com/realtime",
    };

    m_wsClient->setOpenCallback([this]() {
        stopIdleWatchdog();
        obs_log(LOG_INFO, "[AblyDiag] websocket_open host_index=%d", m_hostIndex);
        if (m_onOpen)
            QMetaObject::invokeMethod(this, [this]() { m_onOpen(); }, Qt::QueuedConnection);
    });
    m_wsClient->setMessageCallback([this](const std::string& msg) {
        m_diagLastMessageMs = QDateTime::currentMSecsSinceEpoch();
        noteInboundActivity();
        try {
            handleProtocolMessage(nlohmann::json::parse(msg));
        } catch (const std::exception& e) {
            obs_log(LOG_WARNING, "[Ably] JSON parse error: %s", e.what());
        } catch (...) {
            obs_log(LOG_WARNING, "[Ably] Unknown JSON parse error");
        }

        OneSevenLiveChatMessageHandler handler;
        handler.handleRaw(msg);
        if (m_onMessage)
            QMetaObject::invokeMethod(
                this, [this, msg]() { m_onMessage(msg); }, Qt::QueuedConnection);
    });
    m_wsClient->setCloseCallback([this]() {
        stopIdleWatchdog();
        obs_log(LOG_INFO, "[AblyDiag] websocket_close attached=%d closing=%d channel_failed=%d",
                m_attached, m_closing, m_channelFailed);
        m_attached = false;
        if (m_onClose)
            QMetaObject::invokeMethod(this, [this]() { m_onClose(); }, Qt::QueuedConnection);
        if (!m_closing && !m_channelFailed)
            scheduleReconnect();
        broadcastStatus(false);
    });
    m_wsClient->setErrorCallback([this](const std::string& err) {
        stopIdleWatchdog();
        const auto category = err.substr(0, err.find(' '));
        obs_log(LOG_WARNING,
                "[AblyDiag] websocket_error category=%s host_index=%d attached=%d closing=%d",
                category.c_str(), m_hostIndex, m_attached, m_closing);
        m_attached = false;
        if (m_onError)
            QMetaObject::invokeMethod(
                this, [this, err]() { m_onError(err); }, Qt::QueuedConnection);
        if (m_closing || m_channelFailed)
            return;
        if (m_hostIndex + 1 < (int) m_hosts.size()) {
            m_hostIndex++;
            tryConnectWithFallbackHosts();
        } else {
            scheduleReconnect();
        }
        broadcastStatus(false);
    });
}

OneSevenLiveAblyChatClient::~OneSevenLiveAblyChatClient() {
    disconnect();
}

void OneSevenLiveAblyChatClient::broadcastStatus(bool connected, int errorCode) {
    auto* sm = OneSevenLiveCoreManager::getInstance().getStreamManager();
    connected = connected && sm &&
                sm->getCurrentStreamingStatus() != OneSevenLiveStreamingStatus::NotStarted;
    nlohmann::json payload{{"status", connected ? "connected" : "break"}};
    if (!connected && !errorCode)
        errorCode = m_lastErrorCode;
    if (errorCode)
        payload["errorCode"] = errorCode;
    OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
        QString::fromUtf8(ws::EventAblyChatConnected), payload);
}

void OneSevenLiveAblyChatClient::handleProtocolMessage(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("action") ||
        !message["action"].is_number_integer())
        return;
    const int action = message["action"].get<int>();
    const bool roomChannel = message.value("channel", std::string()) == m_roomId.toStdString();
    if (roomChannel && action == 15)
        ++m_diagMessageFrames;
    if (action == 4) {
        const auto details = message.find("connectionDetails");
        if (details != message.end() && details->is_object()) {
            configureIdleWatchdog(*details);
            const auto key = details->find("connectionKey");
            if (key != details->end() && key->is_string())
                m_connectionKey = QString::fromStdString(key->get<std::string>());
        }
        const QString connectionId =
            QString::fromStdString(message.value("connectionId", std::string()));
        const bool authUpdate = ably::isAuthUpdate(
            m_attached, m_connectionId.toStdString(), connectionId.toStdString());
        obs_log(LOG_INFO,
                "[AblyDiag] protocol_connected auth_update=%d connection_id_changed=%d has_key=%d",
                authUpdate,
                !m_connectionId.isEmpty() && !connectionId.isEmpty() &&
                    connectionId != m_connectionId,
                !m_connectionKey.isEmpty());
        if (!m_connectionId.isEmpty() && !connectionId.isEmpty() && connectionId != m_connectionId)
            obs_log(LOG_WARNING, "[Ably] Connection ID changed; checking channel continuity");
        if (!connectionId.isEmpty())
            m_connectionId = connectionId;
        if (!authUpdate)
            attachChannel();
    } else if (roomChannel && action == 11) {
        const bool continuityLost = m_channelRecovery.attached(message);
        const int flags = message.value("flags", 0);
        obs_log(LOG_INFO, "[AblyDiag] channel_attached flags=%d resumed=%d continuity_lost=%d",
                flags, (flags & (1 << 2)) != 0, continuityLost);
        m_attached = true;
        m_lastErrorCode = 0;
        m_reconnectAttempts = 0;
        cancelReconnect();
        if (continuityLost)
            obs_log(LOG_WARNING, "[Ably] Channel attached without message continuity");
        broadcastStatus(true);
    } else if (action == 6 || action == 9 || (roomChannel && action == 13)) {
        obs_log(LOG_WARNING, "[AblyDiag] protocol_error action=%d room_channel=%d", action,
                roomChannel);
        handleProtocolError(message);
    } else if (roomChannel && (action == 14 || action == 15 || action == 19 || action == 21)) {
        m_channelRecovery.updateSerial(message);
    }
}

void OneSevenLiveAblyChatClient::handleProtocolError(const nlohmann::json& message) {
    int code = 0;
    int statusCode = 0;
    const auto error = message.find("error");
    if (error != message.end() && error->is_object()) {
        code = error->value("code", 0);
        statusCode = error->value("statusCode", 0);
    }
    m_attached = false;
    m_lastErrorCode = code;
    broadcastStatus(false, code);
    if (ably::classifyError(code, statusCode) == ably::ErrorKind::Permission) {
        m_channelFailed = true;
        cancelReconnect();
        obs_log(LOG_ERROR, "[Ably] Channel permission denied (code %d, status %d)", code,
                statusCode);
        return;
    }
    if (ably::classifyError(code, statusCode) == ably::ErrorKind::Token)
        m_token.clear();
    obs_log(LOG_WARNING, "[Ably] Protocol error (code %d, status %d); scheduling retry", code,
            statusCode);
    scheduleReconnect();
}

void OneSevenLiveAblyChatClient::setRoomId(const QString& roomId) {
    if (m_roomId != roomId) {
        if (!m_roomId.isEmpty())
            disconnect();
        m_token.clear();
        m_connectionKey.clear();
        m_connectionId.clear();
        m_channelRecovery.reset();
    }
    m_roomId = roomId;
}

void OneSevenLiveAblyChatClient::setAblyToken(const QString& token) {
    m_token = token;
}

void OneSevenLiveAblyChatClient::setOnOpen(const std::function<void()>& cb) {
    m_onOpen = cb;
}

void OneSevenLiveAblyChatClient::setOnMessage(const std::function<void(const std::string&)>& cb) {
    m_onMessage = cb;
}

void OneSevenLiveAblyChatClient::setOnClose(const std::function<void()>& cb) {
    m_onClose = cb;
}

void OneSevenLiveAblyChatClient::setOnError(const std::function<void(const std::string&)>& cb) {
    m_onError = cb;
}

void OneSevenLiveAblyChatClient::setAuthCallback(
    const std::function<bool(const QString&, nlohmann::json&)>& cb) {
    m_authCallback = cb;
}

bool OneSevenLiveAblyChatClient::connect() {
    if (m_roomId.isEmpty())
        return false;

    m_diagLastMessageMs = 0;
    m_diagMessageFrames = 0;
    obs_log(LOG_INFO, "[AblyDiag] connect_requested has_token=%d", !m_token.isEmpty());
    m_closing = false;
    m_channelFailed = false;
    m_lastErrorCode = 0;
    m_hostIndex = 0;
    m_reconnectAttempts = 0;
    cancelReconnect();
    if (m_token.isEmpty()) {
        fetchTokenAsync([this](bool success) {
            if (m_closing)
                return;
            if (success) {
                tryConnectWithFallbackHosts();
            } else {
                obs_log(LOG_WARNING,
                        "Failed to fetch Ably token during connect, scheduling retry");
                scheduleReconnect();
            }
        });
    } else {
        scheduleTokenRefresh(0, 0, 0);
        tryConnectWithFallbackHosts();
    }

    return true;
}

void OneSevenLiveAblyChatClient::disconnect() {
    obs_log(LOG_INFO, "[AblyDiag] disconnect_requested attached=%d", m_attached);
    m_closing = true;
    stopIdleWatchdog();
    ++m_generation;
    m_tokenFetchInProgress = false;
    m_attached = false;
    cancelReconnect();
    cancelTokenRefresh();
    if (m_wsClient)
        m_wsClient->disconnect();
}

bool OneSevenLiveAblyChatClient::isConnected() const {
    return m_attached && m_wsClient && m_wsClient->isConnected();
}

int OneSevenLiveAblyChatClient::lastErrorCode() const {
    return m_lastErrorCode;
}

void OneSevenLiveAblyChatClient::configureIdleWatchdog(const nlohmann::json& details) {
    const int timeout = ably::idleTimeoutMs(details);
    if (!timeout) {
        obs_log(LOG_INFO, "[AblyDiag] idle_watchdog_no_valid_interval");
        return;
    }
    if (!m_idleTimer) {
        m_idleTimer = new QTimer(this);
        m_idleTimer->setSingleShot(true);
        m_idleTimer->setTimerType(Qt::PreciseTimer);
        QObject::connect(m_idleTimer, &QTimer::timeout, this, [this]() {
            if (m_closing || m_channelFailed || !m_idleTimeoutMs ||
                !m_lastInboundActivity.isValid())
                return;
            const auto elapsed = m_lastInboundActivity.elapsed();
            if (elapsed < m_idleTimeoutMs) {
                m_idleTimer->start(static_cast<int>(m_idleTimeoutMs - elapsed));
                return;
            }
            obs_log(LOG_WARNING, "[AblyDiag] idle_timeout elapsed_ms=%lld timeout_ms=%d",
                    static_cast<long long>(elapsed), m_idleTimeoutMs);
            stopIdleWatchdog();
            m_attached = false;
            broadcastStatus(false);
            if (m_wsClient)
                m_wsClient->abortAsync();
            scheduleReconnect();
        });
    }
    m_idleTimeoutMs = timeout;
    noteInboundActivity();
    obs_log(LOG_INFO, "[AblyDiag] idle_watchdog_started timeout_ms=%d", timeout);
}

void OneSevenLiveAblyChatClient::noteInboundActivity() {
    if (!m_idleTimer || !m_idleTimeoutMs)
        return;
    m_lastInboundActivity.start();
    m_idleTimer->start(m_idleTimeoutMs);
}

void OneSevenLiveAblyChatClient::stopIdleWatchdog() {
    if (m_idleTimer)
        m_idleTimer->stop();
    m_idleTimeoutMs = 0;
    m_lastInboundActivity.invalidate();
}

void OneSevenLiveAblyChatClient::logDiagnostics() const {
    const auto lastMessageAgeMs =
        m_diagLastMessageMs > 0 ? QDateTime::currentMSecsSinceEpoch() - m_diagLastMessageMs : -1;
    obs_log(LOG_INFO,
            "[AblyDiag] state socket_connected=%d attached=%d closing=%d channel_failed=%d "
            "retry_active=%d token_fetch_active=%d last_frame_age_ms=%lld message_frames=%llu",
            m_wsClient && m_wsClient->isConnected(), m_attached, m_closing, m_channelFailed,
            m_reconnectTimer && m_reconnectTimer->isActive(), m_tokenFetchInProgress,
            static_cast<long long>(lastMessageAgeMs),
            static_cast<unsigned long long>(m_diagMessageFrames));
}

void OneSevenLiveAblyChatClient::tryConnectWithFallbackHosts() {
    if (m_hostIndex < 0 || m_hostIndex >= (int) m_hosts.size())
        return;
    obs_log(LOG_INFO, "[AblyDiag] websocket_connect host_index=%d resume_requested=%d",
            m_hostIndex, !m_connectionKey.isEmpty());
    const QString host = m_hosts[m_hostIndex];
    // Build Ably websocket URL with token auth (JSON protocol, echo off)
    // Ably protocol v2: resume is driven by connectionKey only, connection_serial is gone
    QString query = QString("protocol=json&echo=false&access_token=%1&v=2").arg(m_token);
    if (!m_connectionKey.isEmpty()) {
        query += QString("&resume=%1").arg(m_connectionKey);
    }
    QUrl url(QString("%1?%2").arg(host, query));
    m_wsClient->connectUrl(url.toString());
}

void OneSevenLiveAblyChatClient::attachChannel() {
    if (!m_wsClient || !m_wsClient->isConnected() || m_roomId.isEmpty() || m_channelFailed)
        return;
    m_attached = false;
    const auto attachMsg = m_channelRecovery.attach(m_roomId.toStdString());
    obs_log(LOG_INFO, "[AblyDiag] channel_attach resume_requested=%d has_serial=%d",
            attachMsg.contains("flags"), attachMsg.contains("channelSerial"));
    m_wsClient->sendText(QString::fromStdString(attachMsg.dump()));
    const auto generation = m_generation;
    const auto attempt = ++m_attachAttempt;
    QTimer::singleShot(10000, this, [this, generation, attempt]() {
        if (generation == m_generation && attempt == m_attachAttempt && !m_closing && !m_attached &&
            !m_channelFailed) {
            obs_log(LOG_WARNING, "[AblyDiag] channel_attach_timeout");
            scheduleReconnect();
        }
    });
}

void OneSevenLiveAblyChatClient::scheduleReconnect() {
    if (m_closing || m_channelFailed)
        return;
    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        QObject::connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
            if (m_closing)
                return;
            obs_log(LOG_INFO, "[AblyDiag] retry_started attempt=%d", m_reconnectAttempts);
            refreshToken([this](bool success) {
                if (m_closing || m_channelFailed)
                    return;
                obs_log(LOG_INFO, "[AblyDiag] retry_token_result success=%d connected=%d", success,
                        isConnected());
                if (!success) {
                    scheduleReconnect();
                    return;
                }
                if (isConnected()) {
                    m_reconnectAttempts = 0;
                    sendAuth();
                    return;
                }
                m_hostIndex = 0;
                tryConnectWithFallbackHosts();
            });
        });
    }
    if (m_reconnectTimer->isActive())
        return;
    if (m_reconnectAttempts >= m_maxReconnectAttempts)
        m_reconnectAttempts = m_maxReconnectAttempts;
    int delay = m_baseReconnectDelayMs;
    for (int i = 0; i < m_reconnectAttempts; ++i) {
        delay = std::min(delay * 2, 15000);
    }
    m_reconnectAttempts = std::min(m_reconnectAttempts + 1, m_maxReconnectAttempts);
    obs_log(LOG_INFO, "[AblyDiag] retry_scheduled attempt=%d delay_ms=%d", m_reconnectAttempts,
            delay);
    m_reconnectTimer->start(delay);
}

void OneSevenLiveAblyChatClient::cancelReconnect() {
    if (m_reconnectTimer)
        m_reconnectTimer->stop();
}

void OneSevenLiveAblyChatClient::refreshToken(std::function<void(bool)> callback) {
    if (m_roomId.isEmpty()) {
        if (callback)
            callback(false);
        return;
    }

    fetchTokenAsync(callback);
}

void OneSevenLiveAblyChatClient::scheduleTokenRefresh(qint64 expiresEpochMs, qint64 issuedEpochMs,
                                                      qint64 ttlMs) {
    if (!m_tokenRefreshTimer) {
        m_tokenRefreshTimer = new QTimer(this);
        m_tokenRefreshTimer->setSingleShot(true);
        QObject::connect(m_tokenRefreshTimer, &QTimer::timeout, this, [this]() {
            if (m_closing)
                return;
            refreshToken([this](bool success) {
                if (m_closing)
                    return;
                if (success) {
                    if (m_wsClient && m_wsClient->isConnected()) {
                        sendAuth();
                    } else {
                        scheduleReconnect();
                    }
                } else {
                    scheduleReconnect();
                }
            });
        });
    }
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 expiry = 0;
    if (expiresEpochMs > 0)
        expiry = expiresEpochMs;
    else if (issuedEpochMs > 0 && ttlMs > 0)
        expiry = issuedEpochMs + ttlMs;
    else if (ttlMs > 0)
        expiry = now + ttlMs;
    else
        expiry = now + m_tokenDefaultTtlMs;
    m_tokenExpiresMs = expiry;
    qint64 msUntilRefresh = std::max<qint64>(1000, expiry - now - m_tokenRefreshAdvanceMs);
    m_tokenRefreshTimer->start((int) msUntilRefresh);
}

void OneSevenLiveAblyChatClient::cancelTokenRefresh() {
    if (m_tokenRefreshTimer)
        m_tokenRefreshTimer->stop();
}

void OneSevenLiveAblyChatClient::sendAuth() {
    if (!m_wsClient || !m_wsClient->isConnected())
        return;
    nlohmann::json authMsg;
    authMsg["action"] = 17;
    nlohmann::json auth;
    auth["accessToken"] = m_token.toStdString();
    authMsg["auth"] = auth;
    m_wsClient->sendText(QString::fromStdString(authMsg.dump()));
}

void OneSevenLiveAblyChatClient::fetchTokenAsync(std::function<void(bool)> callback) {
    if (m_tokenFetchInProgress) {
        obs_log(LOG_INFO, "[AblyDiag] token_fetch_skipped already_in_progress=1");
        if (callback)
            callback(false);
        return;
    }
    obs_log(LOG_INFO, "[AblyDiag] token_fetch_started");
    m_tokenFetchInProgress = true;
    const auto generation = m_generation;
    QString rid = m_roomId;
    // Capture required resources by value to ensure thread safety if 'this' is destroyed
    // Note: apiWrapper is still a pointer, but capturing it avoids accessing 'this->' inside the
    // thread The caller (OneSevenLiveCoreManager) owns apiWrapper, so its lifetime usually exceeds
    // this operation
    auto authCb = m_authCallback;
    auto* api = OneSevenLiveCoreManager::getInstance().getApiWrapper();

    // Use QPointer to track object validity
    QPointer<OneSevenLiveAblyChatClient> self(this);

    QThread* t = QThread::create([self, rid, callback, authCb, api, generation]() {
        nlohmann::json resp;
        bool success = false;

        if (authCb) {
            success = authCb(rid, resp);
        } else if (api) {
            success = api->GetAblyToken(rid.toStdString(), resp);
        }

        // If object is destroyed, don't invoke callback
        if (!self)
            return;

        QMetaObject::invokeMethod(
            self,
            [self, success, resp, callback, generation]() {
                if (!self || self->m_generation != generation || self->m_closing)
                    return;
                self->m_tokenFetchInProgress = false;
                if (success && resp.contains("token") && resp["token"].is_string()) {
                    const auto token = QString::fromStdString(resp["token"].get<std::string>());
                    if (!token.isEmpty()) {
                        obs_log(LOG_INFO, "[AblyDiag] token_fetch_finished success=1");
                        self->m_token = token;
                        qint64 expiresMs = 0, issuedMs = 0, ttlMs = 0;
                        if (resp.contains("expires") && resp["expires"].is_number_integer())
                            expiresMs = resp["expires"].get<long long>();
                        if (resp.contains("issued") && resp["issued"].is_number_integer())
                            issuedMs = resp["issued"].get<long long>();
                        if (resp.contains("ttl") && resp["ttl"].is_number_integer())
                            ttlMs = resp["ttl"].get<long long>();
                        if (resp.contains("expiresIn") && resp["expiresIn"].is_number_integer())
                            ttlMs = resp["expiresIn"].get<long long>();
                        self->scheduleTokenRefresh(expiresMs, issuedMs, ttlMs);

                        if (callback)
                            callback(true);
                        return;
                    }
                }
                obs_log(LOG_WARNING, "[AblyDiag] token_fetch_finished success=0");
                if (callback)
                    callback(false);
            },
            Qt::QueuedConnection);
    });
    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}
