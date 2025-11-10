#include "OneSevenLiveYouTubeClient.hpp"
#include "utility/RemoteTextThread.hpp"
#include <nlohmann/json.hpp>
#include <QDebug>
#include <QUrlQuery>
#include <QRegularExpression>

const QString OneSevenLiveYouTubeClient::YOUTUBE_API_BASE_URL = "https://www.googleapis.com/youtube/v3";
const QString OneSevenLiveYouTubeClient::YOUTUBE_API_VERSION = "v3";

OneSevenLiveYouTubeClient::OneSevenLiveYouTubeClient(QObject* parent)
    : QObject(parent)
    , m_timeoutMs(30000) // 30 seconds default timeout
    , m_hasValidAuth(false)
{
}

OneSevenLiveYouTubeClient::~OneSevenLiveYouTubeClient() = default;

void OneSevenLiveYouTubeClient::setAccessToken(const QString& accessToken)
{
    m_accessToken = accessToken;
    m_hasValidAuth = !accessToken.isEmpty();
    qDebug() << "YouTube access token set, valid:" << m_hasValidAuth;
}

bool OneSevenLiveYouTubeClient::hasValidAuth() const
{
    return m_hasValidAuth;
}

void OneSevenLiveYouTubeClient::getMyLiveStreams()
{
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

void OneSevenLiveYouTubeClient::getLiveStreamById(const QString& streamId)
{
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

void OneSevenLiveYouTubeClient::createLiveStream(const QString& title, const QString& description)
{
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

void OneSevenLiveYouTubeClient::deleteLiveStream(const QString& streamId)
{
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

void OneSevenLiveYouTubeClient::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey;
    qDebug() << "YouTube API key set";
}

void OneSevenLiveYouTubeClient::setTimeout(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
    qDebug() << "API timeout set to" << timeoutMs << "ms";
}

void OneSevenLiveYouTubeClient::makeApiRequest(const QString& endpoint, const QString& method, const QString& body)
{
    qDebug() << "YouTube API Request:" << method << endpoint;
    if (!body.isEmpty()) {
        qDebug() << "Request body:" << body;
    }
    
    // Build headers
    std::vector<std::string> headers;
    headers.push_back(std::string("Authorization: ") + QString("Bearer %1").arg(m_accessToken).toStdString());
    headers.push_back(std::string("Accept: application/json"));
    if (method != "GET") {
        headers.push_back(std::string("Content-Type: application/json"));
    }

    RemoteTextThread* thread = new RemoteTextThread(
        endpoint.toStdString(),
        std::move(headers),
        "application/json",
        method == "POST" || method == "PUT" ? body.toStdString() : std::string(),
        /*timeoutSec=*/m_timeoutMs / 1000,
        /*isImageRequest=*/false);

    m_currentOperation = method == "GET" ? "getLiveStreams" : (method == "POST" ? "createLiveStream" : "API request");

    connect(thread, &RemoteTextThread::Result, this, &OneSevenLiveYouTubeClient::onApiRequestFinished);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QString OneSevenLiveYouTubeClient::buildApiUrl(const QString& endpoint, const QMap<QString, QString>& params) const
{
    QString url = YOUTUBE_API_BASE_URL + "/" + endpoint;
    
    if (!params.isEmpty()) {
        QUrlQuery query;
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            query.addQueryItem(it.key(), it.value());
        }
        url += "?" + query.toString();
    }
    
    return url;
}

void OneSevenLiveYouTubeClient::onApiRequestFinished(const QString& response, const QString& error)
{
    if (!error.isEmpty()) {
        qWarning() << "YouTube API Error:" << error;
        handleApiError(error, m_currentOperation, -1);
        return;
    }

    // Assume success when error is empty
    {
        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.toStdString());
        } catch (const std::exception& e) {
            emit errorOccurred(QString("Failed to parse JSON response: ") + e.what(), "API request");
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
        }
    }
}

void OneSevenLiveYouTubeClient::onApiRequestError(const QString& response, const QString& error)
{
    qWarning() << "YouTube API Error:" << error << ":" << response;
    handleApiError(error, m_currentOperation, -1);
}

void OneSevenLiveYouTubeClient::handleApiError(const QString& error, const QString& operation, int httpStatus)
{
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

YouTubeLiveStream OneSevenLiveYouTubeClient::parseLiveStream(const nlohmann::json& json) const
{
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

YouTubeLiveStreamSnippet OneSevenLiveYouTubeClient::parseSnippet(const nlohmann::json& json) const
{
    YouTubeLiveStreamSnippet snippet;
    
    snippet.publishedAt = QString::fromStdString(json.value("publishedAt", ""));
    snippet.channelId = QString::fromStdString(json.value("channelId", ""));
    snippet.title = QString::fromStdString(json.value("title", ""));
    snippet.description = QString::fromStdString(json.value("description", ""));
    snippet.isDefaultStream = json.value("isDefaultStream", false);
    
    return snippet;
}

YouTubeLiveStreamCdn OneSevenLiveYouTubeClient::parseCdn(const nlohmann::json& json) const
{
    YouTubeLiveStreamCdn cdn;
    
    cdn.ingestionType = QString::fromStdString(json.value("ingestionType", ""));
    cdn.resolution = QString::fromStdString(json.value("resolution", ""));
    cdn.frameRate = QString::fromStdString(json.value("frameRate", ""));
    
    if (json.contains("ingestionInfo") && json["ingestionInfo"].is_object()) {
        cdn.ingestionInfo = parseIngestionInfo(json["ingestionInfo"]);
    }
    
    return cdn;
}

YouTubeLiveStreamIngestionInfo OneSevenLiveYouTubeClient::parseIngestionInfo(const nlohmann::json& json) const
{
    YouTubeLiveStreamIngestionInfo info;
    
    info.streamName = QString::fromStdString(json.value("streamName", ""));
    info.ingestionAddress = QString::fromStdString(json.value("ingestionAddress", ""));
    info.backupIngestionAddress = QString::fromStdString(json.value("backupIngestionAddress", ""));
    info.rtmpsIngestionAddress = QString::fromStdString(json.value("rtmpsIngestionAddress", ""));
    info.rtmpsBackupIngestionAddress = QString::fromStdString(json.value("rtmpsBackupIngestionAddress", ""));
    
    return info;
}

YouTubeLiveStreamStatus OneSevenLiveYouTubeClient::parseStatus(const nlohmann::json& json) const
{
    YouTubeLiveStreamStatus status;
    
    status.streamStatus = QString::fromStdString(json.value("streamStatus", ""));
    
    if (json.contains("healthStatus") && json["healthStatus"].is_object()) {
        status.healthStatus = parseHealthStatus(json["healthStatus"]);
    }
    
    return status;
}

YouTubeLiveStreamHealthStatus OneSevenLiveYouTubeClient::parseHealthStatus(const nlohmann::json& json) const
{
    YouTubeLiveStreamHealthStatus healthStatus;
    healthStatus.status = QString::fromStdString(json.value("status", ""));
    return healthStatus;
}

YouTubeLiveStreamContentDetails OneSevenLiveYouTubeClient::parseContentDetails(const nlohmann::json& json) const
{
    YouTubeLiveStreamContentDetails details;
    
    details.closedCaptionsIngestionUrl = QString::fromStdString(json.value("closedCaptionsIngestionUrl", ""));
    details.isReusable = json.value("isReusable", true);
    
    return details;
}

YouTubeLiveStreamListResponse OneSevenLiveYouTubeClient::parseLiveStreamListResponse(const nlohmann::json& json) const
{
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
