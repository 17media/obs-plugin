#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <nlohmann/json.hpp>

struct TwitchUserInfo {
    QString id;
    QString login;
    QString displayName;
    QString type;
    QString broadcasterType;
    QString description;
    QString profileImageUrl;
    QString offlineImageUrl;
    qint64 viewCount;
    QString email;
    QString createdAt;

    TwitchUserInfo() : viewCount(0) {}
};

struct TwitchChannelInfo {
    QString broadcasterId;
    QString broadcasterLogin;
    QString broadcasterName;
    QString gameName;
    QString gameId;
    QString broadcasterLanguage;
    QString title;
    qint64 delay;

    TwitchChannelInfo() : delay(0) {}
};

class OneSevenLiveTwitchClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveTwitchClient(QObject* parent = nullptr);
    ~OneSevenLiveTwitchClient();

    // Authentication
    void setAuthData(const QString& accessToken, const QString& clientId);
    bool hasValidAuth() const;

    // User information API
    void getCurrentUser();
    void getUserById(const QString& userId);
    void getUserByLogin(const QString& login);

    // Channel information API
    void getChannelInformation(const QString& broadcasterId);

    // Stream key API
    void getStreamKey(const QString& broadcasterId);

    // Get cached user info
    TwitchUserInfo getCachedUserInfo() const {
        return m_cachedUserInfo;
    }

    TwitchChannelInfo getCachedChannelInfo() const {
        return m_cachedChannelInfo;
    }

   signals:
    void userInfoReceived(const TwitchUserInfo& userInfo);
    void channelInfoReceived(const TwitchChannelInfo& channelInfo);
    void errorOccurred(const QString& errorMessage);
    void streamKeyReceived(const QString& streamKey);

   private slots:
    void onUserInfoResult(const QString& text, const QString& error);
    void onChannelInfoResult(const QString& text, const QString& error);
    void onStreamKeyResult(const QString& text, const QString& error);

   private:
    void makeApiRequest(const QString& endpoint, const QString& query = QString());
    TwitchUserInfo parseUserInfo(const nlohmann::json& userObj);
    TwitchChannelInfo parseChannelInfo(const nlohmann::json& channelObj);
    void clearCache();

    // Authentication data
    QString m_accessToken;
    QString m_clientId;

    // Cached data
    TwitchUserInfo m_cachedUserInfo;
    TwitchChannelInfo m_cachedChannelInfo;

    // API endpoints
    static const QString TWITCH_HELIX_API_BASE;
    static const QString TWITCH_USERS_ENDPOINT;
    static const QString TWITCH_CHANNELS_ENDPOINT;
    static const QString TWITCH_STREAM_KEY_ENDPOINT;

   public:
    static const QString TWITCH_RTMP_SERVER;
};
