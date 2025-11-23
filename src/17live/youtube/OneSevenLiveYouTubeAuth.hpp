#pragma once

#include <QObject>
#include <QString>
#include <memory>

class QTimer;

/**
 * YouTube authorization handler using implicit grant flow
 * Encapsulates building auth URL and handling the callback to persist tokens.
 */
class OneSevenLiveYouTubeAuth : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveYouTubeAuth(QObject* parent = nullptr);
    ~OneSevenLiveYouTubeAuth();

    // Build authorization URL for the given redirect URI
    QString getAuthUrl(const QString& redirectUri);

    // CSRF state helpers
    QString getState();
    bool validateState(const QString& state) const;

    // Callback handler: parse query and persist access token
    // Returns true on success; false on error or unexpected format
    bool handleAuthorizationCallbackUrl(const QString& callbackUrl);

    // Refresh the access token using stored refresh_token
    // Returns true on success; persists new token and updates in-memory state
    bool refreshAccessToken();
    void refreshAccessTokenAsync();

    // Schedule auto-refresh 1 minute before access token expiry
    // If already expired on startup, refresh immediately if refresh_token is valid,
    // otherwise clear tokens from config
    void scheduleAutoRefresh(int accessExpiresInSec, qint64 accessFetchedAtEpochSec,
                             int refreshExpiresInSec, qint64 refreshFetchedAtEpochSec);
    void stopAutoRefresh();

    QString getRedirectUri() const {
        return m_redirectUri;
    }

    // Token state
    bool hasValidToken() const;

    QString getAccessToken() const {
        return m_accessToken;
    }

    void setAccessToken(const QString& token);
    void clearToken();

   signals:
    void authorizationCompleted(const QString& accessToken);
    void authorizationFailed(const QString& error);

   private:
    QString getClientId() const;
    QString getClientSecret() const;
    QString getScope() const;

    // Internal state
    QString m_state;
    QString m_accessToken;
    QString m_callbackScope;
    QString m_redirectUri;
    QString m_refreshToken;
    QTimer* m_refreshTimer{nullptr};

   public:
    // Constants
    static const QString YT_AUTH_URL_TEMPLATE;
    static const QString YT_SCOPE;
    static const QString YT_TOKEN_URL;
    static const QString PLATFORM;

   private slots:
    void onRefreshTimerTimeout();
};
