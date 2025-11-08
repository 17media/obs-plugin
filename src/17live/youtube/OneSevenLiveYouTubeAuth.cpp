#include "OneSevenLiveYouTubeAuth.hpp"

#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>
#include <QUrl>
#include <QUrlQuery>

#include "plugin-support.h"
#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveConfigManager.hpp"
#include "../utility/RemoteTextThread.hpp"

#include <obs-module.h>
#include <nlohmann/json.hpp>

using Json = nlohmann::json;

const QString OneSevenLiveYouTubeAuth::YT_AUTH_URL_TEMPLATE =
    "https://accounts.google.com/o/oauth2/v2/auth?scope=%1&response_type=code&state=%2&redirect_uri=%3&client_id=%4";
const QString OneSevenLiveYouTubeAuth::YT_SCOPE =
    "https://www.googleapis.com/auth/youtube.force-ssl";
const QString OneSevenLiveYouTubeAuth::YT_TOKEN_URL = "https://oauth2.googleapis.com/token";

OneSevenLiveYouTubeAuth::OneSevenLiveYouTubeAuth(QObject* parent)
    : QObject(parent) {}

OneSevenLiveYouTubeAuth::~OneSevenLiveYouTubeAuth() {}

QString OneSevenLiveYouTubeAuth::getAuthUrl(const QString& redirectUri)
{
    m_redirectUri = redirectUri;
    return YT_AUTH_URL_TEMPLATE.arg(getScope(), getState(), redirectUri, getClientId());
}

QString OneSevenLiveYouTubeAuth::getState()
{
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

bool OneSevenLiveYouTubeAuth::validateState(const QString& state) const
{
    return !m_state.isEmpty() && state == m_state;
}

bool OneSevenLiveYouTubeAuth::hasValidToken() const
{
    return !m_accessToken.isEmpty();
}

void OneSevenLiveYouTubeAuth::setAccessToken(const QString& token)
{
    m_accessToken = token;
}

void OneSevenLiveYouTubeAuth::clearToken()
{
    m_accessToken.clear();
}

void OneSevenLiveYouTubeAuth::handleAuthorizationCallbackUrl(const QString& callbackUrl)
{
    QUrl url(callbackUrl);
    if (!url.isValid()) {
        obs_log(LOG_WARNING, "YouTube callback URL invalid: %s", callbackUrl.toUtf8().constData());
        return;
    }


    const QUrlQuery query(url.query());
    const QString code = query.queryItemValue("code");
    const QString state = query.queryItemValue("state");
    const QString scope = query.queryItemValue("scope");

    if (code.isEmpty()) {
        obs_log(LOG_WARNING, "YouTube authorization code not found in callback query");
        emit authorizationFailed("Authorization code missing in callback");
        return;
    }

    if (!state.isEmpty() && !validateState(state)) {
        obs_log(LOG_WARNING, "YouTube callback state mismatch: expected=%s got=%s",
                m_state.toUtf8().constData(), state.toUtf8().constData());
    }

    // Exchange code for tokens
    const QString tokenUrl = YT_TOKEN_URL;
    
    const QByteArray codeEnc = QUrl::toPercentEncoding(code);
    const QByteArray clientIdEnc = QUrl::toPercentEncoding(getClientId());
    const QByteArray clientSecretEnc = QUrl::toPercentEncoding(getClientSecret());
    const QByteArray redirectEnc = QUrl::toPercentEncoding(m_redirectUri);

    std::string postData = QString("code=%1&client_id=%2&client_secret=%3&redirect_uri=%4&grant_type=authorization_code")
                               .arg(QString::fromUtf8(codeEnc), QString::fromUtf8(clientIdEnc), QString::fromUtf8(clientSecretEnc), QString::fromUtf8(redirectEnc))
                               .toStdString();

    std::string responseBody;
    std::string error;
    long httpStatusCode = 0;
    bool ok = GetRemoteFile(tokenUrl.toUtf8().constData(), responseBody, error, &httpStatusCode,
                            "application/x-www-form-urlencoded", "POST", postData.c_str(),
                            std::vector<std::string>(), nullptr, /*timeout*/ 0, /*fail_on_error*/ true,
                            static_cast<int>(postData.size()));

    if (!ok || httpStatusCode < 200 || httpStatusCode >= 300) {
        obs_log(LOG_ERROR, "YouTube token exchange failed (HTTP %ld): %s", httpStatusCode,
                error.c_str());
        emit authorizationFailed(QString::fromUtf8(error.c_str()));
        return;
    }

    // Parse JSON response
    QString accessToken;
    int expiresIn = 0;
    QString tokenType;
    QString refreshToken;
    int refreshTokenExpiresIn = 0;

    try {
        Json json = Json::parse(responseBody);
        if (json.contains("access_token") && json["access_token"].is_string()) {
            accessToken = QString::fromStdString(json["access_token"].get<std::string>());
        }
        if (json.contains("expires_in") && json["expires_in"].is_number_integer()) {
            expiresIn = json["expires_in"].get<int>();
        }
        if (json.contains("token_type") && json["token_type"].is_string()) {
            tokenType = QString::fromStdString(json["token_type"].get<std::string>());
        }
        if (json.contains("refresh_token") && json["refresh_token"].is_string()) {
            refreshToken = QString::fromStdString(json["refresh_token"].get<std::string>());
        }
        if (json.contains("refresh_token_expires_in") && json["refresh_token_expires_in"].is_number_integer()) {
            refreshTokenExpiresIn = json["refresh_token_expires_in"].get<int>();
        }
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "Failed to parse YouTube token JSON: %s", e.what());
        emit authorizationFailed("Failed to parse token response");
        return;
    }

    if (accessToken.isEmpty()) {
        obs_log(LOG_ERROR, "YouTube token exchange did not return access_token");
        emit authorizationFailed("Token exchange missing access_token");
        return;
    }

    // Persist token, fetched time, and expires_in
    OneSevenLiveConfigManager* cfg = OneSevenLiveCoreManager::getInstance().getConfigManager();
    if (!cfg || !cfg->initialize()) {
        obs_log(LOG_ERROR, "ConfigManager not initialized; cannot save YouTube token");
        emit authorizationFailed("Configuration manager not initialized");
        return;
    }

    const qint64 nowEpoch = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    if (!cfg->setYouTubeAccessToken(accessToken, expiresIn, nowEpoch)) {
        obs_log(LOG_ERROR, "Failed to save YouTube access token");
        emit authorizationFailed("Failed to save YouTube access token");
        return;
    }
    if (!cfg->setYouTubeRefreshToken(refreshToken, refreshTokenExpiresIn, nowEpoch)) {
        obs_log(LOG_ERROR, "Failed to save YouTube refresh token");
        emit authorizationFailed("Failed to save YouTube refresh token");
        return;
    }

    // Update local state and notify
    setAccessToken(accessToken);
    m_refreshToken = refreshToken;
    m_callbackScope = scope;
    obs_log(LOG_INFO, "YouTube token exchange success: scope=%s token_type=%s expires_in=%d",
            m_callbackScope.toUtf8().constData(), tokenType.toUtf8().constData(), expiresIn);

    // Schedule auto refresh one minute before expiry
    scheduleAutoRefresh(expiresIn, nowEpoch, refreshTokenExpiresIn, nowEpoch);

    emit authorizationCompleted(m_accessToken);
}

