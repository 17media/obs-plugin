#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <functional>
#include <memory>
#include <vector>

#include "../websocket/OneSevenLiveWebsocketClient.hpp"
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

   private:
    void tryConnectWithFallbackHosts();
    void attachChannel();
    void attachChannel(const QString& channel);
    void detachChannel(const QString& channel);
    void publishMessage(const QString& channel, const nlohmann::json& payload);
    void enterPresence(const QString& channel, const nlohmann::json& data);
    void updatePresence(const QString& channel, const nlohmann::json& data);
    void leavePresence(const QString& channel);
    void scheduleReconnect();
    void cancelReconnect();
    void refreshToken(std::function<void(bool)> callback = nullptr);
    void scheduleTokenRefresh(qint64 expiresEpochMs, qint64 issuedEpochMs, qint64 ttlMs);
    void cancelTokenRefresh();
    void sendAuth();
    void sendConnect();
    
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
    qint64 m_tokenExpiresMs{0};
    int m_tokenRefreshAdvanceMs{60000};
    int m_tokenDefaultTtlMs{3000000};
    QString m_connectionKey;
    long long m_lastConnectionSerial{-1};
    bool m_attached{false};
    QSet<QString> m_attachedChannels;
    long long m_msgSerialCounter{0};

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
    static constexpr int MsgType_NEW_LUCKYBAG = 32;        // Random gift message
    static constexpr int MsgType_POKE = 47;                // Poke message
    static constexpr int MsgType_ROCKZONE = 74;            // Rock Zone message
    static constexpr int MsgType_AI_COHOST_MESSAGE = 120;  // AI co-host message
}  // namespace ably
