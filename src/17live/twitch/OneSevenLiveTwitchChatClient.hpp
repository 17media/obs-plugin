#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <functional>
#include <memory>
#include <string>

class OneSevenLiveWebsocketClient;

namespace ably {}
class OneSevenLiveStreamManager;
enum class OneSevenLiveStreamingStatus;

enum class TwitchMessageType {
    Chat,
    Subscription,
    Resubscription,
    GiftSubscription,
    Raid,
    Host,
    Whisper,
    Notice,
    UserNotice,
    RoomState,
    UserState,
    GlobalUserState,
    Unknown
};

struct TwitchChatMessage {
    QString id;
    QString channel;
    QString username;
    QString displayName;
    QString message;
    QString userId;
    QString color;
    QDateTime timestamp;
    bool isModerator;
    bool isSubscriber;
    bool isTurbo;
    bool isFirstMessage;
    bool isReturningChatter;
    int bits;
    QString emotes;
    QString badges;
    TwitchMessageType type;

    TwitchChatMessage()
        : isModerator(false),
          isSubscriber(false),
          isTurbo(false),
          isFirstMessage(false),
          isReturningChatter(false),
          bits(0),
          type(TwitchMessageType::Chat) {}
};

struct TwitchChatUser {
    QString userId;
    QString username;
    QString displayName;
    QString color;
    bool isModerator;
    bool isSubscriber;
    bool isTurbo;

    TwitchChatUser() : isModerator(false), isSubscriber(false), isTurbo(false) {}
};

class OneSevenLiveTwitchChatClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveTwitchChatClient(QObject* parent = nullptr);
    ~OneSevenLiveTwitchChatClient();

    // Connection management
    void connectToChat(const QString& username, const QString& oauthToken);
    void disconnectFromChat();
    bool isConnected() const;

    // Channel management
    void joinChannel(const QString& channel);
    void leaveChannel(const QString& channel);
    void leaveAllChannels();
    QVector<QString> getJoinedChannels() const;

    // Message sending
    void sendMessage(const QString& channel, const QString& message);
    void sendWhisper(const QString& username, const QString& message);

    // Configuration
    void setAutoReconnect(bool enabled);
    void setReconnectDelay(int seconds);
    void setPingInterval(int seconds);
    void setTargetChannel(const QString& channel);

   signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void messageReceived(const TwitchChatMessage& message);
    void userJoined(const QString& channel, const TwitchChatUser& user);
    void userLeft(const QString& channel, const QString& username);
    void channelJoined(const QString& channel);
    void channelLeft(const QString& channel);
    void subscriptionReceived(const TwitchChatMessage& message);
    void raidReceived(const QString& channel, const QString& raider, int viewerCount);
    void noticeReceived(const QString& channel, const QString& message);
    void reconnecting(int attempt);
    void reconnected();

   private slots:
    void onWebSocketMessage(const std::string& message);
    void onWebSocketOpen();
    void onWebSocketClose();
    void onWebSocketError(const std::string& error);
    void onPingTimeout();
    void attemptReconnect();
    void onStatusTimer();

   private:
    void sendRawMessage(const QString& message);
    void sendIRCCommand(const QString& command, const QString& parameters);
    void authenticate();
    void requestCapabilities();
    void startPingTimer();
    void stopPingTimer();

    // Message parsing
    void parseIRCMessage(const QString& rawMessage);
    TwitchChatMessage parseChatMessage(const QString& rawMessage, const QString& prefix);
    QString extractUsernameFromPrefix(const QString& prefix);

    // Channel management
    bool isChannelJoined(const QString& channel) const;
    QString normalizeChannelName(const QString& channel) const;

    // Reconnection logic
    void scheduleReconnect();
    void resetReconnectAttempts();

    void connectWebSocket();
    void disconnectWebSocket();
    void sendWebSocketMessage(const std::string& message);

    // Connection state
    bool m_connected;
    QString m_username;
    QString m_oauthToken;
    QString m_targetChannel;
    QVector<QString> m_joinedChannels;
    std::unique_ptr<OneSevenLiveWebsocketClient> m_client;

    // Configuration
    bool m_autoReconnect;
    int m_reconnectDelay;
    int m_pingInterval;
    int m_reconnectAttempts;
    int m_maxReconnectAttempts;

    // Timers
    QTimer* m_pingTimer;
    QTimer* m_reconnectTimer;
    QTimer* m_statusTimer;
    QDateTime m_lastPongTs;

    // Constants
    static const QString TWITCH_IRC_SERVER;
    static const int DEFAULT_PING_INTERVAL;
    static const int DEFAULT_RECONNECT_DELAY;
    static const int MAX_RECONNECT_ATTEMPTS;
    static const int STATUS_BROADCAST_INTERVAL;
    static const int LONG_RETRY_DELAY;

    bool m_streamSignalConnected{false};
};