QString OneSevenLiveYouTubeAuth::getClientId() const
{
    return QString(YOUTUBE_API_CLIENT_ID);
}

QString OneSevenLiveYouTubeAuth::getClientSecret() const
{
    return QString(YOUTUBE_API_CLIENT_SECRET);
}

QString OneSevenLiveYouTubeAuth::getScope() const
{
    return YT_SCOPE;
}

bool OneSevenLiveYouTubeAuth::refreshAccessToken()
{
    // Acquire refresh_token from memory or config
    QString rt = m_refreshToken;
    OneSevenLiveConfigManager* cfg = OneSevenLiveCoreManager::getInstance().getConfigManager();
    if (rt.isEmpty()) {
        if (!cfg || !cfg->initialize()) {
            obs_log(LOG_ERROR, "ConfigManager not initialized; cannot refresh YouTube token");
            return false;
        }
        QString cfgRt;
        int rtExpiresIn = 0;
        qint64 rtFetched = 0;
        if (!cfg->getYouTubeRefreshToken(cfgRt, rtExpiresIn, rtFetched)) {
            obs_log(LOG_ERROR, "No YouTube refresh token available in config");
            return false;
        }
        rt = cfgRt;
    }

    if (rt.isEmpty()) {
        obs_log(LOG_ERROR, "YouTube refresh token is empty; cannot refresh");
        return false;
    }

    // Build POST body per Google OAuth refresh flow
    const QByteArray clientIdEnc = QUrl::toPercentEncoding(getClientId());
    const QByteArray refreshEnc = QUrl::toPercentEncoding(rt);
    std::string postData = QString("client_id=%1&refresh_token=%2&grant_type=refresh_token")
                               .arg(QString::fromUtf8(clientIdEnc), QString::fromUtf8(refreshEnc))
                               .toStdString();

    std::string responseBody;
    std::string error;
    long httpStatusCode = 0;
    bool ok = GetRemoteFile(YT_TOKEN_URL.toUtf8().constData(), responseBody, error, &httpStatusCode,
                            "application/x-www-form-urlencoded", "POST", postData.c_str(),
                            std::vector<std::string>(), nullptr, /*timeout*/ 0, /*fail_on_error*/ true,
                            static_cast<int>(postData.size()));

    if (!ok || httpStatusCode < 200 || httpStatusCode >= 300) {
        obs_log(LOG_ERROR, "YouTube token refresh failed (HTTP %ld): %s", httpStatusCode, error.c_str());
        return false;
    }

    // Parse refreshed token response
    QString newAccessToken;
    int expiresIn = 0;
    QString tokenType;
    QString scope;
    try {
        Json json = Json::parse(responseBody);
        if (json.contains("access_token") && json["access_token"].is_string()) {
            newAccessToken = QString::fromStdString(json["access_token"].get<std::string>());
        }
        if (json.contains("expires_in") && json["expires_in"].is_number_integer()) {
            expiresIn = json["expires_in"].get<int>();
        }
        if (json.contains("token_type") && json["token_type"].is_string()) {
            tokenType = QString::fromStdString(json["token_type"].get<std::string>());
        }
        if (json.contains("scope") && json["scope"].is_string()) {
            scope = QString::fromStdString(json["scope"].get<std::string>());
        }
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "Failed to parse YouTube refresh JSON: %s", e.what());
        return false;
    }

    if (newAccessToken.isEmpty()) {
        obs_log(LOG_ERROR, "YouTube token refresh did not return access_token");
        return false;
    }

    // Persist refreshed access_token with expires_in and fetched time
    if (!cfg || !cfg->initialize()) {
        cfg = OneSevenLiveCoreManager::getInstance().getConfigManager();
        if (!cfg || !cfg->initialize()) {
            obs_log(LOG_ERROR, "ConfigManager not initialized; cannot persist refreshed token");
            return false;
        }
    }

    const qint64 nowEpoch = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    if (!cfg->setYouTubeAccessToken(newAccessToken, expiresIn, nowEpoch)) {
        obs_log(LOG_ERROR, "Failed to save refreshed YouTube access token");
        return false;
    }

    // Update in-memory token
    setAccessToken(newAccessToken);
    m_callbackScope = scope;
    obs_log(LOG_INFO, "YouTube token refreshed: token_type=%s expires_in=%d",
            tokenType.toUtf8().constData(), expiresIn);
    // Notify listeners using existing signal for simplicity
    emit authorizationCompleted(m_accessToken);
    return true;
}

