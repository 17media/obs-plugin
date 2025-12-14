#include "OneSevenLiveAblyChatClient.hpp"

#include <obs-module.h>
#include <zlib.h>

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <optional>
#include <thread>

#include "../OneSevenLiveCoreManager.hpp"
#include "streaming/OneSevenLiveStreamManager.hpp"
#include "chat/OneSevenLiveChatMessageHandler.hpp"
#include "plugin-support.h"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
#include "websocket/WebsocketUtils.hpp"
#include "websocket/WsMessage.hpp"
#include "streaming/OneSevenLiveStreamManager.hpp"

OneSevenLiveAblyChatClient::OneSevenLiveAblyChatClient(QObject* parent)
    : QObject(parent), m_wsClient(std::make_unique<OneSevenLiveWebsocketClient>()) {
    m_hosts = {
        "wss://17-media-a-fallback.ably-realtime.com/realtime",
        "wss://17-media-b-fallback.ably-realtime.com/realtime",
        "wss://17-media-c-fallback.ably-realtime.com/realtime",
    };

    m_wsClient->setOpenCallback([this]() {
        if (m_onOpen)
            QMetaObject::invokeMethod(this, [this]() { m_onOpen(); }, Qt::QueuedConnection);
        m_reconnectAttempts = 0;
    });
    m_wsClient->setMessageCallback([this](const std::string& msg) {
        // Parse Ably protocol message and attach after CONNECTED
        try {
            nlohmann::json j = nlohmann::json::parse(msg);
            obs_log(LOG_DEBUG, "[Ably] recv %s", j.dump().c_str());
            if (j.contains("action")) {
                int action = -1;
                if (j["action"].is_number_integer())
                    action = j["action"].get<int>();
                else if (j["action"].is_string()) {
                    // Fallback: map common strings
                    std::string a = j["action"].get<std::string>();
                    if (a == "connected")
                        action = 4;
                    else if (a == "message")
                        action = 15;
                    else if (a == "error")
                        action = 9;
                    else if (a == "disconnected")
                        action = 6;
                }
                if (action == 4) {
                    try {
                        if (j.contains("connectionKey") && j["connectionKey"].is_string()) {
                            m_connectionKey =
                                QString::fromStdString(j["connectionKey"].get<std::string>());
                        } else if (j.contains("connectionId") && j["connectionId"].is_string()) {
                            m_connectionKey =
                                QString::fromStdString(j["connectionId"].get<std::string>());
                        }
                    } catch (...) {
                    }
                    auto* sm = OneSevenLiveCoreManager::getInstance().getStreamManager();
                    bool isLive = false;
                    if (sm) {
                        auto st = sm->getCurrentStreamingStatus();
                        isLive = (st != OneSevenLiveStreamingStatus::NotStarted);
                    }
                    if (isLive) {
                        OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
                            QString::fromUtf8(ws::EventAblyChatConnected),
                            nlohmann::json{{"status", "connected"}});
                    }
                    QTimer::singleShot(100, this, [this]() { attachChannel(); });
                } else if (action == 11) {
                    m_attached = true;
                } else if (action == 6 || action == 9) {
                    bool needRefresh = false;
                    if (j.contains("error") && j["error"].is_object()) {
                        try {
                            const auto& e = j["error"];
                            if (e.contains("code") && e["code"].is_number_integer()) {
                                int code = e["code"].get<int>();
                                if (code >= 40140 && code <= 40149)
                                    needRefresh = true;
                            }
                        } catch (...) {
                        }
                    }
                    if (needRefresh) {
                        refreshToken([this](bool success) {
                            (void) success;
                            scheduleReconnect();
                        });
                    } else {
                        scheduleReconnect();
                    }
                } else if (action == 14) {
                } else if (action == 16) {
                } else if (action == 12) {
                    if (j.contains("channel") && j["channel"].is_string()) {
                        m_attachedChannels.remove(
                            QString::fromStdString(j["channel"].get<std::string>()));
                    }
                } else if (action == 1 || action == 2) {
                    int msgSerial = j.contains("msgSerial") && j["msgSerial"].is_number_integer()
                                        ? j["msgSerial"].get<int>()
                                        : -1;
                    std::string reason;
                    if (j.contains("error") && j["error"].is_object()) {
                        reason = j["error"].dump();
                    }
                    obs_log(LOG_INFO, "[Ably] %s msgSerial=%d reason=%s",
                            action == 1 ? "ack" : "nack", msgSerial, reason.c_str());
                }
                if (j.contains("connectionSerial") && j["connectionSerial"].is_number_integer()) {
                    try {
                        m_lastConnectionSerial = j["connectionSerial"].get<long long>();
                    } catch (...) {
                    }
                }
            }
        } catch (...) {
        }

        OneSevenLiveChatMessageHandler handler;
        handler.handleRaw(msg);
        if (m_onMessage)
            QMetaObject::invokeMethod(
                this, [this, msg]() { m_onMessage(msg); }, Qt::QueuedConnection);
    });
    m_wsClient->setCloseCallback([this]() {
        if (m_onClose)
            QMetaObject::invokeMethod(this, [this]() { m_onClose(); }, Qt::QueuedConnection);
        if (!m_closing)
            scheduleReconnect();
        OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
            QString::fromUtf8(ws::EventAblyChatConnected), nlohmann::json{{"status", "break"}});
    });
    m_wsClient->setErrorCallback([this](const std::string& err) {
        if (m_onError)
            QMetaObject::invokeMethod(
                this, [this, err]() { m_onError(err); }, Qt::QueuedConnection);
        if (m_hostIndex + 1 < (int) m_hosts.size()) {
            m_hostIndex++;
            tryConnectWithFallbackHosts();
        } else {
            scheduleReconnect();
        }
        OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
            QString::fromUtf8(ws::EventAblyChatConnected),
            nlohmann::json{{"status", "break"}, {"error", err}});
    });
}

