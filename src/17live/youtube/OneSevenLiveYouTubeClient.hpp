#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <memory>
#include <nlohmann/json.hpp>

// Forward declarations
class RemoteTextThread;

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

    // Configuration
    void setApiKey(const QString& apiKey);
    void setTimeout(int timeoutMs);

signals:
    void myLiveStreamsReceived(const YouTubeLiveStreamListResponse& response);
    void liveStreamReceived(const YouTubeLiveStream& stream);
    void liveStreamCreated(const YouTubeLiveStream& stream);
    void liveStreamDeleted(const QString& streamId);
    void errorOccurred(const QString& error, const QString& operation);
    void requestCompleted(const QString& operation);

private slots:
    void onApiRequestFinished(const QString& response, const QString& error);
    void onApiRequestError(const QString& response, const QString& error);

private:
    void makeApiRequest(const QString& endpoint, const QString& method = "GET", const QString& body = QString());
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
    
    // Error handling
    void handleApiError(const QString& error, const QString& operation, int httpStatus);
    
    // Constants
    static const QString YOUTUBE_API_BASE_URL;
    static const QString YOUTUBE_API_VERSION;
    
    // State
    QString m_accessToken;
    QString m_apiKey;
    int m_timeoutMs;
    bool m_hasValidAuth;
    
    // Request context
    QString m_currentOperation;
};
