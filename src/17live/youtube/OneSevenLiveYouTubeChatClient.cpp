#include "OneSevenLiveYouTubeChatClient.hpp"

#include <obs-module.h>

#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <nlohmann/json.hpp>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveYouTubeClient.hpp"
#include "api/OneSevenLiveModels.hpp"
#include "multi-rtmp/OneSevenLiveMultiRtmpManager.hpp"
#include "plugin-support.h"
#include "utility/RemoteTextThread.hpp"
#include "websocket/OneSevenLiveWebsocketServer.hpp"
#include "websocket/WebsocketUtils.hpp"
#include "websocket/WsMessage.hpp"
#include "youtube/OneSevenLiveYouTubeAuth.hpp"

const QString OneSevenLiveYouTubeChatClient::YOUTUBE_API_BASE_URL =
    "https://www.googleapis.com/youtube/v3";
const QString OneSevenLiveYouTubeChatClient::YOUTUBE_API_VERSION = "v3";
const int OneSevenLiveYouTubeChatClient::DEFAULT_POLLING_INTERVAL = 5000;  // 5 seconds
const int OneSevenLiveYouTubeChatClient::MIN_POLLING_INTERVAL = 5000;
const int OneSevenLiveYouTubeChatClient::MAX_EXPONENTIAL_BACKOFF_DELAY = 32000;  // 32 seconds max
const int OneSevenLiveYouTubeChatClient::STATUS_BROADCAST_INTERVAL = 10;
const int OneSevenLiveYouTubeChatClient::MAX_QUICK_RETRIES = 5;
const int OneSevenLiveYouTubeChatClient::LONG_RETRY_DELAY = 600;
const int OneSevenLiveYouTubeChatClient::MAX_NO_MESSAGE_QUICK_POLLS = 5;
const int OneSevenLiveYouTubeChatClient::EMPTY_CHAT_BACKOFF_BASE_MS = 10000;
const int OneSevenLiveYouTubeChatClient::EMPTY_CHAT_BACKOFF_INCREMENT_MS = 5000;
const int OneSevenLiveYouTubeChatClient::EMPTY_CHAT_BACKOFF_MAX_MS = 30000;

// Helper: convert YouTubeChatMessage to JSON for websocket payload
static nlohmann::json toJson(const YouTubeChatMessage& msg) {
    nlohmann::json j;
    j["kind"] = msg.kind.toStdString();
    j["etag"] = msg.etag.toStdString();
    j["id"] = msg.id.toStdString();

    nlohmann::json snippet;
    snippet["type"] = msg.snippet.type.toStdString();
    snippet["liveChatId"] = msg.snippet.liveChatId.toStdString();
    snippet["authorChannelId"] = msg.snippet.authorChannelId.toStdString();
    snippet["publishedAt"] = msg.snippet.publishedAt.toStdString();
    snippet["displayMessage"] = msg.snippet.displayMessage.toStdString();
    snippet["textMessageDetails"] = msg.snippet.textMessageDetails.toStdString();
    snippet["messageId"] = msg.snippet.messageId.toStdString();
    j["snippet"] = snippet;

    nlohmann::json author;
    author["channelId"] = msg.authorDetails.channelId.toStdString();
    author["displayName"] = msg.authorDetails.displayName.toStdString();
    author["profileImageUrl"] = msg.authorDetails.profileImageUrl.toStdString();
    author["isVerified"] = msg.authorDetails.isVerified;
    author["isChatOwner"] = msg.authorDetails.isChatOwner;
    author["isChatSponsor"] = msg.authorDetails.isChatSponsor;
    author["isChatModerator"] = msg.authorDetails.isChatModerator;
    j["authorDetails"] = author;

    return j;
}

