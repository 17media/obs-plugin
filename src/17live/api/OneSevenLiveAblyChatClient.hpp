#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "../websocket/OneSevenLiveWebsocketClient.hpp"
#include "OneSevenLiveAblyProtocol.hpp"
#include "OneSevenLiveApiWrappers.hpp"

class QTimer;

class OneSevenLiveAblyChatClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveAblyChatClient(QObject* parent = nullptr);
    ~OneSevenLiveAblyChatClient();

    void setRoomId(const QString& roomId);
    void setAblyToken(const QString& token);

    void setOnOpen(const std::function<void()>& cb);
    void setOnMessage(const std::function<void(const std::string&)>& cb);
    void setOnClose(const std::function<void()>& cb);
    void setOnError(const std::function<void(const std::string&)>& cb);
    void setAuthCallback(const std::function<bool(const QString&, nlohmann::json&)>& cb);

    bool connect();
    void disconnect();
    bool isConnected() const;
    int lastErrorCode() const;
    void logDiagnostics() const;

   private:
    void tryConnectWithFallbackHosts();
    void attachChannel();
    void handleProtocolMessage(const nlohmann::json& message);
    void handleProtocolError(const nlohmann::json& message);
    void broadcastStatus(bool connected, int errorCode = 0);
    void scheduleReconnect();
    void cancelReconnect();
    void refreshToken(std::function<void(bool)> callback = nullptr);
    void scheduleTokenRefresh(qint64 expiresEpochMs, qint64 issuedEpochMs, qint64 ttlMs);
    void cancelTokenRefresh();
    void sendAuth();
    void configureIdleWatchdog(const nlohmann::json& details);
    void noteInboundActivity();
    void stopIdleWatchdog();

    void fetchTokenAsync(std::function<void(bool)> callback);

    QString m_roomId;
    QString m_token;
    std::unique_ptr<OneSevenLiveWebsocketClient> m_wsClient;
    std::vector<QString> m_hosts;
    int m_hostIndex = 0;
    QTimer* m_reconnectTimer{nullptr};
    int m_reconnectAttempts{0};
    int m_maxReconnectAttempts{10};
    int m_baseReconnectDelayMs{1000};
    bool m_closing{false};
    QTimer* m_tokenRefreshTimer{nullptr};
    QTimer* m_idleTimer{nullptr};
    QElapsedTimer m_lastInboundActivity;
    int m_idleTimeoutMs{0};
    qint64 m_tokenExpiresMs{0};
    int m_tokenRefreshAdvanceMs{60000};
    int m_tokenDefaultTtlMs{3000000};
    QString m_connectionKey;
    QString m_connectionId;
    bool m_attached{false};
    bool m_channelFailed{false};
    bool m_tokenFetchInProgress{false};
    std::uint64_t m_generation{0};
    std::uint64_t m_attachAttempt{0};
    qint64 m_diagLastMessageMs{0};
    std::uint64_t m_diagMessageFrames{0};
    int m_lastErrorCode{0};
    ably::ChannelRecovery m_channelRecovery;

    std::function<void()> m_onOpen;
    std::function<void(const std::string&)> m_onMessage;
    std::function<void()> m_onClose;
    std::function<void(const std::string&)> m_onError;
    std::function<bool(const QString&, nlohmann::json&)> m_authCallback;
};

namespace ably {
    static constexpr int MsgType_COMMENT = 3;              // General comment message
    static constexpr int MsgType_NEW_GIFT = 13;            // Gift animation message
    static constexpr int MsgType_JOIN_ROOM = 18;           // Audience join room message
    static constexpr int MsgType_ENTER_ANIMATION = 27; // Enter animation message
    static constexpr int MsgType_REACT = 28;             // React message
    static constexpr int MsgType_NEW_LUCKYBAG = 32;        // Random gift message
    static constexpr int MsgType_POKE = 47;                // Poke message
    static constexpr int MsgType_ROCKZONE = 74;            // Rock Zone message
    static constexpr int MsgType_LABOR_RECEIVE_REWARD = 79;  // Labor receive reward message
    static constexpr int MsgType_AI_COHOST_MESSAGE = 120;  // AI co-host message
    
}  // namespace ably
