#include "OneSevenLiveYouTubeClient.hpp"

#include <obs-module.h>

#include <QRegularExpression>
#include <QUrlQuery>
#include <nlohmann/json.hpp>

#include "plugin-support.h"
#include "utility/RemoteTextThread.hpp"

const QString OneSevenLiveYouTubeClient::YOUTUBE_API_BASE_URL =
    "https://www.googleapis.com/youtube/v3";
const QString OneSevenLiveYouTubeClient::YOUTUBE_API_VERSION = "v3";

OneSevenLiveYouTubeClient::OneSevenLiveYouTubeClient(QObject* parent)
    : QObject(parent),
      m_timeoutMs(30000)  // 30 seconds default timeout
      ,
      m_hasValidAuth(false) {}

OneSevenLiveYouTubeClient::~OneSevenLiveYouTubeClient() = default;

void OneSevenLiveYouTubeClient::setAccessToken(const QString& accessToken) {
    m_accessToken = accessToken;
    m_hasValidAuth = !accessToken.isEmpty();
    obs_log(LOG_INFO, "YouTube access token set, valid: %s", m_hasValidAuth ? "true" : "false");
}

bool OneSevenLiveYouTubeClient::hasValidAuth() const {
    return m_hasValidAuth;
}

void OneSevenLiveYouTubeClient::getMyLiveStreams() {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "getMyLiveStreams");
        return;
    }

    QMap<QString, QString> params;
    params["mine"] = "true";
    params["part"] = "snippet,cdn,status,contentDetails";

    QString endpoint = buildApiUrl("liveStreams", params);
    makeApiRequest(endpoint);
}

void OneSevenLiveYouTubeClient::getLiveStreamById(const QString& streamId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "getLiveStreamById");
        return;
    }

    if (streamId.isEmpty()) {
        emit errorOccurred("Stream ID cannot be empty", "getLiveStreamById");
        return;
    }

    QMap<QString, QString> params;
    params["id"] = streamId;
    params["part"] = "snippet,cdn,status,contentDetails";

    QString endpoint = buildApiUrl("liveStreams", params);
    makeApiRequest(endpoint);
}

void OneSevenLiveYouTubeClient::createLiveStream(const QString& title, const QString& description) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "createLiveStream");
        return;
    }

    if (title.isEmpty()) {
        emit errorOccurred("Title cannot be empty", "createLiveStream");
        return;
    }

    nlohmann::json requestBody;
    nlohmann::json snippet;
    snippet["title"] = title.toStdString();
    if (!description.isEmpty()) {
        snippet["description"] = description.toStdString();
    }
    requestBody["snippet"] = snippet;

    QString body = QString::fromStdString(requestBody.dump());

    QMap<QString, QString> params;
    params["part"] = "snippet,cdn,status";

    QString endpoint = buildApiUrl("liveStreams", params);
    makeApiRequest(endpoint, "POST", body);
}

void OneSevenLiveYouTubeClient::deleteLiveStream(const QString& streamId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "deleteLiveStream");
        return;
    }

    if (streamId.isEmpty()) {
        emit errorOccurred("Stream ID cannot be empty", "deleteLiveStream");
        return;
    }

    QMap<QString, QString> params;
    params["id"] = streamId;

    QString endpoint = buildApiUrl("liveStreams", params);
    makeApiRequest(endpoint, "DELETE");
}

void OneSevenLiveYouTubeClient::getMyLiveBroadcasts(const QString& broadcastStatus) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "getMyLiveBroadcasts");
        return;
    }

    QMap<QString, QString> params;
    params["mine"] = "true";
    params["part"] = "snippet";
    if (!broadcastStatus.isEmpty()) {
        params["broadcastStatus"] = broadcastStatus;
    }
    if (m_hasValidAuth && !m_accessToken.isEmpty()) {
        params["access_token"] = m_accessToken;
    }

    QString endpoint = buildApiUrl("liveBroadcasts", params);
    m_currentOperation = "getMyLiveBroadcasts";
    makeApiRequest(endpoint);
}

void OneSevenLiveYouTubeClient::setApiKey(const QString& apiKey) {
    m_apiKey = apiKey;
    obs_log(LOG_INFO, "YouTube API key set");
}

void OneSevenLiveYouTubeClient::setTimeout(int timeoutMs) {
    m_timeoutMs = timeoutMs;
    obs_log(LOG_INFO, "API timeout set to %d ms", timeoutMs);
}