void OneSevenLiveYouTubeAuth::scheduleAutoRefresh(int accessExpiresInSec, qint64 accessFetchedAtEpochSec,
                                                  int refreshExpiresInSec, qint64 refreshFetchedAtEpochSec)
{
    OneSevenLiveConfigManager* cfg = OneSevenLiveCoreManager::getInstance().getConfigManager();

    // Load refresh token from config if missing
    if (m_refreshToken.isEmpty() && cfg && cfg->initialize()) {
        QString rt;
        int rtExp{0};
        qint64 rtFetched{0};
        if (cfg->getYouTubeRefreshToken(rt, rtExp, rtFetched)) {
            m_refreshToken = rt;
            if (refreshExpiresInSec <= 0) refreshExpiresInSec = rtExp;
            if (refreshFetchedAtEpochSec <= 0) refreshFetchedAtEpochSec = rtFetched;
        }
    }

    const qint64 nowEpoch = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const qint64 accessExpiresAt = accessFetchedAtEpochSec + accessExpiresInSec;
    const qint64 refreshExpiresAt = refreshFetchedAtEpochSec + refreshExpiresInSec;

    // Handle already-expired access token on startup
    if (accessExpiresInSec > 0 && nowEpoch >= accessExpiresAt) {
        if (!m_refreshToken.isEmpty() && refreshExpiresInSec > 0 && nowEpoch < refreshExpiresAt) {
            obs_log(LOG_INFO, "YouTube access token expired; attempting immediate refresh");
            if (!refreshAccessToken()) {
                obs_log(LOG_ERROR, "YouTube immediate refresh failed");
            }
        } else if (cfg && cfg->initialize()) {
            obs_log(LOG_INFO, "YouTube refresh token expired or missing; clearing stored tokens");
            cfg->clearYouTubeAccessToken();
            cfg->clearYouTubeRefreshToken();
        }
        return;
    }

    // Schedule one minute before expiry
    if (accessExpiresInSec > 0 && accessFetchedAtEpochSec > 0) {
        const qint64 refreshAt = accessExpiresAt - 60; // 1 minute before
        qint64 delaySec = refreshAt - nowEpoch;
        if (delaySec < 0) delaySec = 0;

        if (!m_refreshTimer) {
            m_refreshTimer = new QTimer(this);
            m_refreshTimer->setSingleShot(true);
            connect(m_refreshTimer, &QTimer::timeout, this, &OneSevenLiveYouTubeAuth::onRefreshTimerTimeout);
        }

        if (!m_refreshToken.isEmpty()) {
            obs_log(LOG_INFO, "Scheduling YouTube access token refresh in %lld sec", (long long)delaySec);
            m_refreshTimer->start(static_cast<int>(delaySec * 1000));
        } else {
            obs_log(LOG_INFO, "No YouTube refresh token; auto-refresh not scheduled");
        }
    }
}

void OneSevenLiveYouTubeAuth::stopAutoRefresh()
{
    if (m_refreshTimer && m_refreshTimer->isActive()) {
        m_refreshTimer->stop();
    }
}

void OneSevenLiveYouTubeAuth::onRefreshTimerTimeout()
{
    if (!refreshAccessToken()) {
        obs_log(LOG_ERROR, "Scheduled YouTube token refresh failed");
    }
}
