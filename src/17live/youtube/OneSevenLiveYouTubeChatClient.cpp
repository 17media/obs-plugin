#include "OneSevenLiveYouTubeChatClient.hpp"
#include "utility/RemoteTextThread.hpp"
#include <nlohmann/json.hpp>
#include <QDebug>
#include "plugin-support.h"
#include <obs-module.h>
#include <QUrlQuery>
#include <QTimer>
#include <QRegularExpression>
#include "../OneSevenLiveCoreManager.hpp"
#include "../OneSevenLiveWebsocketServer.hpp"

const QString OneSevenLiveYouTubeChatClient::YOUTUBE_API_BASE_URL = "https://www.googleapis.com/youtube/v3";
const QString OneSevenLiveYouTubeChatClient::YOUTUBE_API_VERSION = "v3";
const int OneSevenLiveYouTubeChatClient::DEFAULT_POLLING_INTERVAL = 5000; // 5 seconds
const int OneSevenLiveYouTubeChatClient::MAX_EXPONENTIAL_BACKOFF_DELAY = 32000; // 32 seconds max

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
    : QObject(parent)
    , m_timeoutMs(30000) // 30 seconds default timeout
    , m_maxRetries(3)
    , m_retryDelayMs(1000) // 1 second base delay
    , m_currentRetryCount(0)
    , m_hasValidAuth(false)
    , m_isPolling(false)
    , m_isRateLimited(false)
    , m_currentPollingInterval(DEFAULT_POLLING_INTERVAL)
    , m_exponentialBackoffDelay(m_retryDelayMs)
    , m_pollingTimer(new QTimer(this))
{
    connect(m_pollingTimer, &QTimer::timeout, this, &OneSevenLiveYouTubeChatClient::onPollingTimeout);
    m_pollingTimer->setSingleShot(true); // Single shot timer for controlled polling
}

OneSevenLiveYouTubeChatClient::~OneSevenLiveYouTubeChatClient()
{
    stopChatPolling();
}

void OneSevenLiveYouTubeChatClient::setAccessToken(const QString& accessToken)
{
    m_accessToken = accessToken;
    m_hasValidAuth = !accessToken.isEmpty();
    obs_log(LOG_INFO, "YouTube chat access token set, valid: %s", m_hasValidAuth ? "true" : "false");
}

bool OneSevenLiveYouTubeChatClient::hasValidAuth() const
{
    return m_hasValidAuth;
}

void OneSevenLiveYouTubeChatClient::startChatPolling(const QString& liveChatId)
{
    if (liveChatId.isEmpty()) {
        emit errorOccurred("Live Chat ID cannot be empty", "startChatPolling");
        return;
    }
    
    if (!m_hasValidAuth && m_apiKey.isEmpty()) {
        emit errorOccurred("No valid authentication (access token or API key)", "startChatPolling");
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
    
    obs_log(LOG_INFO, "Starting YouTube chat polling for liveChatId: %s", liveChatId.toUtf8().constData());
    
    m_isPolling = true;
    emit pollingStarted(liveChatId);
    
    // Start first request immediately
    fetchChatMessages();
}

void OneSevenLiveYouTubeChatClient::stopChatPolling()
{
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
    
    emit pollingStopped();
}

bool OneSevenLiveYouTubeChatClient::isPolling() const
{
    return m_isPolling;
}

void OneSevenLiveYouTubeChatClient::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey;
    obs_log(LOG_INFO, "YouTube chat API key set");
}

void OneSevenLiveYouTubeChatClient::setTimeout(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
    obs_log(LOG_INFO, "API timeout set to %d ms", timeoutMs);
}

void OneSevenLiveYouTubeChatClient::setMaxRetries(int maxRetries)
{
    m_maxRetries = maxRetries;
    obs_log(LOG_INFO, "Max retries set to %d", maxRetries);
}

void OneSevenLiveYouTubeChatClient::setRetryDelay(int baseDelayMs)
{
    m_retryDelayMs = baseDelayMs;
    m_exponentialBackoffDelay = baseDelayMs;
    obs_log(LOG_INFO, "Retry delay set to %d ms", baseDelayMs);
}

void OneSevenLiveYouTubeChatClient::fetchChatMessages()
{
    if (!m_isPolling || m_liveChatId.isEmpty()) {
        return;
    }
    
    QString endpoint = buildChatMessagesUrl(m_liveChatId, m_nextPageToken);
    makeChatRequest(endpoint);
}

void OneSevenLiveYouTubeChatClient::scheduleNextPoll(int intervalMs)
{
    if (!m_isPolling) {
        return;
    }
    
    // Store the polling interval for potential rate limit handling
    m_currentPollingInterval = intervalMs;
    
    obs_log(LOG_INFO, "Scheduling next poll in %d ms", intervalMs);
    m_pollingTimer->start(intervalMs);
}

void OneSevenLiveYouTubeChatClient::handleRateLimit(int retryAfterMs)
{
    m_isRateLimited = true;
    int actualDelay = qMax(retryAfterMs, m_exponentialBackoffDelay);
    
    obs_log(LOG_WARNING, "Rate limit hit, scheduling retry in %d ms", actualDelay);
    emit rateLimitHit(actualDelay);
    
    // Exponential backoff for next time
    m_exponentialBackoffDelay = qMin(m_exponentialBackoffDelay * 2, MAX_EXPONENTIAL_BACKOFF_DELAY);
    
    scheduleNextPoll(actualDelay);
}