void OneSevenLiveYouTubeClient::makeApiRequest(const QString& endpoint, const QString& method,
                                               const QString& body) {
    obs_log(LOG_INFO, "YouTube API Request: %s %s", method.toUtf8().constData(),
            endpoint.toUtf8().constData());
    if (!body.isEmpty()) {
        obs_log(LOG_INFO, "Request body: %s", body.toUtf8().constData());
    }
    m_lastEndpoint = endpoint;
    m_lastMethod = method;
    m_lastBody = body;
    obs_log(LOG_DEBUG, "YouTube API auth present=%s token_len=%d",
            m_hasValidAuth ? "true" : "false", m_accessToken.size());
    if (m_hasValidAuth) {
        const QString tok = m_accessToken;
        const QString masked = tok.length() >= 12 ? tok.left(6) + "..." + tok.right(6) : tok;
        const char* mode =
            (m_currentOperation == "getMyLiveBroadcasts") ? "QueryParam+Bearer" : "Bearer";
        obs_log(LOG_DEBUG, "YouTube API token(masked)=%s auth_mode=%s", masked.toUtf8().constData(),
                mode);
    }

    // Build headers
    std::vector<std::string> headers;
    headers.push_back(std::string("Accept: application/json"));
    if (method != "GET") {
        headers.push_back(std::string("Content-Type: application/json"));
    }
    if (m_hasValidAuth && !m_accessToken.isEmpty()) {
        std::string bearer = std::string("Authorization: Bearer ") + m_accessToken.toStdString();
        headers.push_back(bearer);
    }

    RemoteTextThread* thread = new RemoteTextThread(
        endpoint.toStdString(), std::move(headers), "application/json",
        method == "POST" || method == "PUT" ? body.toStdString() : std::string(),
        /*timeoutSec=*/m_timeoutMs / 1000,
        /*isImageRequest=*/false);

    if (m_currentOperation.isEmpty()) {
        if (m_currentOperation.isEmpty()) {
            m_currentOperation = method == "GET"
                                     ? "getLiveStreams"
                                     : (method == "POST" ? "createLiveStream" : "API request");
        }
    }

    connect(thread, &RemoteTextThread::Result, this,
            &OneSevenLiveYouTubeClient::onApiRequestFinished);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QString OneSevenLiveYouTubeClient::buildApiUrl(const QString& endpoint,
                                               const QMap<QString, QString>& params) const {
    QString url = YOUTUBE_API_BASE_URL + "/" + endpoint;

    QUrlQuery query;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        query.addQueryItem(it.key(), it.value());
    }
    if (!query.isEmpty()) {
        url += "?" + query.toString();
    }

    return url;
}

void OneSevenLiveYouTubeClient::onApiRequestFinished(const QString& response,
                                                     const QString& error) {
    if (!error.isEmpty()) {
        int httpStatus = -1;
        QRegularExpression statusRegex(R"(HTTP (\d{3}))");
        QRegularExpressionMatch match = statusRegex.match(error);
        if (match.hasMatch()) {
            httpStatus = match.captured(1).toInt();
        }
        if (httpStatus == -1) {
            QRegularExpression returnedRegex(R"(returned error:\s*(\d{3}))");
            QRegularExpressionMatch m2 = returnedRegex.match(error);
            if (m2.hasMatch())
                httpStatus = m2.captured(1).toInt();
        }
        obs_log(LOG_WARNING, "YouTube API Error: status=%d op=%s method=%s endpoint=%s", httpStatus,
                m_currentOperation.toUtf8().constData(), m_lastMethod.toUtf8().constData(),
                m_lastEndpoint.toUtf8().constData());
        if (!response.isEmpty()) {
            obs_log(LOG_WARNING, "YouTube API Error response: %s", response.toUtf8().constData());
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
                            "YouTube API Error details: code=%d message=%s status=%s reason=%s",
                            ecode, emsg.c_str(), estatus.c_str(), ereason.c_str());
                }
            } catch (...) {
            }
        }
        handleApiError(error, m_currentOperation, httpStatus);
        return;
    }

    // Assume success when error is empty
    {
        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.toStdString());
        } catch (const std::exception& e) {
            emit errorOccurred(QString("Failed to parse JSON response: ") + e.what(),
                               "API request");
            return;
        }

        // Determine the operation based on URL
        if (m_currentOperation == "getLiveStreams") {
            if (json.contains("items")) {
                // This is a GET request for stream(s)
                YouTubeLiveStreamListResponse streamList = parseLiveStreamListResponse(json);
                emit myLiveStreamsReceived(streamList);
                emit requestCompleted("getLiveStreams");
            } else {
                // This might be a POST request (create)
                YouTubeLiveStream stream = parseLiveStream(json);
                emit liveStreamCreated(stream);
                emit requestCompleted("createLiveStream");
            }
        } else if (m_currentOperation == "getMyLiveBroadcasts") {
            YouTubeLiveBroadcastListResponse broadcasts = parseLiveBroadcastListResponse(json);
            emit myLiveBroadcastsReceived(broadcasts);
            emit requestCompleted("getMyLiveBroadcasts");
        }
    }
}

