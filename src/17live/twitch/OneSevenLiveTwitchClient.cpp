#include "OneSevenLiveTwitchClient.hpp"
#include "../utility/RemoteTextThread.hpp"
#include "plugin-support.h"

#include <nlohmann/json.hpp>
#include <QUrlQuery>

// API endpoints
const QString OneSevenLiveTwitchClient::TWITCH_HELIX_API_BASE = "https://api.twitch.tv/helix";
const QString OneSevenLiveTwitchClient::TWITCH_USERS_ENDPOINT = "/users";
const QString OneSevenLiveTwitchClient::TWITCH_CHANNELS_ENDPOINT = "/channels";

OneSevenLiveTwitchClient::OneSevenLiveTwitchClient(QObject* parent)
    : QObject(parent)
{
    obs_log(LOG_INFO, "TwitchClient initialized");
}

OneSevenLiveTwitchClient::~OneSevenLiveTwitchClient()
{
    obs_log(LOG_INFO, "TwitchClient destroyed");
}

void OneSevenLiveTwitchClient::setAuthData(const QString& accessToken, const QString& clientId)
{
    m_accessToken = accessToken;
    m_clientId = clientId;
    obs_log(LOG_INFO, "TwitchClient auth data set");
}

bool OneSevenLiveTwitchClient::hasValidAuth() const
{
    return !m_accessToken.isEmpty() && !m_clientId.isEmpty();
}

void OneSevenLiveTwitchClient::getCurrentUser()
{
    if (!hasValidAuth()) {
        emit errorOccurred("Missing authentication data");
        return;
    }
    
    obs_log(LOG_INFO, "Fetching current Twitch user info");
    makeApiRequest(TWITCH_USERS_ENDPOINT);
}

void OneSevenLiveTwitchClient::getUserById(const QString& userId)
{
    if (!hasValidAuth()) {
        emit errorOccurred("Missing authentication data");
        return;
    }
    
    if (userId.isEmpty()) {
        emit errorOccurred("User ID cannot be empty");
        return;
    }
    
    obs_log(LOG_INFO, "Fetching Twitch user info by ID: %s", userId.toUtf8().constData());
    makeApiRequest(TWITCH_USERS_ENDPOINT, QString("id=%1").arg(userId));
}

void OneSevenLiveTwitchClient::getUserByLogin(const QString& login)
{
    if (!hasValidAuth()) {
        emit errorOccurred("Missing authentication data");
        return;
    }
    
    if (login.isEmpty()) {
        emit errorOccurred("Login cannot be empty");
        return;
    }
    
    obs_log(LOG_INFO, "Fetching Twitch user info by login: %s", login.toUtf8().constData());
    makeApiRequest(TWITCH_USERS_ENDPOINT, QString("login=%1").arg(login));
}

