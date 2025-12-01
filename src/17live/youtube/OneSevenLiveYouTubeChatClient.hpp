#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>
#include <nlohmann/json.hpp>

#include "OneSevenLiveYouTubeClient.hpp"

// Forward declarations
class RemoteTextThread;
class OneSevenLiveYouTubeClient;

struct YouTubeChatMessageSnippet {
    QString type;
    QString liveChatId;
    QString authorChannelId;
    QString publishedAt;
    QString displayMessage;
    QString textMessageDetails;
    QString messageId;

    YouTubeChatMessageSnippet() : type("textMessageEvent") {}
};

struct YouTubeChatAuthorDetails {
    QString channelId;
    QString displayName;
    QString profileImageUrl;
    bool isVerified;
    bool isChatOwner;
    bool isChatSponsor;
    bool isChatModerator;

    YouTubeChatAuthorDetails()
        : isVerified(false), isChatOwner(false), isChatSponsor(false), isChatModerator(false) {}
};

struct YouTubeChatMessage {
    QString kind;
    QString etag;
    QString id;
    YouTubeChatMessageSnippet snippet;
    YouTubeChatAuthorDetails authorDetails;

    YouTubeChatMessage() : kind("youtube#liveChatMessage") {}
};

struct YouTubeChatMessageListResponse {
    QString kind;
    QString etag;
    QString nextPageToken;
    int pollingIntervalMillis;
    int totalResults;
    QVector<YouTubeChatMessage> items;

    YouTubeChatMessageListResponse()
        : kind("youtube#liveChatMessageListResponse"),
          pollingIntervalMillis(5000),
          totalResults(0) {}
};

class OneSevenLiveYouTubeChatClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveYouTubeChatClient(QObject* parent = nullptr);
    ~OneSevenLiveYouTubeChatClient();

    // Authentication
    void setAccessToken(const QString& accessToken);
    bool hasValidAuth() const;

    // Chat Methods
    void startChatPolling(const QString& liveChatId);
    void stopChatPolling();
    bool isPolling() const;

    // Configuration
    void setApiKey(const QString& apiKey);
    void setTimeout(int timeoutMs);
    void setMaxRetries(int maxRetries);
    void setRetryDelay(int baseDelayMs);
    void setApiClient(OneSevenLiveYouTubeClient* apiClient);
    void startDiscovery();
    void stopDiscovery();

   signals:
    void chatMessagesReceived(const YouTubeChatMessageListResponse& response);
    void newChatMessage(const YouTubeChatMessage& message);
    void pollingStarted(const QString& liveChatId);
    void pollingStopped();
    void errorOccurred(const QString& error, const QString& operation);
    void rateLimitHit(int retryAfterMs);

   private slots:
    void onChatRequestFinished(const QString& response, const QString& error);
    void onPollingTimeout();
    void onStatusTimer();
    void doReconnect();
    void onBroadcastsReceived(const YouTubeLiveBroadcastListResponse& resp);

   private:
    void fetchChatMessages();
    void scheduleNextPoll(int intervalMs);
    void scheduleReconnect();
    void handleRateLimit(int retryAfterMs);
    void handleApiError(const QString& error, const QString& operation, int httpStatus);

    // JSON parsing
    YouTubeChatMessage parseChatMessage(const nlohmann::json& json) const;
    YouTubeChatMessageSnippet parseMessageSnippet(const nlohmann::json& json) const;
    YouTubeChatAuthorDetails parseAuthorDetails(const nlohmann::json& json) const;
    YouTubeChatMessageListResponse parseChatMessageListResponse(const nlohmann::json& json) const;

    // Request building
    QString buildChatMessagesUrl(const QString& liveChatId,
                                 const QString& pageToken = QString()) const;
    void makeChatRequest(const QString& endpoint);

    // Constants
    static const QString YOUTUBE_API_BASE_URL;
    static const QString YOUTUBE_API_VERSION;
    static const int DEFAULT_POLLING_INTERVAL;
    static const int MAX_EXPONENTIAL_BACKOFF_DELAY;
    static const int STATUS_BROADCAST_INTERVAL;
    static const int MAX_QUICK_RETRIES;
    static const int LONG_RETRY_DELAY;
    static const int MAX_NO_MESSAGE_QUICK_POLLS;

    // State
    QString m_accessToken;
    QString m_apiKey;
    QString m_liveChatId;
    QString m_nextPageToken;
    OneSevenLiveYouTubeClient* m_apiClient{nullptr};
    int m_timeoutMs = 0;
    int m_maxRetries = 0;
    int m_retryDelayMs = 0;
    int m_currentRetryCount = 0;
    int m_reconnectAttempts = 0;
    int m_noMessageStreak = 0;
    bool m_hasValidAuth = false;
    bool m_isPolling = false;
    bool m_isRateLimited = false;

    // Polling
    int m_currentPollingInterval = 0;
    int m_exponentialBackoffDelay = 0;

    // Request context
    QString m_currentOperation;
    QString m_lastEndpoint;

    // Timer for polling
    class QTimer* m_pollingTimer = nullptr;
    class QTimer* m_statusTimer = nullptr;
    class QTimer* m_reconnectTimer = nullptr;
    class QTimer* m_discoverTimer{nullptr};
};