OneSevenLiveAblyChatClient::~OneSevenLiveAblyChatClient() {
    disconnect();
}

void OneSevenLiveAblyChatClient::setRoomId(const QString& roomId) {
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

    if (m_token.isEmpty()) {
        fetchTokenAsync([this](bool success) {
            if (success) {
                m_hostIndex = 0;
                m_closing = false;
                m_reconnectAttempts = 0;
                cancelReconnect();
                tryConnectWithFallbackHosts();
            } else {
                obs_log(LOG_ERROR, "Failed to fetch Ably token during connect");
            }
        });
    } else {
        scheduleTokenRefresh(0, 0, 0);
        m_hostIndex = 0;
        m_closing = false;
        m_reconnectAttempts = 0;
        cancelReconnect();
        tryConnectWithFallbackHosts();
    }

    return true;
}

void OneSevenLiveAblyChatClient::disconnect() {
    m_closing = true;
    cancelReconnect();
    cancelTokenRefresh();
    if (m_wsClient)
        m_wsClient->disconnect();
}

bool OneSevenLiveAblyChatClient::isConnected() const {
    return m_wsClient && m_wsClient->isConnected();
}

void OneSevenLiveAblyChatClient::tryConnectWithFallbackHosts() {
    if (m_hostIndex < 0 || m_hostIndex >= (int) m_hosts.size())
        return;
    const QString host = m_hosts[m_hostIndex];
    // Build Ably websocket URL with token auth (JSON protocol, echo off)
    QString query = QString("protocol=json&echo=false&access_token=%1&v=1.2").arg(m_token);
    if (!m_connectionKey.isEmpty() && m_lastConnectionSerial >= 0) {
        query += QString("&resume=%1&connection_serial=%2")
                     .arg(m_connectionKey, QString::number(m_lastConnectionSerial));
    }
    QUrl url(QString("%1?%2").arg(host, query));
    m_wsClient->connectUrl(url.toString());
}

void OneSevenLiveAblyChatClient::attachChannel() {
    if (!isConnected())
        return;
    m_attached = false;
    // Attach/subscribe to channel per Ably protocol (JSON protocol message)
    // Send ATTACH (numeric action per Ably protocol)
    nlohmann::json attachMsg;
    attachMsg["action"] = 10;  // ATTACH
    attachMsg["channel"] = m_roomId.toStdString();

    // Only log error if send fails or logic fails; success is noisy
    // obs_log(LOG_INFO, "[Ably] send %s", attachMsg.dump().c_str());
    m_wsClient->sendText(QString::fromStdString(attachMsg.dump()));
}

void OneSevenLiveAblyChatClient::attachChannel(const QString& channel) {
    if (!isConnected() || channel.isEmpty())
        return;
    nlohmann::json attachMsg;
    attachMsg["action"] = 10;
    attachMsg["channel"] = channel.toStdString();
    // obs_log(LOG_INFO, "[Ably] send %s", attachMsg.dump().c_str());
    m_wsClient->sendText(QString::fromStdString(attachMsg.dump()));
}

void OneSevenLiveAblyChatClient::detachChannel(const QString& channel) {
    if (!isConnected() || channel.isEmpty())
        return;
    nlohmann::json detachMsg;
    detachMsg["action"] = 12;
    detachMsg["channel"] = channel.toStdString();
    obs_log(LOG_INFO, "[Ably] send %s", detachMsg.dump().c_str());
    m_wsClient->sendText(QString::fromStdString(detachMsg.dump()));
}

void OneSevenLiveAblyChatClient::publishMessage(const QString& channel,
                                                const nlohmann::json& payload) {
    if (!isConnected() || channel.isEmpty())
        return;
    nlohmann::json pm;
    pm["action"] = 15;
    pm["channel"] = channel.toStdString();
    pm["msgSerial"] = m_msgSerialCounter++;
    nlohmann::json m;
    m["data"] = payload;
    pm["messages"] = nlohmann::json::array({m});
    obs_log(LOG_INFO, "[Ably] send %s", pm.dump().c_str());
    m_wsClient->sendText(QString::fromStdString(pm.dump()));
}