OneSevenLiveYouTubeChatClient::OneSevenLiveYouTubeChatClient(QObject* parent)
    : QObject(parent),
      m_timeoutMs(30000)  // 30 seconds default timeout
      ,
      m_maxRetries(3),
      m_retryDelayMs(1000)  // 1 second base delay
      ,
      m_currentRetryCount(0),
      m_reconnectAttempts(0),
      m_noMessageStreak(0),
      m_hasValidAuth(false),
      m_isPolling(false),
      m_isRateLimited(false),
      m_currentPollingInterval(DEFAULT_POLLING_INTERVAL),
      m_exponentialBackoffDelay(m_retryDelayMs),
      m_pollingTimer(new QTimer(this)),
      m_statusTimer(new QTimer(this)),
      m_reconnectTimer(new QTimer(this)),
      m_emptyChatBackoffBaseMs(EMPTY_CHAT_BACKOFF_BASE_MS),
      m_emptyChatBackoffIncrementMs(EMPTY_CHAT_BACKOFF_INCREMENT_MS),
      m_emptyChatBackoffMaxMs(EMPTY_CHAT_BACKOFF_MAX_MS) {
    connect(m_pollingTimer, &QTimer::timeout, this,
            &OneSevenLiveYouTubeChatClient::onPollingTimeout);
    m_pollingTimer->setSingleShot(true);  // Single shot timer for controlled polling
    connect(m_statusTimer, &QTimer::timeout, this, &OneSevenLiveYouTubeChatClient::onStatusTimer);
    m_statusTimer->setInterval(STATUS_BROADCAST_INTERVAL * 1000);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &OneSevenLiveYouTubeChatClient::doReconnect);
}

OneSevenLiveYouTubeChatClient::~OneSevenLiveYouTubeChatClient() {
    stopChatPolling();
}

void OneSevenLiveYouTubeChatClient::setAccessToken(const QString& accessToken) {
    m_accessToken = accessToken;
    m_hasValidAuth = !accessToken.isEmpty();
    obs_log(LOG_INFO, "YouTube chat access token set, valid: %s",
            m_hasValidAuth ? "true" : "false");
}

bool OneSevenLiveYouTubeChatClient::hasValidAuth() const {
    return m_hasValidAuth;
}

void OneSevenLiveYouTubeChatClient::startChatPolling(const QString& liveChatId) {
    if (liveChatId.isEmpty()) {
        emit errorOccurred("Live Chat ID cannot be empty", "startChatPolling");
        return;
    }

    if (!m_hasValidAuth && m_apiKey.isEmpty()) {
        emit errorOccurred("No valid authentication (access token or API key)", "startChatPolling");
        return;
    }

    if (m_isPolling && liveChatId == m_liveChatId) {
        obs_log(LOG_INFO, "Chat polling already running for same chatId; ignoring restart");
        return;
    }
    if (m_isPolling) {
        obs_log(LOG_INFO, "Chat polling already running, stopping first");
        stopChatPolling();
    }

    m_liveChatId = liveChatId;
    m_nextPageToken.clear();
    m_currentRetryCount = 0;
    m_exponentialBackoffDelay = m_retryDelayMs;
    m_isRateLimited = false;

    obs_log(LOG_INFO, "Starting YouTube chat polling for liveChatId: %s",
            liveChatId.toUtf8().constData());

    m_isPolling = true;
    m_reconnectAttempts = 0;
    if (m_reconnectTimer->isActive())
        m_reconnectTimer->stop();
    emit pollingStarted(liveChatId);

    if (!m_statusTimer->isActive())
        m_statusTimer->start();
    obs_log(LOG_INFO, "YouTube chat connected");
    {
        auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager();
        if (cm) {
            std::string v;
            if (cm->getConfigValue("YouTubeEmptyChatBackoffBaseSec", v) && !v.empty()) {
                try {
                    int sec = std::stoi(v);
                    int ms = sec * 1000;
                    m_emptyChatBackoffBaseMs = qMax(10000, qMin(ms, 60000));
                } catch (...) {
                }
            }
            if (cm->getConfigValue("YouTubeEmptyChatBackoffIncrementSec", v) && !v.empty()) {
                try {
                    int sec = std::stoi(v);
                    int ms = sec * 1000;
                    m_emptyChatBackoffIncrementMs = qMax(0, qMin(ms, 60000));
                } catch (...) {
                }
            }
            if (cm->getConfigValue("YouTubeEmptyChatBackoffMaxSec", v) && !v.empty()) {
                try {
                    int sec = std::stoi(v);
                    int ms = sec * 1000;
                    m_emptyChatBackoffMaxMs = qMax(MIN_POLLING_INTERVAL, qMin(ms, 60000));
                } catch (...) {
                }
            }
            if (m_emptyChatBackoffBaseMs > m_emptyChatBackoffMaxMs) {
                m_emptyChatBackoffBaseMs = m_emptyChatBackoffMaxMs;
            }
        }
    }

    // Start first request immediately
    fetchChatMessages();
}

