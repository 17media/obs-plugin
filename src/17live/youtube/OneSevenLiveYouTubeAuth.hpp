#pragma once

#include <QObject>
#include <QString>
#include <memory>

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

    // Callback handler: parse fragment and persist access token
    void handleAuthorizationCallbackUrl(const QString& callbackUrl);

    // Token state
    bool hasValidToken() const;
    QString getAccessToken() const { return m_accessToken; }
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

    // Constants
    static const QString YT_AUTH_URL_TEMPLATE;
    static const QString YT_SCOPE;
};