void OneSevenLiveYouTubeChatClient::handleApiError(const QString& error, const QString& operation, int httpStatus)
{
    QString detailedError;
    
    switch (httpStatus) {
        case 401:
            detailedError = "Authentication failed - invalid or expired token";
            m_hasValidAuth = false;
            break;
        case 403:
            detailedError = "Access forbidden - insufficient permissions or quota exceeded";
            if (error.contains("quotaExceeded") || error.contains("rateLimitExceeded")) {
                handleRateLimit(60000); // 1 minute for quota issues
                return;
            }
            break;
        case 404:
            detailedError = "Live chat not found";
            break;
        case 429:
            detailedError = "Rate limit exceeded";
            handleRateLimit(30000); // 30 seconds for rate limits
            return;
        default:
            detailedError = error;
            break;
    }
    
    emit errorOccurred(detailedError, operation);
    
    // For non-rate-limit errors, continue with normal polling interval
    if (m_isPolling && httpStatus != 429 && !error.contains("quotaExceeded")) {
        scheduleNextPoll(m_currentPollingInterval);
    }
}

QString OneSevenLiveYouTubeChatClient::buildChatMessagesUrl(const QString& liveChatId, const QString& pageToken) const
{
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
    
    return url + "?" + query.toString();
}

void OneSevenLiveYouTubeChatClient::makeChatRequest(const QString& endpoint)
{
    obs_log(LOG_INFO, "YouTube Chat API Request: %s", endpoint.toUtf8().constData());
    
    // Build headers
    std::vector<std::string> headers;
    if (m_hasValidAuth) {
        headers.push_back(std::string("Authorization: ") + QString("Bearer %1").arg(m_accessToken).toStdString());
    }
    headers.push_back(std::string("Accept: application/json"));

    RemoteTextThread* thread = new RemoteTextThread(
        endpoint.toStdString(),
        std::move(headers),
        "application/json",
        std::string(), // No body for GET request
        /*timeoutSec=*/m_timeoutMs / 1000,
        /*isImageRequest=*/false);

    m_currentOperation = "getChatMessages";

    connect(thread, &RemoteTextThread::Result, this, &OneSevenLiveYouTubeChatClient::onChatRequestFinished);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OneSevenLiveYouTubeChatClient::onChatRequestFinished(const QString& response, const QString& error)
{
    if (!m_isPolling) {
        return; // Ignore responses if polling was stopped
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
        
        handleApiError(error, m_currentOperation, httpStatus);
        return;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(response.toStdString());
        
        YouTubeChatMessageListResponse chatResponse = parseChatMessageListResponse(json);
        
        // Update next page token for pagination
        m_nextPageToken = chatResponse.nextPageToken;
        
        // Emit the complete response
        emit chatMessagesReceived(chatResponse);
        
        // Emit individual messages
        for (const auto& message : chatResponse.items) {
            emit newChatMessage(message);

            // Broadcast to websocket clients: { type: "youtube-chat", payload: message }
            OneSevenLiveCoreManager* core = qobject_cast<OneSevenLiveCoreManager*>(parent());
            if (core) {
                OneSevenLiveWebsocketServer* ws = core->getWebsocketServer();
                if (ws && ws->is_running()) {
                    try {
                        nlohmann::json payload = {
                            {"type", "youtube-chat"},
                            {"payload", toJson(message)}
                        };
                        ws->broadcastMessage(payload.dump());
                    } catch (const std::exception& e) {
                        obs_log(LOG_WARNING, "Failed to serialize/broadcast YouTube chat message: %s", e.what());
                    }
                }
            }
        }
        
        // Reset retry count on successful request
        m_currentRetryCount = 0;
        m_exponentialBackoffDelay = m_retryDelayMs;
        m_isRateLimited = false;
        
        // Schedule next poll based on server's recommended interval
        scheduleNextPoll(chatResponse.pollingIntervalMillis);
        
    } catch (const std::exception& e) {
        emit errorOccurred(QString("Failed to parse chat JSON response: ") + e.what(), "parseChatResponse");
        
        // Continue polling with default interval on parse error
        if (m_isPolling) {
            scheduleNextPoll(m_currentPollingInterval);
        }
    }
}

void OneSevenLiveYouTubeChatClient::onPollingTimeout()
{
    if (!m_isPolling) {
        return;
    }
    
    fetchChatMessages();
}

YouTubeChatMessage OneSevenLiveYouTubeChatClient::parseChatMessage(const nlohmann::json& json) const
{
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

YouTubeChatMessageSnippet OneSevenLiveYouTubeChatClient::parseMessageSnippet(const nlohmann::json& json) const
{
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
            snippet.textMessageDetails = QString::fromStdString(textDetails.value("messageText", ""));
        }
    }
    
    return snippet;
}

YouTubeChatAuthorDetails OneSevenLiveYouTubeChatClient::parseAuthorDetails(const nlohmann::json& json) const
{
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

YouTubeChatMessageListResponse OneSevenLiveYouTubeChatClient::parseChatMessageListResponse(const nlohmann::json& json) const
{
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
