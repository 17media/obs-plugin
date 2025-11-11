#include "OneSevenLiveTwitchAuth.hpp"
#include "OneSevenLiveTwitchClient.hpp"
#include "plugin-support.h"
#include "utility/RemoteTextThread.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include <QTimer>
#include <QDateTime>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QTimerEvent>
#include <QRandomGenerator>
#include <QMessageBox>

#include <obs-module.h>

// https://id.twitch.tv/oauth2/authorize?response_type=code&client_id=hof5gwx0su6owfnys0nyan9c87zr6t&redirect_uri=http://localhost:3000&scope=channel%3Amanage%3Apolls+channel%3Aread%3Apolls&state=c3ab8aa609ea11e793ae92361f002671
const QString OneSevenLiveTwitchAuth::TWITCH_DEVICE_AUTH_URL = "https://id.twitch.tv/oauth2/authorize?response_type=token&client_id=%1&redirect_uri=%2&scope=%3&state=%4";
const QString OneSevenLiveTwitchAuth::TWITCH_TOKEN_URL = "https://id.twitch.tv/oauth2/token";
const QString OneSevenLiveTwitchAuth::TWITCH_SCOPE = "channel:read:stream_key channel:manage:broadcast user:read:email chat:read chat:edit";

OneSevenLiveTwitchAuth::OneSevenLiveTwitchAuth(QObject* parent)
    : QObject(parent)
    , m_pollingTimer(new QTimer(this))
    , m_expiresIn(0)
    , m_interval(5)
    , m_remainingTime(0)
    , m_isAuthorizing(false)
    , m_isPolling(false)
    , m_wasCancelled(false)
    , m_twitchClient(std::make_unique<OneSevenLiveTwitchClient>(this))
{
    connect(m_pollingTimer, &QTimer::timeout, this, &OneSevenLiveTwitchAuth::pollForToken);
    
    // Connect Twitch client signals to handle user info retrieval
    connect(m_twitchClient.get(), &OneSevenLiveTwitchClient::userInfoReceived, this, [this](const TwitchUserInfo& userInfo) {
        obs_log(LOG_INFO, "Twitch user info received for: %s", userInfo.login.toUtf8().constData());
        
        // Save user info to config manager
        auto* configManager = OneSevenLiveCoreManager::getInstance().getConfigManager();
        if (configManager) {
            configManager->setTwitchUserInfo(
                userInfo.id,
                userInfo.login,
                userInfo.displayName,
                userInfo.profileImageUrl,
                userInfo.email,
                userInfo.viewCount
            );
        }
    });
    
    connect(m_twitchClient.get(), &OneSevenLiveTwitchClient::errorOccurred, this, [](const QString& errorMessage) {
        obs_log(LOG_ERROR, "Twitch API client error: %s", errorMessage.toUtf8().constData());
    });
}

OneSevenLiveTwitchAuth::~OneSevenLiveTwitchAuth()
{
    stopPolling();
}

QString OneSevenLiveTwitchAuth::getAuthUrl(const QString& redirectUri)
{
    return TWITCH_DEVICE_AUTH_URL.arg(getClientId(), redirectUri, getScope(), getState());
}

QString OneSevenLiveTwitchAuth::getState()
{
    // Generate a 32-hex-character CSRF state if not present
    if (m_state.isEmpty()) {
        QByteArray bytes;
        bytes.resize(16); // 128-bit random
        for (int i = 0; i < bytes.size(); ++i) {
            bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
        }
        m_state = QString::fromLatin1(bytes.toHex());
    }
    return m_state;
}

bool OneSevenLiveTwitchAuth::validateState(const QString& state) const
{
    return !m_state.isEmpty() && state == m_state;
}

void OneSevenLiveTwitchAuth::startDeviceCodeFlow()
{
    if (m_isAuthorizing) {
        obs_log(LOG_WARNING, "Twitch authorization already in progress");
        return;
    }
    
    m_isAuthorizing = true;
    m_wasCancelled = false;
    emit authorizationStarted();
    
    requestDeviceCode();
}

void OneSevenLiveTwitchAuth::cancelAuthorization()
{
    if (!m_isAuthorizing) {
        return;
    }
    
    m_wasCancelled = true;
    m_isAuthorizing = false;
    stopPolling();
    
    emit authorizationCancelled();
}

bool OneSevenLiveTwitchAuth::hasValidToken() const
{
    return !m_accessToken.isEmpty();
}

void OneSevenLiveTwitchAuth::setTokens(const QString& accessToken, const QString& refreshToken)
{
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;
}

void OneSevenLiveTwitchAuth::clearTokens()
{
    m_accessToken.clear();
    m_refreshToken.clear();
}