void OneSevenLiveYouTubeChatClient::stopChatPolling() {
    if (!m_isPolling) {
        return;
    }

    obs_log(LOG_INFO, "Stopping YouTube chat polling");

    m_isPolling = false;
    m_pollingTimer->stop();

    m_liveChatId.clear();
    m_nextPageToken.clear();
    m_currentRetryCount = 0;
    m_exponentialBackoffDelay = m_retryDelayMs;
    m_isRateLimited = false;
    m_noMessageStreak = 0;

    emit pollingStopped();
    if (m_statusTimer->isActive())
        m_statusTimer->stop();
    if (m_reconnectTimer->isActive())
        m_reconnectTimer->stop();
}

bool OneSevenLiveYouTubeChatClient::isPolling() const {
    return m_isPolling;
}

void OneSevenLiveYouTubeChatClient::setApiKey(const QString& apiKey) {
    m_apiKey = apiKey;
    obs_log(LOG_INFO, "YouTube chat API key set");
}

void OneSevenLiveYouTubeChatClient::setTimeout(int timeoutMs) {
    m_timeoutMs = timeoutMs;
    // obs_log(LOG_INFO, "API timeout set to %d ms", timeoutMs);
}

void OneSevenLiveYouTubeChatClient::setMaxRetries(int maxRetries) {
    m_maxRetries = maxRetries;
    obs_log(LOG_INFO, "Max retries set to %d", maxRetries);
}

void OneSevenLiveYouTubeChatClient::setRetryDelay(int baseDelayMs) {
    m_retryDelayMs = baseDelayMs;
    m_exponentialBackoffDelay = baseDelayMs;
    obs_log(LOG_INFO, "Retry delay set to %d ms", baseDelayMs);
}

void OneSevenLiveYouTubeChatClient::setApiClient(OneSevenLiveYouTubeClient* apiClient) {
    m_apiClient = apiClient;
    if (!m_apiClient)
        return;
    connect(m_apiClient, &OneSevenLiveYouTubeClient::myLiveBroadcastsReceived, this,
            &OneSevenLiveYouTubeChatClient::onBroadcastsReceived);
}

void OneSevenLiveYouTubeChatClient::startDiscovery() {
    if (!m_apiClient)
        return;
    if (!m_discoverTimer) {
        m_discoverTimer = new QTimer(this);
        m_discoverTimer->setInterval(60000);
        connect(m_discoverTimer, &QTimer::timeout, this, [this]() {
            if (m_apiClient && m_apiClient->hasValidAuth()) {
                auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager();
                if (cm) {
                    QString bid;
                    QString chat;
                    if (cm->getYouTubeBroadcastInfo(bid, chat) && !bid.isEmpty()) {
                        m_apiClient->getLiveBroadcastById(bid);
                        return;
                    }
                }
                m_apiClient->getMyLiveBroadcasts();
            }
        });
    }
    if (m_apiClient->hasValidAuth()) {
        if (!m_discoverTimer->isActive())
            m_discoverTimer->start();
        {
            auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager();
            if (cm) {
                QString bid;
                QString chat;
                if (cm->getYouTubeBroadcastInfo(bid, chat) && !bid.isEmpty()) {
                    m_apiClient->getLiveBroadcastById(bid);
                    return;
                }
            }
        }
        m_apiClient->getMyLiveBroadcasts();
    } else {
        if (m_discoverTimer->isActive())
            m_discoverTimer->stop();
    }
}

void OneSevenLiveYouTubeChatClient::stopDiscovery() {
    if (m_discoverTimer && m_discoverTimer->isActive())
        m_discoverTimer->stop();
}

void OneSevenLiveYouTubeChatClient::fetchChatMessages() {
    if (!m_isPolling || m_liveChatId.isEmpty()) {
        return;
    }

    if (!m_hasValidAuth && m_apiKey.isEmpty()) {
        return;
    }

    QString endpoint = buildChatMessagesUrl(m_liveChatId, m_nextPageToken);
    // obs_log(LOG_INFO, "YouTube chat fetch: liveChatId=%s pageToken=%s",
    // m_liveChatId.toUtf8().constData(), m_nextPageToken.toUtf8().constData());
    makeChatRequest(endpoint);
}

