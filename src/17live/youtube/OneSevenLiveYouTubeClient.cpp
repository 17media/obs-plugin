#include "OneSevenLiveYouTubeClient.hpp"

#include <obs-module.h>

#include <QRegularExpression>
#include <QUrlQuery>
#include <nlohmann/json.hpp>
#include <QTimer>
#include <QDateTime>

#include "OneSevenLiveCoreManager.hpp"
#include "plugin-support.h"
#include "utility/RemoteTextThread.hpp"
#include "youtube/OneSevenLiveYouTubeAuth.hpp"

static nlohmann::json prepareResetBody(const nlohmann::json& broadcast) {
    nlohmann::json data;
    data["id"] = broadcast["id"];
    
    if (broadcast.contains("snippet")) {
        auto snippet = broadcast["snippet"];
        nlohmann::json newSnippet;
        newSnippet["title"] = snippet["title"];
        newSnippet["description"] = snippet.value("description", "");
        newSnippet["scheduledStartTime"] = snippet["scheduledStartTime"];
        if (snippet.contains("scheduledEndTime"))
            newSnippet["scheduledEndTime"] = snippet["scheduledEndTime"];
        data["snippet"] = newSnippet;
    }
    
    if (broadcast.contains("status")) {
        auto status = broadcast["status"];
        nlohmann::json newStatus;
        newStatus["privacyStatus"] = status["privacyStatus"];
        if (status.contains("madeForKids"))
            newStatus["madeForKids"] = status["madeForKids"];
        if (status.contains("selfDeclaredMadeForKids"))
            newStatus["selfDeclaredMadeForKids"] = status["selfDeclaredMadeForKids"];
        data["status"] = newStatus;
    }
    
    if (broadcast.contains("contentDetails")) {
        auto cd = broadcast["contentDetails"];
        nlohmann::json newCd;
        
        nlohmann::json mon;
        mon["enableMonitorStream"] = false;
        if (cd.contains("monitorStream") && cd["monitorStream"].contains("broadcastStreamDelayMs")) {
            mon["broadcastStreamDelayMs"] = cd["monitorStream"]["broadcastStreamDelayMs"];
        }
        newCd["monitorStream"] = mon;
        
        const char* copyFields[] = {
            "enableAutoStart", "enableAutoStop", "enableClosedCaptions", 
            "enableDvr", "enableContentEncryption", "enableEmbed", 
            "recordFromStart", "startWithSlate"
        };
        
        for (const char* field : copyFields) {
            if (cd.contains(field))
                newCd[field] = cd[field];
        }
        data["contentDetails"] = newCd;
    }
    
    return data;
}

const QString OneSevenLiveYouTubeClient::YOUTUBE_API_BASE_URL =
    "https://www.googleapis.com/youtube/v3";
const QString OneSevenLiveYouTubeClient::YOUTUBE_API_VERSION = "v3";

OneSevenLiveYouTubeClient::OneSevenLiveYouTubeClient(QObject* parent)
    : QObject(parent),
      m_timeoutMs(30000)  // 30 seconds default timeout
      ,
      m_hasValidAuth(false) {}

OneSevenLiveYouTubeClient::~OneSevenLiveYouTubeClient() {
    disconnect(this);
}

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
    m_currentOperation = "getLiveStreams";
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
    m_currentOperation = "getLiveStreams";
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
    nlohmann::json cdn;
    cdn["ingestionType"] = "rtmp";
    cdn["resolution"] = "variable";
    cdn["frameRate"] = "variable";
    requestBody["cdn"] = cdn;
    nlohmann::json contentDetails;
    contentDetails["isReusable"] = false;
    requestBody["contentDetails"] = contentDetails;

    QString body = QString::fromStdString(requestBody.dump());

    QMap<QString, QString> params;
    params["part"] = "snippet,cdn,status,contentDetails";

    QString endpoint = buildApiUrl("liveStreams", params);
    m_currentOperation = "createLiveStream";
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
    m_currentOperation = "deleteLiveStream";
    makeApiRequest(endpoint, "DELETE");
}