void OneSevenLiveTwitchAuth::requestDeviceCode()
{
    obs_log(LOG_INFO, "Requesting Twitch device code");

    QUrlQuery query;
    query.addQueryItem("client_id", getClientId());
    query.addQueryItem("scope", getScope());
    QByteArray postData = query.query(QUrl::FullyEncoded).toUtf8();

    RemoteTextThread* thread = new RemoteTextThread(
        TWITCH_DEVICE_AUTH_URL.toStdString(),
        "application/x-www-form-urlencoded",
        std::string(postData.constData(), postData.size()),
        /*timeoutSec=*/15,
        /*isImageRequest=*/false);

    connect(thread, &RemoteTextThread::Result, this, &OneSevenLiveTwitchAuth::onDeviceCodeResult);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveTwitchAuth::onDeviceCodeResult(const QString& text, const QString& error)
{
    if (m_wasCancelled) {
        return;
    }

    if (!error.isEmpty()) {
        QString errorMsg = QString("Device code request failed: %1").arg(error);
        obs_log(LOG_ERROR, "Twitch device code request error: %s", errorMsg.toUtf8().constData());
        emit authorizationFailed(errorMsg);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    QJsonObject json = doc.object();

    if (json.contains("error")) {
        QString err = json["error"].toString();
        QString errorDescription = json["error_description"].toString();
        QString errorMsg = QString("%1: %2").arg(err, errorDescription);
        obs_log(LOG_ERROR, "Twitch device code error: %s", errorMsg.toUtf8().constData());
        emit authorizationFailed(errorMsg);
        return;
    }

    m_deviceCode = json["device_code"].toString();
    m_userCode = json["user_code"].toString();
    m_verificationUri = json["verification_uri"].toString();
    m_verificationUriComplete = json["verification_uri_complete"].toString();
    m_expiresIn = json["expires_in"].toInt();
    m_interval = json["interval"].toInt();

    if (m_deviceCode.isEmpty() || m_userCode.isEmpty() || m_verificationUri.isEmpty()) {
        emit authorizationFailed("Invalid device code response from Twitch");
        return;
    }

    obs_log(LOG_INFO, "Twitch device code received successfully");
    emit deviceCodeReceived(m_userCode, m_verificationUri, m_verificationUriComplete);

    // Start polling for token
    startPolling();
}

void OneSevenLiveTwitchAuth::startPolling()
{
    if (m_isPolling) {
        return;
    }
    
    m_isPolling = true;
    m_remainingTime = m_expiresIn;
    
    m_pollingTimer->start(m_interval * 1000); // Convert to milliseconds
    emit pollingStarted(m_interval);
    
    // Start the first poll immediately
    pollForToken();
}

void OneSevenLiveTwitchAuth::stopPolling()
{
    if (!m_isPolling) {
        return;
    }
    
    m_isPolling = false;
    m_pollingTimer->stop();
}

void OneSevenLiveTwitchAuth::pollForToken()
{
    if (m_wasCancelled || !m_isPolling) {
        return;
    }
    
    m_remainingTime -= m_interval;
    emit pollingProgress(m_remainingTime);
    
    if (m_remainingTime <= 0) {
        stopPolling();
        m_isAuthorizing = false;
        emit authorizationFailed("Authorization timeout - device code expired");
        return;
    }
    
    requestToken();
}

void OneSevenLiveTwitchAuth::requestToken()
{
    QUrlQuery query;
    query.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");
    query.addQueryItem("device_code", m_deviceCode);
    query.addQueryItem("client_id", getClientId());
    QByteArray postData = query.query(QUrl::FullyEncoded).toUtf8();

    RemoteTextThread* thread = new RemoteTextThread(
        TWITCH_TOKEN_URL.toStdString(),
        "application/x-www-form-urlencoded",
        std::string(postData.constData(), postData.size()),
        /*timeoutSec=*/15,
        /*isImageRequest=*/false);

    connect(thread, &RemoteTextThread::Result, this, &OneSevenLiveTwitchAuth::onTokenResult);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveTwitchAuth::onTokenResult(const QString& text, const QString& error)
{
    if (m_wasCancelled) {
        return;
    }
    if (!error.isEmpty()) {
        QString errorMsg = QString("Token request failed: %1").arg(error);
        obs_log(LOG_ERROR, "Twitch token request error: %s", errorMsg.toUtf8().constData());
        stopPolling();
        m_isAuthorizing = false;
        emit authorizationFailed(errorMsg);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    QJsonObject json = doc.object();

    if (json.contains("error")) {
        QString err = json["error"].toString();
        if (err == "authorization_pending") {
            return; // continue polling
        } else if (err == "slow_down") {
            m_interval += 5;
            m_pollingTimer->setInterval(m_interval * 1000);
            return;
        } else if (err == "expired_token") {
            stopPolling();
            m_isAuthorizing = false;
            emit authorizationFailed("Device code expired - please restart authorization");
            return;
        } else {
            QString errorDescription = json["error_description"].toString();
            QString errorMsg = QString("%1: %2").arg(err, errorDescription);
            obs_log(LOG_ERROR, "Twitch token request error: %s", errorMsg.toUtf8().constData());
            stopPolling();
            m_isAuthorizing = false;
            emit authorizationFailed(errorMsg);
            return;
        }
    }

    if (json.contains("access_token")) {
        m_accessToken = json["access_token"].toString();
        m_refreshToken = json["refresh_token"].toString();
        obs_log(LOG_INFO, "Twitch authorization completed successfully");
        stopPolling();
        m_isAuthorizing = false;
        
        // Initialize Twitch client with the access token and fetch user info
        if (m_twitchClient && !m_accessToken.isEmpty()) {
            m_twitchClient->setAuthData(m_accessToken, getClientId());
            m_twitchClient->getCurrentUser();
        }
        
        emit authorizationCompleted(m_accessToken, m_refreshToken);
    } else {
        emit authorizationFailed("Invalid token response from Twitch");
    }
}


QString OneSevenLiveTwitchAuth::getClientId() const
{
    return QString(TWITCH_API_CLIENT_ID);
}

QString OneSevenLiveTwitchAuth::getScope() const
{
    return TWITCH_SCOPE;
}

// getTwitchClient is defined inline in the header; no out-of-line definition needed.

bool OneSevenLiveTwitchAuth::handleAuthorizationCallbackUrl(const QString& callbackUrl)
{
    QUrl url(callbackUrl);
    if (!url.isValid()) {
        obs_log(LOG_WARNING, "Twitch callback URL invalid: %s", callbackUrl.toUtf8().constData());
        return false;
    }

    // If the callback contains error parameters, notify user and fail
    const QUrlQuery query(url.query());
    const QString error = query.queryItemValue("error");
    const QString errorDescription = query.queryItemValue("error_description");
    if (!error.isEmpty()) {
        const QString desc = errorDescription.isEmpty() ? error : errorDescription;
        obs_log(LOG_WARNING, "Twitch authorization error: %s - %s",
                error.toUtf8().constData(), desc.toUtf8().constData());
        QMessageBox::warning(nullptr, obs_module_text("Live.Common.Notice"),
                             QString("Twitch authorization failed: %1").arg(desc));
        emit authorizationFailed(desc);
        return false;
    }

    // Validate expected origin: only localhost:3000 is accepted for implicit flow
    const QString origin = url.scheme() + "://" + url.host() +
                           (url.port() != -1 ? (":" + QString::number(url.port())) : QString()) + "/";
    const bool originIsLocalhost = (origin == "http://localhost:3000/");

    // Support implicit grant style: http://localhost:3000/#access_token=...&scope=...&state=...&token_type=bearer
    const QString fragment = url.fragment();
    if (!fragment.isEmpty()) {
        QUrlQuery fragQuery(fragment);
        const QString accessToken = fragQuery.queryItemValue("access_token");
        const QString tokenType = fragQuery.queryItemValue("token_type");
        const QString scope = fragQuery.queryItemValue("scope");
        const QString state = fragQuery.queryItemValue("state");

        if (accessToken.isEmpty()) {
            obs_log(LOG_WARNING, "Twitch implicit callback missing 'access_token' in fragment");
            return false;
        }

        // If origin is unexpected, treat as error and fail
        if (!originIsLocalhost) {
            obs_log(LOG_WARNING, "Twitch callback origin unexpected: %s",
                    origin.toUtf8().constData());
            return false;
        }

        // Validate CSRF state if present (warn only)
        if (!state.isEmpty() && !validateState(state)) {
            obs_log(LOG_WARNING, "Twitch callback state mismatch: expected=%s got=%s",
                    m_state.toUtf8().constData(), state.toUtf8().constData());
        }

        // Persist access token and fetched time
        auto* cfg = OneSevenLiveCoreManager::getInstance().getConfigManager();
        if (cfg && cfg->initialize()) {
            const qint64 fetchedAt = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
            // Save token (no refresh token in implicit flow)
            if (!cfg->setTwitchTokens(accessToken, fetchedAt)) {
                obs_log(LOG_ERROR, "Failed to save Twitch access token to config.ini");
            }
        } else {
            obs_log(LOG_ERROR, "ConfigManager not initialized; cannot persist Twitch token");
        }

        // Update local state and notify
        setTokens(accessToken, "");
        m_callbackScope = scope;
        obs_log(LOG_INFO, "Twitch implicit callback parsed: access_token set, scope=%s token_type=%s",
                m_callbackScope.toUtf8().constData(), tokenType.toUtf8().constData());
        
        // Initialize Twitch client with the access token and fetch user info
        if (m_twitchClient && !m_accessToken.isEmpty()) {
            m_twitchClient->setAuthData(m_accessToken, getClientId());
            m_twitchClient->getCurrentUser();
        }
        
        emit authorizationCompleted(m_accessToken, m_refreshToken);
        return true;
    }

    // No fragment and no explicit error -> treat as unexpected format
    obs_log(LOG_WARNING, "Twitch callback URL does not contain expected fragment or error: %s",
            callbackUrl.toUtf8().constData());
    return false;
}