void OneSevenLiveYouTubeChatClient::scheduleNextPoll(int intervalMs) {
    if (!m_isPolling) {
        return;
    }

    intervalMs = qMax(intervalMs, MIN_POLLING_INTERVAL);
    m_currentPollingInterval = intervalMs;

    obs_log(LOG_DEBUG, "Scheduling next poll in %d ms", intervalMs);
    m_pollingTimer->start(intervalMs);
}

void OneSevenLiveYouTubeChatClient::handleRateLimit(int retryAfterMs) {
    m_isRateLimited = true;
    int actualDelay = qMax(retryAfterMs, m_exponentialBackoffDelay);

    obs_log(LOG_WARNING, "Rate limit hit, scheduling retry in %d ms", actualDelay);
    emit rateLimitHit(actualDelay);

    // Exponential backoff for next time
    m_exponentialBackoffDelay = qMin(m_exponentialBackoffDelay * 2, MAX_EXPONENTIAL_BACKOFF_DELAY);

    scheduleNextPoll(actualDelay);
}

void OneSevenLiveYouTubeChatClient::handleApiError(const QString& error, const QString& operation,
                                                   int httpStatus) {
    QString detailedError;

    switch (httpStatus) {
    case 401:
        detailedError = "Authentication failed - invalid or expired token";
        m_hasValidAuth = false;
        if (auto* auth = OneSevenLiveCoreManager::getInstance().getYouTubeAuth()) {
            QTimer::singleShot(0, auth, &OneSevenLiveYouTubeAuth::refreshAccessTokenAsync);
        }
        scheduleReconnect();
        return;
        break;
    case 403:
        detailedError = "Access forbidden - insufficient permissions or quota exceeded";
        if (error.contains("quotaExceeded") || error.contains("rateLimitExceeded")) {
            handleRateLimit(0);
            return;
        }
        break;
    case 404:
        detailedError = "Live chat not found";
        break;
    case 429:
        detailedError = "Rate limit exceeded";
        handleRateLimit(30000);  // 30 seconds for rate limits
        return;
    default:
        detailedError = error;
        break;
    }

    emit errorOccurred(detailedError, operation);

    // For non-rate-limit errors, continue with normal polling interval
    if (m_isPolling && httpStatus != 429 && httpStatus != 403 && httpStatus != 401 &&
        !error.contains("quotaExceeded")) {
        scheduleNextPoll(m_currentPollingInterval);
    }
}

QString OneSevenLiveYouTubeChatClient::buildChatMessagesUrl(const QString& liveChatId,
                                                            const QString& pageToken) const {
    QString url = YOUTUBE_API_BASE_URL + "/liveChat/messages";

    QUrlQuery query;
    query.addQueryItem("liveChatId", liveChatId);
    query.addQueryItem("part", "snippet,authorDetails");

    if (!pageToken.isEmpty()) {
        query.addQueryItem("pageToken", pageToken);
    }

    if (!m_apiKey.isEmpty()) {
        query.addQueryItem("key", m_apiKey);
    }

    {
        auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager();
        if (cm) {
            std::string openId;
            if (cm->getConfigValue("UserID", openId) && !openId.empty()) {
                query.addQueryItem("quotaUser", QString::fromStdString(openId));
            }
        }
    }

    return url + "?" + query.toString();
}

