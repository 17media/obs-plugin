#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>
#include <nlohmann/json.hpp>

// Forward declarations
class RemoteTextThread;
class QTimer;

struct YouTubeLiveStreamSnippet {
    QString publishedAt;
    QString channelId;
    QString title;
    QString description;
    bool isDefaultStream;

    YouTubeLiveStreamSnippet() : isDefaultStream(false) {}
};

struct YouTubeLiveStreamIngestionInfo {
    QString streamName;
    QString ingestionAddress;
    QString backupIngestionAddress;
    QString rtmpsIngestionAddress;
    QString rtmpsBackupIngestionAddress;
};

struct YouTubeLiveStreamCdn {
    QString ingestionType;
    YouTubeLiveStreamIngestionInfo ingestionInfo;
    QString resolution;
    QString frameRate;

    YouTubeLiveStreamCdn() : ingestionType("rtmp"), resolution("variable"), frameRate("variable") {}
};

struct YouTubeLiveStreamHealthStatus {
    QString status;
};

struct YouTubeLiveStreamStatus {
    QString streamStatus;
    YouTubeLiveStreamHealthStatus healthStatus;

    YouTubeLiveStreamStatus() : streamStatus("inactive") {}
};

struct YouTubeLiveStreamContentDetails {
    QString closedCaptionsIngestionUrl;
    bool isReusable;

    YouTubeLiveStreamContentDetails() : isReusable(true) {}
};

struct YouTubeLiveStream {
    QString kind;
    QString etag;
    QString id;
    YouTubeLiveStreamSnippet snippet;
    YouTubeLiveStreamCdn cdn;
    YouTubeLiveStreamStatus status;
    YouTubeLiveStreamContentDetails contentDetails;

    YouTubeLiveStream() : kind("youtube#liveStream") {}
};

struct YouTubeLiveStreamListResponse {
    QString kind;
    QString etag;

    struct {
        int totalResults;
        int resultsPerPage;
    } pageInfo;

    QVector<YouTubeLiveStream> items;

    YouTubeLiveStreamListResponse() : kind("youtube#liveStreamListResponse"), pageInfo{0, 5} {}
};

struct YouTubeLiveBroadcastSnippet {
    QString liveChatId;
    QString title;
    QString channelId;
    QString scheduledStartTime;
    QString actualStartTime;
    QString actualEndTime;

    YouTubeLiveBroadcastSnippet() {}
};

struct YouTubeLiveBroadcastStatus {
    QString lifeCycleStatus;

    YouTubeLiveBroadcastStatus() {}
};

struct YouTubeLiveBroadcastContentDetails {
    QString boundStreamId;
    bool enableMonitorStream = false;
};

struct YouTubeLiveBroadcast {
    QString kind;
    QString etag;
    QString id;
    YouTubeLiveBroadcastSnippet snippet;
    YouTubeLiveBroadcastStatus status;
    YouTubeLiveBroadcastContentDetails contentDetails;

    YouTubeLiveBroadcast() : kind("youtube#liveBroadcast") {}
};

struct YouTubeLiveBroadcastListResponse {
    QString kind;
    QString etag;
    QString nextPageToken;
    QVector<YouTubeLiveBroadcast> items;

    YouTubeLiveBroadcastListResponse() : kind("youtube#liveBroadcastListResponse") {}
};

class OneSevenLiveYouTubeClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveYouTubeClient(QObject* parent = nullptr);
    ~OneSevenLiveYouTubeClient();

    // Authentication
    void setAccessToken(const QString& accessToken);
    bool hasValidAuth() const;

    // API Methods
    void getMyLiveStreams();
    void getLiveStreamById(const QString& streamId);
    void createLiveStream(const QString& title, const QString& description = QString());
    void deleteLiveStream(const QString& streamId);
    void getMyLiveBroadcasts(const QString& broadcastStatus = QString(),
                             const QString& pageToken = QString());
    void getLiveBroadcastById(const QString& broadcastId);
    void createLiveBroadcast(const QString& title, const QString& privacyStatus = "public",
                             const QString& latency = "normal", bool autoStart = false,
                             bool autoStop = false, bool dvr = true, bool scheduleLater = false);
    void bindLiveBroadcast(const QString& broadcastId, const QString& streamId);
    void transitionLiveBroadcast(const QString& broadcastId, const QString& status);
    void startBroadcast(const QString& broadcastId);
    void startBroadcast(const QString& broadcastId, const QString& boundStreamId);
    void stopBroadcast(const QString& broadcastId);
    void resetBroadcast(const QString& broadcastId);

    // Configuration
    void setApiKey(const QString& apiKey);
    void setTimeout(int timeoutMs);
    void retryLastRequest();

   signals:
    void myLiveStreamsReceived(const YouTubeLiveStreamListResponse& response);
    void liveStreamReceived(const YouTubeLiveStream& stream);
    void liveStreamCreated(const YouTubeLiveStream& stream);
    void liveStreamDeleted(const QString& streamId);
    void myLiveBroadcastsReceived(const YouTubeLiveBroadcastListResponse& response);
    void liveBroadcastReceived(const YouTubeLiveBroadcast& broadcast);
    void liveBroadcastCreated(const QString& broadcastId);
    void liveBroadcastBound(const QString& broadcastId, const QString& streamId);
    void liveBroadcastTransitioned(const QString& broadcastId, const QString& status);
    void errorOccurred(const QString& error, const QString& operation);
    void requestCompleted(const QString& operation);

   private slots:
    void onApiRequestFinished(const QString& response, const QString& error);
    void onApiRequestError(const QString& response, const QString& error);

   private:
    void beginWaitStreamActiveAndTransition(const QString& broadcastId, const QString& streamId);
    void makeApiRequest(const QString& endpoint, const QString& method = "GET",
                        const QString& body = QString());
    QString m_lastBroadcastId;
    QString m_lastStreamId;
    QString m_lastTransitionStatus;
    QTimer* m_streamActivePollTimer{nullptr};
    bool m_waitingStreamActiveForStart{false};
    QString m_pendingStartBroadcastId;
    QString m_boundStreamIdForStart;
    int m_streamActivePollAttempts{0};
    QString m_boundStreamIdOverrideForStart;
    QString buildApiUrl(const QString& endpoint, const QMap<QString, QString>& params) const;

    // JSON parsing
    YouTubeLiveStream parseLiveStream(const nlohmann::json& json) const;
    YouTubeLiveStreamSnippet parseSnippet(const nlohmann::json& json) const;
    YouTubeLiveStreamCdn parseCdn(const nlohmann::json& json) const;
    YouTubeLiveStreamIngestionInfo parseIngestionInfo(const nlohmann::json& json) const;
    YouTubeLiveStreamStatus parseStatus(const nlohmann::json& json) const;
    YouTubeLiveStreamHealthStatus parseHealthStatus(const nlohmann::json& json) const;
    YouTubeLiveStreamContentDetails parseContentDetails(const nlohmann::json& json) const;
    YouTubeLiveStreamListResponse parseLiveStreamListResponse(const nlohmann::json& json) const;
    YouTubeLiveBroadcastSnippet parseLiveBroadcastSnippet(const nlohmann::json& json) const;
    YouTubeLiveBroadcastStatus parseLiveBroadcastStatus(const nlohmann::json& json) const;
    YouTubeLiveBroadcastContentDetails parseLiveBroadcastContentDetails(
        const nlohmann::json& json) const;
    YouTubeLiveBroadcast parseLiveBroadcast(const nlohmann::json& json) const;
    YouTubeLiveBroadcastListResponse parseLiveBroadcastListResponse(
        const nlohmann::json& json) const;

    // Error handling
    void handleApiError(const QString& error, const QString& operation, int httpStatus);

    // Constants
    static const QString YOUTUBE_API_BASE_URL;
    static const QString YOUTUBE_API_VERSION;

    // State
    QString m_accessToken;
    QString m_apiKey;
    int m_timeoutMs = 30000;  // Default timeout
    bool m_hasValidAuth = false;
    bool m_retryPending = false;

    // Request context
    QString m_currentOperation;
    QString m_lastEndpoint;
    QString m_lastMethod;
    QString m_lastBody;
    nlohmann::json m_tempBroadcastJson;
};