void OneSevenLiveAblyChatClient::enterPresence(const QString& channel, const nlohmann::json& data) {
    if (!isConnected() || channel.isEmpty())
        return;
    nlohmann::json pm;
    pm["action"] = 14;
    pm["channel"] = channel.toStdString();
    nlohmann::json p;
    p["action"] = "enter";
    p["data"] = data;
    pm["presence"] = nlohmann::json::array({p});
    obs_log(LOG_INFO, "[Ably] send %s", pm.dump().c_str());
    m_wsClient->sendText(QString::fromStdString(pm.dump()));
}

void OneSevenLiveAblyChatClient::updatePresence(const QString& channel,
                                                const nlohmann::json& data) {
    if (!isConnected() || channel.isEmpty())
        return;
    nlohmann::json pm;
    pm["action"] = 14;
    pm["channel"] = channel.toStdString();
    nlohmann::json p;
    p["action"] = "update";
    p["data"] = data;
    pm["presence"] = nlohmann::json::array({p});
    obs_log(LOG_INFO, "[Ably] send %s", pm.dump().c_str());
    m_wsClient->sendText(QString::fromStdString(pm.dump()));
}

void OneSevenLiveAblyChatClient::leavePresence(const QString& channel) {
    if (!isConnected() || channel.isEmpty())
        return;
    nlohmann::json pm;
    pm["action"] = 14;
    pm["channel"] = channel.toStdString();
    nlohmann::json p;
    p["action"] = "leave";
    pm["presence"] = nlohmann::json::array({p});
    obs_log(LOG_INFO, "[Ably] send %s", pm.dump().c_str());
    m_wsClient->sendText(QString::fromStdString(pm.dump()));
}

void OneSevenLiveAblyChatClient::scheduleReconnect() {
    if (m_closing)
        return;
    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        QObject::connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
            if (m_closing)
                return;
            refreshToken([this](bool success) {
                (void) success;
                if (m_closing)
                    return;
                m_hostIndex = 0;
                tryConnectWithFallbackHosts();
            });
        });
    }
    if (m_reconnectAttempts >= m_maxReconnectAttempts)
        m_reconnectAttempts = m_maxReconnectAttempts;
    int delay = m_baseReconnectDelayMs;
    for (int i = 0; i < m_reconnectAttempts; ++i) {
        delay = std::min(delay * 2, 15000);
    }
    m_reconnectAttempts = std::min(m_reconnectAttempts + 1, m_maxReconnectAttempts);
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
                    if (isConnected()) {
                        sendAuth();
                    } else {
                        scheduleReconnect();
                    }
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
    qint64 msUntilRefresh = std::max<qint64>(0, expiry - now - m_tokenRefreshAdvanceMs);
    m_tokenRefreshTimer->start((int) msUntilRefresh);
}

void OneSevenLiveAblyChatClient::cancelTokenRefresh() {
    if (m_tokenRefreshTimer)
        m_tokenRefreshTimer->stop();
}

void OneSevenLiveAblyChatClient::sendAuth() {
    if (!isConnected())
        return;
    nlohmann::json authMsg;
    authMsg["action"] = 17;
    nlohmann::json auth;
    auth["accessToken"] = m_token.toStdString();
    authMsg["auth"] = auth;
    m_wsClient->sendText(QString::fromStdString(authMsg.dump()));
}

void OneSevenLiveAblyChatClient::sendConnect() {
    if (!isConnected())
        return;
    nlohmann::json conn;
    conn["action"] = 2;  // CONNECT
    if (!m_connectionKey.isEmpty()) {
        conn["connectionKey"] = m_connectionKey.toStdString();
    }
    conn["protocol"] = "json";
    m_wsClient->sendText(QString::fromStdString(conn.dump()));
}

void OneSevenLiveAblyChatClient::fetchTokenAsync(std::function<void(bool)> callback) {
    QString rid = m_roomId;
    // Capture required resources by value to ensure thread safety if 'this' is destroyed
    // Note: apiWrapper is still a pointer, but capturing it avoids accessing 'this->' inside the
    // thread The caller (OneSevenLiveCoreManager) owns apiWrapper, so its lifetime usually exceeds
    // this operation
    auto authCb = m_authCallback;
    auto* api = OneSevenLiveCoreManager::getInstance().getApiWrapper();

    // Use QPointer to track object validity
    QPointer<OneSevenLiveAblyChatClient> self(this);

    std::thread([self, rid, callback, authCb, api]() {
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
            [self, success, resp, callback]() {
                if (!self)
                    return;

                if (success) {
                    if (resp.contains("token") && resp["token"].is_string()) {
                        self->m_token = QString::fromStdString(resp["token"].get<std::string>());
                    } else {
                        obs_log(LOG_ERROR, "Got Ably token response error: %s",
                                resp.dump().c_str());
                    }

                    if (!self->m_token.isEmpty()) {
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
                if (callback)
                    callback(false);
            },
            Qt::QueuedConnection);
    }).detach();
}