void OneSevenLiveYouTubeChatClient::makeChatRequest(const QString& endpoint) {
    obs_log(LOG_DEBUG, "YouTube Chat API Request: %s", endpoint.toUtf8().constData());
    m_lastEndpoint = endpoint;
    if (m_hasValidAuth) {
        const QString tok = m_accessToken;
        const QString masked = tok.length() >= 12 ? tok.left(6) + "..." + tok.right(6) : tok;
        obs_log(LOG_DEBUG, "YouTube Chat token(masked)=%s auth_mode=Bearer",
                masked.toUtf8().constData());
    }

    // Build headers
    std::vector<std::string> headers;
    headers.push_back(std::string("Accept: application/json"));
    if (m_hasValidAuth && !m_accessToken.isEmpty()) {
        std::string bearer = std::string("Authorization: Bearer ") + m_accessToken.toStdString();
        headers.push_back(bearer);
    }

    std::atomic<bool>* cancelFlag = OneSevenLiveCoreManager::getInstance().getCancelFlag();
    RemoteTextThread* thread =
        new RemoteTextThread(endpoint.toStdString(), std::move(headers), "application/json",
                             std::string(), m_timeoutMs / 1000, false, cancelFlag);

    m_currentOperation = "getChatMessages";

    connect(thread, &RemoteTextThread::Result, this,
            &OneSevenLiveYouTubeChatClient::onChatRequestFinished, Qt::QueuedConnection);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveYouTubeChatClient::onBroadcastsReceived(
    const YouTubeLiveBroadcastListResponse& resp) {
    QString discovered;
    QString broadcastId;
    {
        auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager();
        QString savedBid;
        QString savedChat;
        if (cm && cm->getYouTubeBroadcastInfo(savedBid, savedChat) && !savedBid.isEmpty()) {
            for (const auto& b : resp.items) {
                if (b.id == savedBid && !b.snippet.liveChatId.isEmpty()) {
                    discovered = b.snippet.liveChatId;
                    broadcastId = b.id;
                    break;
                }
            }
        }
    }
    if (discovered.isEmpty()) {
        for (const auto& b : resp.items) {
            if (!b.snippet.actualEndTime.isEmpty())
                continue;
            if (!b.snippet.liveChatId.isEmpty()) {
                discovered = b.snippet.liveChatId;
                broadcastId = b.id;
                break;
            }
        }
    }
    if (discovered.isEmpty()) {
        if (!isPolling()) {
            OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
                QString::fromUtf8(ws::EventYouTubeChatConnected),
                nlohmann::json{{"status", "break"}});
        }
        return;
    }
    if (!broadcastId.isEmpty()) {
        auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager();
        if (cm) {
            cm->setYouTubeBroadcastInfo(broadcastId, discovered);
        }
    }
    if (!m_liveChatId.isEmpty() && discovered == m_liveChatId) {
        if (!isPolling()) {
            startChatPolling(discovered);
        }
        return;
    }
    startChatPolling(discovered);
}