void OneSevenLiveYouTubeClient::getMyLiveBroadcasts(const QString& broadcastStatus, const QString& pageToken) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "getMyLiveBroadcasts");
        return;
    }

    QMap<QString, QString> params;
    params["part"] = "snippet,contentDetails,status";
    params["broadcastType"] = "all";
    params["maxResults"] = "50";

    if (broadcastStatus.isEmpty()) {
        params["mine"] = "true";
    } else {
        params["broadcastStatus"] = broadcastStatus;
    }

    if (!pageToken.isEmpty()) {
        params["pageToken"] = pageToken;
    }

    QString endpoint = buildApiUrl("liveBroadcasts", params);
    obs_log(LOG_INFO, "YouTube getMyLiveBroadcasts request_url=%s status=%s page=%s",
            endpoint.toUtf8().constData(),
            (broadcastStatus.isEmpty() ? "all(default)" : broadcastStatus.toUtf8().constData()),
            pageToken.toUtf8().constData());
    m_currentOperation = "getMyLiveBroadcasts";
    makeApiRequest(endpoint);
}

void OneSevenLiveYouTubeClient::getLiveBroadcastById(const QString& broadcastId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "getLiveBroadcastById");
        return;
    }
    if (broadcastId.isEmpty()) {
        emit errorOccurred("Broadcast ID cannot be empty", "getLiveBroadcastById");
        return;
    }
    QMap<QString, QString> params;
    params["id"] = broadcastId;
    params["part"] = "snippet,status";
    QString endpoint = buildApiUrl("liveBroadcasts", params);
    m_currentOperation = "getLiveBroadcastById";
    makeApiRequest(endpoint);
}

void OneSevenLiveYouTubeClient::setApiKey(const QString& apiKey) {
    m_apiKey = apiKey;
    obs_log(LOG_INFO, "YouTube API key set");
}

void OneSevenLiveYouTubeClient::setTimeout(int timeoutMs) {
    m_timeoutMs = timeoutMs;
    // obs_log(LOG_INFO, "API timeout set to %d ms", timeoutMs);
}

void OneSevenLiveYouTubeClient::createLiveBroadcast(const QString& title, const QString& privacyStatus,
                                                    const QString& latency, bool autoStart,
                                                    bool autoStop, bool dvr, bool scheduleLater) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "createLiveBroadcast");
        return;
    }
    nlohmann::json req;
    nlohmann::json sn;
    sn["title"] = title.toStdString();
    {
        QDateTime startUtc = QDateTime::currentDateTimeUtc();
        if (scheduleLater) {
             startUtc = startUtc.addSecs(3600);
        } else {
             startUtc = startUtc.addSecs(60);
        }
        QString scheduled =
            startUtc.toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss'Z'"));
        sn["scheduledStartTime"] = scheduled.toStdString();
    }
    req["snippet"] = sn;
    nlohmann::json cd;
    nlohmann::json mon;
    mon["enableMonitorStream"] = false;
    cd["monitorStream"] = mon;
    cd["enableAutoStart"] = autoStart;
    cd["enableAutoStop"] = autoStop;
    cd["enableDvr"] = dvr;
    cd["latencyPreference"] = latency.toStdString();
    req["contentDetails"] = cd;
    nlohmann::json st;
    st["privacyStatus"] = privacyStatus.toStdString();
    req["status"] = st;
    QString body = QString::fromStdString(req.dump());
    QMap<QString, QString> params;
    params["part"] = "snippet,contentDetails,status";
    QString endpoint = buildApiUrl("liveBroadcasts", params);
    m_currentOperation = "createLiveBroadcast";
    makeApiRequest(endpoint, "POST", body);
}

void OneSevenLiveYouTubeClient::bindLiveBroadcast(const QString& broadcastId,
                                                  const QString& streamId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "bindLiveBroadcast");
        return;
    }
    if (broadcastId.isEmpty() || streamId.isEmpty()) {
        emit errorOccurred("Broadcast ID or stream ID cannot be empty", "bindLiveBroadcast");
        return;
    }
    m_lastBroadcastId = broadcastId;
    m_lastStreamId = streamId;
    QMap<QString, QString> params;
    params["part"] = "id,contentDetails";
    params["id"] = broadcastId;
    params["streamId"] = streamId;
    QString endpoint = buildApiUrl("liveBroadcasts/bind", params);
    m_currentOperation = "bindLiveBroadcast";
    makeApiRequest(endpoint, "POST");
}

