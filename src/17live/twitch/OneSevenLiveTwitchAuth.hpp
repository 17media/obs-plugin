#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include "../utility/RemoteTextThread.hpp"
#include "OneSevenLiveTwitchClient.hpp"
#include <memory>

/**
 * Twitch authorization handler using device code flow
 * Implements OAuth 2.0 device authorization grant for OBS plugin
 */
class OneSevenLiveTwitchAuth : public QObject {
    Q_OBJECT

public:
    explicit OneSevenLiveTwitchAuth(QObject* parent = nullptr);
    ~OneSevenLiveTwitchAuth();

    QString getAuthUrl(const QString& redirectUri);
    // CSRF state helpers
    QString getState();
    bool validateState(const QString& state) const;

    // Device code flow steps
    void startDeviceCodeFlow();
    void cancelAuthorization();
    
    // Token management
    bool hasValidToken() const;
    QString getAccessToken() const { return m_accessToken; }
    QString getRefreshToken() const { return m_refreshToken; }
    QString getUserCode() const { return m_userCode; }
    QString getVerificationUri() const { return m_verificationUri; }
    QString getVerificationUriComplete() const { return m_verificationUriComplete; }
    
    // Token operations
    void setTokens(const QString& accessToken, const QString& refreshToken);
    void clearTokens();

    // Authorization callback handler: parse token/scope/state or errors from redirect URL
    // Returns true on successful token parsing; false on error or unexpected format
    bool handleAuthorizationCallbackUrl(const QString& callbackUrl);

    // Get Twitch API client instance
    OneSevenLiveTwitchClient* getTwitchClient() { return m_twitchClient.get(); }

signals:
    void deviceCodeReceived(const QString& userCode, const QString& verificationUri, const QString& verificationUriComplete);
    void authorizationStarted();
    void authorizationCompleted(const QString& accessToken, const QString& refreshToken);
    void authorizationFailed(const QString& error);
    void authorizationCancelled();
    void pollingStarted(int intervalSeconds);
    void pollingProgress(int remainingSeconds);

private slots:
    void onDeviceCodeResult(const QString& text, const QString& error);
    void onTokenResult(const QString& text, const QString& error);
    void pollForToken();

private:
    void requestDeviceCode();
    void requestToken();
    void startPolling();
    void stopPolling();
    
    QString getClientId() const;
    QString getScope() const;
    
    // Internal helpers
    QString m_state;

    QTimer* m_pollingTimer;
    
    // Authorization state
    QString m_deviceCode;
    QString m_userCode;
    QString m_verificationUri;
    QString m_verificationUriComplete;
    int m_expiresIn;
    int m_interval;
    int m_remainingTime;
    
    // Token storage
    QString m_accessToken;
    QString m_refreshToken;
    
    // State flags
    bool m_isAuthorizing;
    bool m_isPolling;
    bool m_wasCancelled;

    // Authorization code flow (via redirect)
    QString m_authorizationCode;
    QString m_callbackScope;
    
    // Constants
    static const QString TWITCH_DEVICE_AUTH_URL;
    static const QString TWITCH_TOKEN_URL;
    static const QString TWITCH_SCOPE;

    // Twitch API client
    std::unique_ptr<OneSevenLiveTwitchClient> m_twitchClient;
};