void OneSevenLiveYouTubeChatClient::onChatRequestFinished(const QString& response,
                                                          const QString& error) {
    if (!m_isPolling) {
        return;  // Ignore responses if polling was stopped
    }

    if (!error.isEmpty()) {
        obs_log(LOG_WARNING, "YouTube Chat API Error: %s", error.toUtf8().constData());

        // Extract HTTP status code from error if possible
        int httpStatus = -1;
        QRegularExpression statusRegex(R"(HTTP (\d{3}))");
        QRegularExpressionMatch match = statusRegex.match(error);
        if (match.hasMatch()) {
            httpStatus = match.captured(1).toInt();
        }

        obs_log(LOG_WARNING, "YouTube Chat API Error context: op=%s endpoint=%s",
                m_currentOperation.toUtf8().constData(), m_lastEndpoint.toUtf8().constData());
        obs_log(LOG_WARNING, "YouTube Chat API Error response: %s",
                response.isEmpty() ? "<empty>" : response.toUtf8().constData());
        try {
            auto j = nlohmann::json::parse(response.toStdString());
            auto ej = j.contains("error") ? j["error"] : nlohmann::json{};
            std::string emsg = ej.value("message", std::string());
            std::string estatus = ej.value("status", std::string());
            int ecode = ej.value("code", 0);
            std::string ereason;
            if (ej.contains("errors") && ej["errors"].is_array() && !ej["errors"].empty()) {
                auto e0 = ej["errors"][0];
                ereason = e0.value("reason", std::string());
            }
            if (ecode || !emsg.empty() || !estatus.empty() || !ereason.empty()) {
                obs_log(LOG_WARNING,
                        "YouTube Chat API Error details: code=%d message=%s status=%s reason=%s",
                        ecode, emsg.c_str(), estatus.c_str(), ereason.c_str());
            }
        } catch (...) {
        }

        bool chatEnded = false;
        try {
            auto j = nlohmann::json::parse(response.toStdString());
            if (j.contains("error") && j["error"].is_object()) {
                auto ej = j["error"];
                if (ej.contains("errors") && ej["errors"].is_array() && !ej["errors"].empty()) {
                    auto e0 = ej["errors"][0];
                    std::string reason = e0.value("reason", std::string());
                    if (reason == "liveChatEnded")
                        chatEnded = true;
                }
                std::string msg = ej.value("message", std::string());
                if (!chatEnded && msg.find("live chat is no longer live") != std::string::npos)
                    chatEnded = true;
            }
        } catch (...) {
        }

        if (httpStatus == 403 && chatEnded) {
            obs_log(LOG_INFO,
                    "YouTube liveChatId is no longer live; stopping polling and clearing chatId");
            m_isPolling = false;
            m_pollingTimer->stop();
            m_statusTimer->stop();
            m_reconnectTimer->stop();
            m_nextPageToken.clear();
            m_liveChatId.clear();
            emit pollingStopped();
            wsBroadcast(QString::fromUtf8(ws::EventYouTubeChatConnected),
                        nlohmann::json{{"status", "break"}});
            emit errorOccurred("liveChatEnded", m_currentOperation);
            return;
        }
        if (httpStatus == 403 || httpStatus == 404) {
            if (m_apiClient && m_apiClient->hasValidAuth()) {
                obs_log(LOG_INFO, "YouTube chat 403/404, triggering liveChatId discovery");
                startDiscovery();
            }
        }
        handleApiError(error, m_currentOperation, httpStatus);
        if (!m_isRateLimited) {
            scheduleReconnect();
        }
        return;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(response.toStdString());

        YouTubeChatMessageListResponse chatResponse = parseChatMessageListResponse(json);
        // obs_log(LOG_INFO, "YouTube chat API result: liveChatId=%s items=%d nextPageToken=%s
        // pollIntervalMs=%d totalResults=%d", m_liveChatId.toUtf8().constData(),
        // chatResponse.items.size(), chatResponse.nextPageToken.toUtf8().constData(),
        // chatResponse.pollingIntervalMillis, chatResponse.totalResults);

        // Update next page token for pagination
        m_nextPageToken = chatResponse.nextPageToken;

        // Emit the complete response
        emit chatMessagesReceived(chatResponse);

        // Emit individual messages
        for (const auto& message : chatResponse.items) {
            emit newChatMessage(message);

            obs_log(LOG_DEBUG, "YouTube chat received: [%s] %s",
                    message.authorDetails.displayName.toUtf8().constData(),
                    message.snippet.displayMessage.toUtf8().constData());

            try {
                OneSevenLiveCoreManager::getInstance().enqueueOrBroadcastChatEvent(
                    QString::fromUtf8(ws::EventYouTubeChatMessage), toJson(message));
            } catch (const std::exception& e) {
                obs_log(LOG_WARNING, "Failed to serialize/broadcast YouTube chat message: %s",
                        e.what());
            }
        }

        // Reset retry count on successful request
        m_currentRetryCount = 0;
        m_exponentialBackoffDelay = m_retryDelayMs;
        m_isRateLimited = false;

        if (chatResponse.items.isEmpty()) {
            m_noMessageStreak++;
            int streak = m_noMessageStreak;
            int baseMs = m_emptyChatBackoffBaseMs;
            int incMs = m_emptyChatBackoffIncrementMs;
            int maxMs = m_emptyChatBackoffMaxMs;
            int candidate = baseMs + incMs * qMax(0, streak - 1);
            int intervalMs = qMin(candidate, maxMs);
            scheduleNextPoll(intervalMs);
        } else {
            m_noMessageStreak = 0;
            scheduleNextPoll(chatResponse.pollingIntervalMillis);
        }

    } catch (const std::exception& e) {
        emit errorOccurred(QString("Failed to parse chat JSON response: ") + e.what(),
                           "parseChatResponse");
        scheduleReconnect();
    }
}

void OneSevenLiveYouTubeChatClient::onPollingTimeout() {
    if (!m_isPolling) {
        return;
    }

    fetchChatMessages();
}

void OneSevenLiveYouTubeChatClient::onStatusTimer() {
    auto& core = OneSevenLiveCoreManager::getInstance();
    const char* status = m_isPolling ? "connected" : "break";
    core.enqueueOrBroadcastChatEvent(QString::fromUtf8(ws::EventYouTubeChatConnected),
                                     nlohmann::json{{"status", status}});

    if (!m_isPolling && m_statusTimer->isActive())
        m_statusTimer->stop();
}