void OneSevenLiveYouTubeClient::onApiRequestError(const QString& response, const QString& error) {
    obs_log(LOG_WARNING, "YouTube API Error: %s : %s", error.toUtf8().constData(),
            response.toUtf8().constData());
    handleApiError(error, m_currentOperation, -1);
}

void OneSevenLiveYouTubeClient::handleApiError(const QString& error, const QString& operation,
                                               int httpStatus) {
    QString detailedError;

    switch (httpStatus) {
    case 401:
        detailedError = "Authentication failed - invalid or expired token";
        m_hasValidAuth = false;
        break;
    case 403:
        detailedError = "Access forbidden - insufficient permissions";
        break;
    case 404:
        detailedError = "Resource not found";
        break;
    case 429:
        detailedError = "Rate limit exceeded";
        break;
    default:
        detailedError = error;
        break;
    }

    emit errorOccurred(detailedError, operation);
}

YouTubeLiveStream OneSevenLiveYouTubeClient::parseLiveStream(const nlohmann::json& json) const {
    YouTubeLiveStream stream;

    stream.kind = QString::fromStdString(json.value("kind", ""));
    stream.etag = QString::fromStdString(json.value("etag", ""));
    stream.id = QString::fromStdString(json.value("id", ""));

    if (json.contains("snippet") && json["snippet"].is_object()) {
        stream.snippet = parseSnippet(json["snippet"]);
    }

    if (json.contains("cdn") && json["cdn"].is_object()) {
        stream.cdn = parseCdn(json["cdn"]);
    }

    if (json.contains("status") && json["status"].is_object()) {
        stream.status = parseStatus(json["status"]);
    }

    if (json.contains("contentDetails") && json["contentDetails"].is_object()) {
        stream.contentDetails = parseContentDetails(json["contentDetails"]);
    }

    return stream;
}

YouTubeLiveStreamSnippet OneSevenLiveYouTubeClient::parseSnippet(const nlohmann::json& json) const {
    YouTubeLiveStreamSnippet snippet;

    snippet.publishedAt = QString::fromStdString(json.value("publishedAt", ""));
    snippet.channelId = QString::fromStdString(json.value("channelId", ""));
    snippet.title = QString::fromStdString(json.value("title", ""));
    snippet.description = QString::fromStdString(json.value("description", ""));
    snippet.isDefaultStream = json.value("isDefaultStream", false);

    return snippet;
}

YouTubeLiveStreamCdn OneSevenLiveYouTubeClient::parseCdn(const nlohmann::json& json) const {
    YouTubeLiveStreamCdn cdn;

    cdn.ingestionType = QString::fromStdString(json.value("ingestionType", ""));
    cdn.resolution = QString::fromStdString(json.value("resolution", ""));
    cdn.frameRate = QString::fromStdString(json.value("frameRate", ""));

    if (json.contains("ingestionInfo") && json["ingestionInfo"].is_object()) {
        cdn.ingestionInfo = parseIngestionInfo(json["ingestionInfo"]);
    }

    return cdn;
}

YouTubeLiveStreamIngestionInfo OneSevenLiveYouTubeClient::parseIngestionInfo(
    const nlohmann::json& json) const {
    YouTubeLiveStreamIngestionInfo info;

    info.streamName = QString::fromStdString(json.value("streamName", ""));
    info.ingestionAddress = QString::fromStdString(json.value("ingestionAddress", ""));
    info.backupIngestionAddress = QString::fromStdString(json.value("backupIngestionAddress", ""));
    info.rtmpsIngestionAddress = QString::fromStdString(json.value("rtmpsIngestionAddress", ""));
    info.rtmpsBackupIngestionAddress =
        QString::fromStdString(json.value("rtmpsBackupIngestionAddress", ""));

    return info;
}