void OneSevenLiveYouTubeClient::transitionLiveBroadcast(const QString& broadcastId,
                                                        const QString& status) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "transitionLiveBroadcast");
        return;
    }
    m_lastBroadcastId = broadcastId;
    m_lastTransitionStatus = status;
    QMap<QString, QString> params;
    params["part"] = "status";
    params["id"] = broadcastId;
    params["broadcastStatus"] = status;
    QString endpoint = buildApiUrl("liveBroadcasts/transition", params);
    m_currentOperation = "transitionLiveBroadcast";
    makeApiRequest(endpoint, "POST");
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

    std::atomic<bool>* cancelFlag = OneSevenLiveCoreManager::getInstance().getCancelFlag();
    // For critical operations like transitioning broadcast state (stop), 
    // we don't want the shutdown cancel flag to abort the request.
    if (m_currentOperation == "transitionLiveBroadcast") {
        cancelFlag = nullptr;
    }

    std::string requestBody = body.toStdString();
    // RemoteTextThread defaults to GET if postData is empty. 
    // We must provide an empty JSON object to force POST/PUT for empty bodies.
    if (requestBody.empty() && (method == "POST" || method == "PUT")) {
        requestBody = "{}";
    }

    RemoteTextThread* thread = new RemoteTextThread(
        endpoint.toStdString(), std::move(headers), "application/json",
        requestBody,
        m_timeoutMs / 1000, false, cancelFlag);

    if (m_currentOperation.isEmpty()) {
        if (m_currentOperation.isEmpty()) {
            m_currentOperation = method == "GET"
                                     ? "getLiveStreams"
                                     : (method == "POST" ? "createLiveStream" : "API request");
        }
    }

    connect(thread, &RemoteTextThread::Result, this,
            &OneSevenLiveYouTubeClient::onApiRequestFinished, Qt::QueuedConnection);
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
        if (m_currentOperation == "bindLiveBroadcast") {
            obs_log(LOG_INFO, "YouTube bindLiveBroadcast success: endpoint=%s response_len=%d",
                    m_lastEndpoint.toUtf8().constData(), response.size());
            if (!response.isEmpty()) {
                obs_log(LOG_INFO, "YouTube bindLiveBroadcast response: %s",
                        response.toUtf8().constData());
            } else {
                obs_log(LOG_INFO, "YouTube bindLiveBroadcast response is empty");
            }
            emit liveBroadcastBound(m_lastBroadcastId, m_lastStreamId);
            QMetaObject::invokeMethod(
                this, [this]() { emit requestCompleted(QString("bindLiveBroadcast")); },
                Qt::QueuedConnection);
            return;
        }

        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.toStdString());
        } catch (const std::exception& e) {
            emit errorOccurred(QString("Failed to parse JSON response: ") + e.what(),
                               "API request");
            return;
        }

        // Determine the operation based on URL
        if (m_currentOperation == "getLiveStreams" || m_currentOperation == "createLiveStream") {
            if (json.contains("items")) {
                // This is a GET request for stream(s)
                YouTubeLiveStreamListResponse streamList = parseLiveStreamListResponse(json);
                emit myLiveStreamsReceived(streamList);
                if (m_waitingStreamActiveForStart) {
                    bool active = false;
                    for (const auto& s : streamList.items) {
                        if (s.id == m_boundStreamIdForStart) {
                            active = (s.status.streamStatus.compare("active", Qt::CaseInsensitive) == 0);
                            break;
                        }
                    }
                    if (active) {
                        m_waitingStreamActiveForStart = false;
                        if (m_streamActivePollTimer && m_streamActivePollTimer->isActive())
                            m_streamActivePollTimer->stop();
                        transitionLiveBroadcast(m_pendingStartBroadcastId, "live");
                        return;
                    } else {
                        m_streamActivePollAttempts++;
                        if (m_streamActivePollAttempts >= 30) {
                            m_waitingStreamActiveForStart = false;
                            if (m_streamActivePollTimer && m_streamActivePollTimer->isActive())
                                m_streamActivePollTimer->stop();
                            transitionLiveBroadcast(m_pendingStartBroadcastId, "live");
                            return;
                        }
                        if (!m_streamActivePollTimer) {
                            m_streamActivePollTimer = new QTimer(this);
                            m_streamActivePollTimer->setSingleShot(true);
                            connect(m_streamActivePollTimer, &QTimer::timeout, this, [this]() {
                                if (!m_boundStreamIdForStart.isEmpty())
                                    getLiveStreamById(m_boundStreamIdForStart);
                            });
                        }
                        m_streamActivePollTimer->start(1000);
                    }
                }
                QMetaObject::invokeMethod(
                    this, [this]() { emit requestCompleted(QString("getLiveStreams")); },
                    Qt::QueuedConnection);
            } else {
                // This might be a POST request (create)
                YouTubeLiveStream stream = parseLiveStream(json);
                if (stream.id.isEmpty()) {
                    obs_log(LOG_WARNING,
                            "YouTube createLiveStream returned stream with empty id. Raw response: "
                            "%s",
                            response.toUtf8().constData());
                }
                emit liveStreamCreated(stream);
                QMetaObject::invokeMethod(
                    this, [this]() { emit requestCompleted(QString("createLiveStream")); },
                    Qt::QueuedConnection);
            }
        } else if (m_currentOperation == "getMyLiveBroadcasts") {
            YouTubeLiveBroadcastListResponse broadcasts = parseLiveBroadcastListResponse(json);
            emit myLiveBroadcastsReceived(broadcasts);
            QMetaObject::invokeMethod(
                this, [this]() { emit requestCompleted(QString("getMyLiveBroadcasts")); },
                Qt::QueuedConnection);
        } else if (m_currentOperation == "getLiveBroadcastById") {
            YouTubeLiveBroadcast b;
            try {
                if (json.contains("items") && json["items"].is_array() && !json["items"].empty()) {
                    auto x = json["items"][0];
                    b = parseLiveBroadcast(x);
                }
            } catch (...) {
            }
            emit liveBroadcastReceived(b);
            QMetaObject::invokeMethod(
                this, [this]() { emit requestCompleted(QString("getLiveBroadcastById")); },
                Qt::QueuedConnection);
        } else if (m_currentOperation == "createLiveBroadcast") {
            QString id;
            try {
                if (json.contains("id") && json["id"].is_string()) {
                    id = QString::fromStdString(json["id"].get<std::string>());
                }
            } catch (...) {
            }
            if (id.isEmpty()) {
                obs_log(LOG_WARNING,
                        "YouTube createLiveBroadcast returned no id. Raw response: %s",
                        response.toUtf8().constData());
            }
            emit liveBroadcastCreated(id);
            QMetaObject::invokeMethod(
                this, [this]() { emit requestCompleted(QString("createLiveBroadcast")); },
                Qt::QueuedConnection);
        } else if (m_currentOperation == "transitionLiveBroadcast") {
            emit liveBroadcastTransitioned(m_lastBroadcastId, m_lastTransitionStatus);
            QMetaObject::invokeMethod(
                this, [this]() { emit requestCompleted(QString("transitionLiveBroadcast")); },
                Qt::QueuedConnection);
        } else if (m_currentOperation == "startBroadcast" || m_currentOperation == "resetBroadcast") {
            try {
                if (json.contains("items") && json["items"].is_array() && !json["items"].empty()) {
                    auto broadcast = json["items"][0];
                    QString id = QString::fromStdString(broadcast["id"].get<std::string>());
                    QString boundStreamId;
                    try {
                        if (broadcast.contains("contentDetails") &&
                            broadcast["contentDetails"].contains("boundStreamId") &&
                            broadcast["contentDetails"]["boundStreamId"].is_string()) {
                            boundStreamId = QString::fromStdString(
                                broadcast["contentDetails"]["boundStreamId"].get<std::string>());
                        }
                    } catch (...) {
                    }
                    
                    if (m_currentOperation == "startBroadcast") {
                        std::string status = broadcast["status"]["lifeCycleStatus"].get<std::string>();
                        if (status == "live" || status == "liveStarting") {
                            emit requestCompleted("startBroadcast");
                            return;
                        } else if (status == "testStarting") {
                             emit errorOccurred("Broadcast is starting testing, please wait.", "startBroadcast");
                             return;
                        }
                        
                        bool monitorEnabled = false;
                        if (broadcast.contains("contentDetails") && 
                            broadcast["contentDetails"].contains("monitorStream") &&
                            broadcast["contentDetails"]["monitorStream"].contains("enableMonitorStream")) {
                            monitorEnabled = broadcast["contentDetails"]["monitorStream"]["enableMonitorStream"].get<bool>();
                        }
                        
                        if (status != "testing" && monitorEnabled) {
                            m_tempBroadcastJson = broadcast;
                            m_lastBroadcastId = id;
                            nlohmann::json resetData = prepareResetBody(broadcast);
                            QString body = QString::fromStdString(resetData.dump());
                            QMap<QString, QString> params;
                            params["part"] = "id,snippet,contentDetails,status";
                            QString endpoint = buildApiUrl("liveBroadcasts", params);
                            m_currentOperation = "resetBroadcastForStart";
                            makeApiRequest(endpoint, "PUT", body);
                            return;
                        }
                        {
                            QString effectiveStreamId = !m_boundStreamIdForStart.isEmpty() ? m_boundStreamIdForStart : boundStreamId;
                            if (!effectiveStreamId.isEmpty()) {
                                beginWaitStreamActiveAndTransition(id, effectiveStreamId);
                                return;
                            }
                        }
                        transitionLiveBroadcast(id, "live");
                        return;
                    } else if (m_currentOperation == "resetBroadcast") {
                         m_tempBroadcastJson = broadcast;
                         m_lastBroadcastId = id;
                         nlohmann::json resetData = prepareResetBody(broadcast);
                         QString body = QString::fromStdString(resetData.dump());
                         QMap<QString, QString> params;
                         params["part"] = "id,snippet,contentDetails,status";
                         QString endpoint = buildApiUrl("liveBroadcasts", params);
                         m_currentOperation = "resetBroadcastExec";
                         makeApiRequest(endpoint, "PUT", body);
                         return;
                    }
                }
            } catch (...) {
                emit errorOccurred("Failed to parse broadcast details", m_currentOperation);
            }
        } else if (m_currentOperation == "resetBroadcastForStart") {
             try {
                 QString boundStreamId;
                 if (m_tempBroadcastJson.contains("contentDetails") &&
                     m_tempBroadcastJson["contentDetails"].contains("boundStreamId") &&
                     m_tempBroadcastJson["contentDetails"]["boundStreamId"].is_string()) {
                     boundStreamId = QString::fromStdString(
                         m_tempBroadcastJson["contentDetails"]["boundStreamId"].get<std::string>());
                 }
                 {
                     QString effectiveStreamId = !m_boundStreamIdForStart.isEmpty() ? m_boundStreamIdForStart : boundStreamId;
                     if (!effectiveStreamId.isEmpty()) {
                         beginWaitStreamActiveAndTransition(m_lastBroadcastId, effectiveStreamId);
                         return;
                     }
                 }
             } catch (...) {
             }
             transitionLiveBroadcast(m_lastBroadcastId, "live");
             return;
        } else if (m_currentOperation == "resetBroadcastExec") {
             emit requestCompleted("resetBroadcast");
             return;
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
        if (auto* auth = OneSevenLiveCoreManager::getInstance().getYouTubeAuth()) {
            QTimer::singleShot(0, auth, &OneSevenLiveYouTubeAuth::refreshAccessTokenAsync);
        }
        m_retryPending = true;
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

void OneSevenLiveYouTubeClient::beginWaitStreamActiveAndTransition(const QString& broadcastId,
                                                                   const QString& streamId) {
    m_pendingStartBroadcastId = broadcastId;
    m_boundStreamIdForStart = streamId;
    m_waitingStreamActiveForStart = true;
    m_streamActivePollAttempts = 0;
    getLiveStreamById(streamId);
}

void OneSevenLiveYouTubeClient::retryLastRequest() {
    if (!m_retryPending)
        return;
    if (!m_hasValidAuth || m_lastEndpoint.isEmpty() || m_lastMethod.isEmpty())
        return;
    m_retryPending = false;
    makeApiRequest(m_lastEndpoint, m_lastMethod, m_lastBody);
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
    if (json.contains("actualEndTime") && json["actualEndTime"].is_string()) {
        snippet.actualEndTime = QString::fromStdString(json["actualEndTime"].get<std::string>());
    }
    snippet.liveChatId = QString::fromStdString(json.value("liveChatId", ""));
    return snippet;
}

YouTubeLiveBroadcastStatus OneSevenLiveYouTubeClient::parseLiveBroadcastStatus(
    const nlohmann::json& json) const {
    YouTubeLiveBroadcastStatus status;
    status.lifeCycleStatus = QString::fromStdString(json.value("lifeCycleStatus", ""));
    return status;
}

YouTubeLiveBroadcastContentDetails OneSevenLiveYouTubeClient::parseLiveBroadcastContentDetails(
    const nlohmann::json& json) const {
    YouTubeLiveBroadcastContentDetails cd;
    try {
        if (json.contains("boundStreamId") && json["boundStreamId"].is_string()) {
            cd.boundStreamId = QString::fromStdString(json["boundStreamId"].get<std::string>());
        }
        if (json.contains("monitorStream") && json["monitorStream"].is_object()) {
            const auto& mon = json["monitorStream"];
            if (mon.contains("enableMonitorStream") && mon["enableMonitorStream"].is_boolean()) {
                cd.enableMonitorStream = mon["enableMonitorStream"].get<bool>();
            }
        }
    } catch (...) {
    }
    return cd;
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
    if (json.contains("contentDetails") && json["contentDetails"].is_object()) {
        b.contentDetails = parseLiveBroadcastContentDetails(json["contentDetails"]);
    }
    return b;
}

YouTubeLiveBroadcastListResponse OneSevenLiveYouTubeClient::parseLiveBroadcastListResponse(
    const nlohmann::json& json) const {
    YouTubeLiveBroadcastListResponse resp;
    resp.kind = QString::fromStdString(json.value("kind", ""));
    resp.etag = QString::fromStdString(json.value("etag", ""));
    resp.nextPageToken = QString::fromStdString(json.value("nextPageToken", ""));
    if (json.contains("items") && json["items"].is_array()) {
        for (const auto& item : json["items"]) {
            if (item.is_object()) {
                resp.items.append(parseLiveBroadcast(item));
            }
        }
    }
    return resp;
}

void OneSevenLiveYouTubeClient::startBroadcast(const QString& broadcastId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "startBroadcast");
        return;
    }
    m_lastBroadcastId = broadcastId;
    m_currentOperation = "startBroadcast";
    QMap<QString, QString> params;
    params["id"] = broadcastId;
    params["part"] = "id,snippet,contentDetails,status";
    QString endpoint = buildApiUrl("liveBroadcasts", params);
    makeApiRequest(endpoint);
}

void OneSevenLiveYouTubeClient::startBroadcast(const QString& broadcastId, const QString& boundStreamId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "startBroadcast");
        return;
    }
    m_boundStreamIdForStart = boundStreamId;
    startBroadcast(broadcastId);
}

void OneSevenLiveYouTubeClient::stopBroadcast(const QString& broadcastId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "stopBroadcast");
        return;
    }
    transitionLiveBroadcast(broadcastId, "complete");
}

void OneSevenLiveYouTubeClient::resetBroadcast(const QString& broadcastId) {
    if (!m_hasValidAuth) {
        emit errorOccurred("No valid authentication token", "resetBroadcast");
        return;
    }
    m_currentOperation = "resetBroadcast";
    QMap<QString, QString> params;
    params["id"] = broadcastId;
    params["part"] = "id,snippet,contentDetails,status";
    QString endpoint = buildApiUrl("liveBroadcasts", params);
    makeApiRequest(endpoint);
}