void OneSevenLiveYouTubeChatClient::scheduleReconnect() {
    if (!m_liveChatId.isEmpty()) {
        if (m_reconnectAttempts < MAX_QUICK_RETRIES) {
            int delayMs = qMin(m_exponentialBackoffDelay, 5000);
            m_reconnectAttempts++;
            if (m_isPolling) {
                m_pollingTimer->start(delayMs);
            } else {
                m_reconnectTimer->start(delayMs);
            }
        } else {
            m_reconnectAttempts = 0;
            m_isPolling = false;
            wsBroadcast(QString::fromUtf8(ws::EventYouTubeChatConnected),
                        nlohmann::json{{"status", "break"}});
            m_reconnectTimer->start(LONG_RETRY_DELAY * 1000);
        }
    }
}

void OneSevenLiveYouTubeChatClient::doReconnect() {
    if (m_liveChatId.isEmpty())
        return;
    startChatPolling(m_liveChatId);
}

YouTubeChatMessage OneSevenLiveYouTubeChatClient::parseChatMessage(
    const nlohmann::json& json) const {
    YouTubeChatMessage message;

    message.kind = QString::fromStdString(json.value("kind", ""));
    message.etag = QString::fromStdString(json.value("etag", ""));
    message.id = QString::fromStdString(json.value("id", ""));

    if (json.contains("snippet") && json["snippet"].is_object()) {
        message.snippet = parseMessageSnippet(json["snippet"]);
    }

    if (json.contains("authorDetails") && json["authorDetails"].is_object()) {
        message.authorDetails = parseAuthorDetails(json["authorDetails"]);
    }

    return message;
}

YouTubeChatMessageSnippet OneSevenLiveYouTubeChatClient::parseMessageSnippet(
    const nlohmann::json& json) const {
    YouTubeChatMessageSnippet snippet;

    snippet.type = QString::fromStdString(json.value("type", ""));
    snippet.liveChatId = QString::fromStdString(json.value("liveChatId", ""));
    snippet.authorChannelId = QString::fromStdString(json.value("authorChannelId", ""));
    snippet.publishedAt = QString::fromStdString(json.value("publishedAt", ""));
    snippet.displayMessage = QString::fromStdString(json.value("displayMessage", ""));
    snippet.messageId = QString::fromStdString(json.value("messageId", ""));

    // Handle textMessageDetails if present
    if (json.contains("textMessageDetails") && json["textMessageDetails"].is_object()) {
        auto textDetails = json["textMessageDetails"];
        if (textDetails.contains("messageText")) {
            snippet.textMessageDetails =
                QString::fromStdString(textDetails.value("messageText", ""));
        }
    }

    return snippet;
}

YouTubeChatAuthorDetails OneSevenLiveYouTubeChatClient::parseAuthorDetails(
    const nlohmann::json& json) const {
    YouTubeChatAuthorDetails author;

    author.channelId = QString::fromStdString(json.value("channelId", ""));
    author.displayName = QString::fromStdString(json.value("displayName", ""));
    author.profileImageUrl = QString::fromStdString(json.value("profileImageUrl", ""));
    author.isVerified = json.value("isVerified", false);
    author.isChatOwner = json.value("isChatOwner", false);
    author.isChatSponsor = json.value("isChatSponsor", false);
    author.isChatModerator = json.value("isChatModerator", false);

    return author;
}

YouTubeChatMessageListResponse OneSevenLiveYouTubeChatClient::parseChatMessageListResponse(
    const nlohmann::json& json) const {
    YouTubeChatMessageListResponse response;

    response.kind = QString::fromStdString(json.value("kind", ""));
    response.etag = QString::fromStdString(json.value("etag", ""));
    response.nextPageToken = QString::fromStdString(json.value("nextPageToken", ""));
    response.pollingIntervalMillis = json.value("pollingIntervalMillis", DEFAULT_POLLING_INTERVAL);
    response.totalResults = json.value("totalResults", 0);

    if (json.contains("items") && json["items"].is_array()) {
        for (const auto& item : json["items"]) {
            if (item.is_object()) {
                YouTubeChatMessage message = parseChatMessage(item);
                response.items.append(message);
            }
        }
    }

    return response;
}