void OneSevenLiveTwitchClient::getChannelInformation(const QString& broadcasterId)
{
    if (!hasValidAuth()) {
        emit errorOccurred("Missing authentication data");
        return;
    }
    
    if (broadcasterId.isEmpty()) {
        emit errorOccurred("Broadcaster ID cannot be empty");
        return;
    }
    
    obs_log(LOG_INFO, "Fetching Twitch channel info for broadcaster: %s", broadcasterId.toUtf8().constData());
    
    QString url = TWITCH_HELIX_API_BASE + TWITCH_CHANNELS_ENDPOINT;
    QString fullUrl = QString("%1?broadcaster_id=%2").arg(url, broadcasterId);
    
    RemoteTextThread* thread = new RemoteTextThread(
        fullUrl.toStdString(),
        "application/json",
        "", // No post data for GET request
        /*timeoutSec=*/15,
        /*isImageRequest=*/false);

    // Set required headers
    thread->setHeader("Authorization", QString("Bearer %1").arg(m_accessToken).toStdString());
    thread->setHeader("Client-Id", m_clientId.toStdString());

    connect(thread, &RemoteTextThread::Result, this, &OneSevenLiveTwitchClient::onChannelInfoResult);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveTwitchClient::makeApiRequest(const QString& endpoint, const QString& query)
{
    QString url = TWITCH_HELIX_API_BASE + endpoint;
    if (!query.isEmpty()) {
        url += "?" + query;
    }
    
    RemoteTextThread* thread = new RemoteTextThread(
        url.toStdString(),
        "application/json",
        "", // No post data for GET request
        /*timeoutSec=*/15,
        /*isImageRequest=*/false);

    // Set required headers
    thread->setHeader("Authorization", QString("Bearer %1").arg(m_accessToken).toStdString());
    thread->setHeader("Client-Id", m_clientId.toStdString());

    connect(thread, &RemoteTextThread::Result, this, &OneSevenLiveTwitchClient::onUserInfoResult);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveTwitchClient::onUserInfoResult(const QString& text, const QString& error)
{
    if (!error.isEmpty()) {
        QString errorMsg = QString("Twitch API request failed: %1").arg(error);
        obs_log(LOG_ERROR, "Twitch user info request error: %s", errorMsg.toUtf8().constData());
        emit errorOccurred(errorMsg);
        return;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(text.toStdString());
        
        if (json.contains("error")) {
            std::string err = json["error"].get<std::string>();
            std::string errorDescription = json.value("message", "");
            QString errorMsg = QString("Twitch API error: %1 - %2").arg(QString::fromStdString(err), QString::fromStdString(errorDescription));
            obs_log(LOG_ERROR, "Twitch API error: %s", errorMsg.toUtf8().constData());
            emit errorOccurred(errorMsg);
            return;
        }

        if (!json.contains("data") || !json["data"].is_array()) {
            QString errorMsg = "Invalid Twitch API response format";
            obs_log(LOG_ERROR, "Twitch API response missing data array");
            emit errorOccurred(errorMsg);
            return;
        }

        auto& dataArray = json["data"];
        if (dataArray.empty()) {
            QString errorMsg = "No user data found";
            obs_log(LOG_WARNING, "Twitch API returned empty user data");
            emit errorOccurred(errorMsg);
            return;
        }

        auto& userObj = dataArray[0];
        TwitchUserInfo userInfo = parseUserInfo(userObj);
        
        // Cache the user info
        m_cachedUserInfo = userInfo;
        
        obs_log(LOG_INFO, "Twitch user info retrieved successfully for: %s", userInfo.login.toUtf8().constData());
        emit userInfoReceived(userInfo);
    } catch (const nlohmann::json::exception& e) {
        QString errorMsg = QString("Failed to parse Twitch API response: %1").arg(QString::fromStdString(e.what()));
        obs_log(LOG_ERROR, "Twitch user info parse error: %s", errorMsg.toUtf8().constData());
        emit errorOccurred(errorMsg);
        return;
    }
}

void OneSevenLiveTwitchClient::onChannelInfoResult(const QString& text, const QString& error)
{
    if (!error.isEmpty()) {
        QString errorMsg = QString("Twitch channel info request failed: %1").arg(error);
        obs_log(LOG_ERROR, "Twitch channel info request error: %s", errorMsg.toUtf8().constData());
        emit errorOccurred(errorMsg);
        return;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(text.toStdString());
        
        if (json.contains("error")) {
            std::string err = json["error"].get<std::string>();
            std::string errorDescription = json.value("message", "");
            QString errorMsg = QString("Twitch API error: %1 - %2").arg(QString::fromStdString(err), QString::fromStdString(errorDescription));
            obs_log(LOG_ERROR, "Twitch API error: %s", errorMsg.toUtf8().constData());
            emit errorOccurred(errorMsg);
            return;
        }

        if (!json.contains("data") || !json["data"].is_array()) {
            QString errorMsg = "Invalid Twitch API response format";
            obs_log(LOG_ERROR, "Twitch API response missing data array");
            emit errorOccurred(errorMsg);
            return;
        }

        auto& dataArray = json["data"];
        if (dataArray.empty()) {
            QString errorMsg = "No channel data found";
            obs_log(LOG_WARNING, "Twitch API returned empty channel data");
            emit errorOccurred(errorMsg);
            return;
        }

        auto& channelObj = dataArray[0];
        TwitchChannelInfo channelInfo = parseChannelInfo(channelObj);
        
        // Cache the channel info
        m_cachedChannelInfo = channelInfo;
        
        obs_log(LOG_INFO, "Twitch channel info retrieved successfully for: %s", channelInfo.broadcasterName.toUtf8().constData());
        emit channelInfoReceived(channelInfo);
    } catch (const nlohmann::json::exception& e) {
        QString errorMsg = QString("Failed to parse Twitch API response: %1").arg(QString::fromStdString(e.what()));
        obs_log(LOG_ERROR, "Twitch channel info parse error: %s", errorMsg.toUtf8().constData());
        emit errorOccurred(errorMsg);
        return;
    }
}

TwitchUserInfo OneSevenLiveTwitchClient::parseUserInfo(const nlohmann::json& userObj)
{
    TwitchUserInfo userInfo;
    
    userInfo.id = QString::fromStdString(userObj.value("id", ""));
    userInfo.login = QString::fromStdString(userObj.value("login", ""));
    userInfo.displayName = QString::fromStdString(userObj.value("display_name", ""));
    userInfo.type = QString::fromStdString(userObj.value("type", ""));
    userInfo.broadcasterType = QString::fromStdString(userObj.value("broadcaster_type", ""));
    userInfo.description = QString::fromStdString(userObj.value("description", ""));
    userInfo.profileImageUrl = QString::fromStdString(userObj.value("profile_image_url", ""));
    userInfo.offlineImageUrl = QString::fromStdString(userObj.value("offline_image_url", ""));
    userInfo.viewCount = userObj.value("view_count", 0);
    userInfo.email = QString::fromStdString(userObj.value("email", ""));
    userInfo.createdAt = QString::fromStdString(userObj.value("created_at", ""));
    
    return userInfo;
}

TwitchChannelInfo OneSevenLiveTwitchClient::parseChannelInfo(const nlohmann::json& channelObj)
{
    TwitchChannelInfo channelInfo;
    
    channelInfo.broadcasterId = QString::fromStdString(channelObj.value("broadcaster_id", ""));
    channelInfo.broadcasterLogin = QString::fromStdString(channelObj.value("broadcaster_login", ""));
    channelInfo.broadcasterName = QString::fromStdString(channelObj.value("broadcaster_name", ""));
    channelInfo.gameName = QString::fromStdString(channelObj.value("game_name", ""));
    channelInfo.gameId = QString::fromStdString(channelObj.value("game_id", ""));
    channelInfo.broadcasterLanguage = QString::fromStdString(channelObj.value("broadcaster_language", ""));
    channelInfo.title = QString::fromStdString(channelObj.value("title", ""));
    channelInfo.delay = channelObj.value("delay", 0);
    
    return channelInfo;
}

void OneSevenLiveTwitchClient::clearCache()
{
    m_cachedUserInfo = TwitchUserInfo();
    m_cachedChannelInfo = TwitchChannelInfo();
}