YouTubeLiveStreamStatus OneSevenLiveYouTubeClient::parseStatus(const nlohmann::json& json) const {
    YouTubeLiveStreamStatus status;

    status.streamStatus = QString::fromStdString(json.value("streamStatus", ""));

    if (json.contains("healthStatus") && json["healthStatus"].is_object()) {
        status.healthStatus = parseHealthStatus(json["healthStatus"]);
    }

    return status;
}

YouTubeLiveStreamHealthStatus OneSevenLiveYouTubeClient::parseHealthStatus(
    const nlohmann::json& json) const {
    YouTubeLiveStreamHealthStatus healthStatus;
    healthStatus.status = QString::fromStdString(json.value("status", ""));
    return healthStatus;
}

YouTubeLiveStreamContentDetails OneSevenLiveYouTubeClient::parseContentDetails(
    const nlohmann::json& json) const {
    YouTubeLiveStreamContentDetails details;

    details.closedCaptionsIngestionUrl =
        QString::fromStdString(json.value("closedCaptionsIngestionUrl", ""));
    details.isReusable = json.value("isReusable", true);

    return details;
}

YouTubeLiveStreamListResponse OneSevenLiveYouTubeClient::parseLiveStreamListResponse(
    const nlohmann::json& json) const {
    YouTubeLiveStreamListResponse response;

    response.kind = QString::fromStdString(json.value("kind", ""));
    response.etag = QString::fromStdString(json.value("etag", ""));

    if (json.contains("pageInfo") && json["pageInfo"].is_object()) {
        auto pageInfo = json["pageInfo"];
        response.pageInfo.totalResults = pageInfo.value("totalResults", 0);
        response.pageInfo.resultsPerPage = pageInfo.value("resultsPerPage", 0);
    }

    if (json.contains("items") && json["items"].is_array()) {
        for (const auto& item : json["items"]) {
            if (item.is_object()) {
                YouTubeLiveStream stream = parseLiveStream(item);
                response.items.append(stream);
            }
        }
    }

    return response;
}

YouTubeLiveBroadcastSnippet OneSevenLiveYouTubeClient::parseLiveBroadcastSnippet(
    const nlohmann::json& json) const {
    YouTubeLiveBroadcastSnippet snippet;
    snippet.title = QString::fromStdString(json.value("title", ""));
    snippet.channelId = QString::fromStdString(json.value("channelId", ""));
    snippet.scheduledStartTime = QString::fromStdString(json.value("scheduledStartTime", ""));
    snippet.actualStartTime = QString::fromStdString(json.value("actualStartTime", ""));
    snippet.liveChatId = QString::fromStdString(json.value("liveChatId", ""));
    return snippet;
}

YouTubeLiveBroadcastStatus OneSevenLiveYouTubeClient::parseLiveBroadcastStatus(
    const nlohmann::json& json) const {
    YouTubeLiveBroadcastStatus status;
    status.lifeCycleStatus = QString::fromStdString(json.value("lifeCycleStatus", ""));
    return status;
}

YouTubeLiveBroadcast OneSevenLiveYouTubeClient::parseLiveBroadcast(
    const nlohmann::json& json) const {
    YouTubeLiveBroadcast b;
    b.kind = QString::fromStdString(json.value("kind", ""));
    b.etag = QString::fromStdString(json.value("etag", ""));
    b.id = QString::fromStdString(json.value("id", ""));
    if (json.contains("snippet") && json["snippet"].is_object()) {
        b.snippet = parseLiveBroadcastSnippet(json["snippet"]);
    }
    if (json.contains("status") && json["status"].is_object()) {
        b.status = parseLiveBroadcastStatus(json["status"]);
    }
    return b;
}

YouTubeLiveBroadcastListResponse OneSevenLiveYouTubeClient::parseLiveBroadcastListResponse(
    const nlohmann::json& json) const {
    YouTubeLiveBroadcastListResponse resp;
    resp.kind = QString::fromStdString(json.value("kind", ""));
    resp.etag = QString::fromStdString(json.value("etag", ""));
    if (json.contains("items") && json["items"].is_array()) {
        for (const auto& item : json["items"]) {
            if (item.is_object()) {
                resp.items.append(parseLiveBroadcast(item));
            }
        }
    }
    return resp;
}
