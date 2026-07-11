#pragma once

#include <obs.h>

#include <QObject>
#include <QTimer>
#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <mutex>

#include "../api/OneSevenLiveModels.hpp"

class QMainWindow;

class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;
class OneSevenLiveStreamManager;

class CustomizedCartoonService : public QObject {
    Q_OBJECT

   public:
    explicit CustomizedCartoonService(QMainWindow* mainWindow, OneSevenLiveApiWrappers* apiWrapper,
                                      OneSevenLiveConfigManager* configManager,
                                      OneSevenLiveStreamManager* streamManager,
                                      QObject* parent = nullptr);
    ~CustomizedCartoonService() override;

    static QStringList supportedVideoExtensions();
    static QStringList supportedImageExtensions();
    static qint64 maxMediaFileSizeBytes();

    void reloadConfig();
    nlohmann::json getConfigSnapshot() const;
    bool saveConfig(const nlohmann::json& cfg, QString* outError = nullptr);

    bool prepareMediaDraftEntry(const QString& filePath, nlohmann::json& outMedia, QString& outError);
    bool importMediaFile(const QString& filePath, QString& outMediaId, QString& outError);
    bool deleteMedia(const QString& mediaId, QString& outError);

    void previewPlayAll(const nlohmann::json* previewConfig = nullptr);
    bool startMediaPreview(const QString& mediaId, bool landscape, const nlohmann::json* previewTransform,
                           const nlohmann::json* previewConfig, QString& outError);
    void stopMediaPreview();
    bool isMediaPreviewing() const;
    QString previewingMediaId() const;
    bool startPositionPreview(const QString& mediaId, bool landscape, const nlohmann::json* previewTransform,
                              const nlohmann::json* previewConfig, QString& outError);
    void stopPositionPreview();
    bool isPositionPreviewing() const;
    bool getCurrentOverlayTransform(nlohmann::json& outTransform, QString& outError) const;
    void applyOverlayTransformForOrientation(bool landscape, const nlohmann::json* previewTransform = nullptr);
    std::vector<OneSevenLiveEngagementProgress> getProgressSnapshot() const;

   signals:
    void configChanged();
    void progressUpdated();
    void previewStateChanged();

   private slots:
    void onStreamStatusChanged(OneSevenLiveStreamingStatus status);
    void onPollTimer();

   private:
    struct MediaItem {
        QString id;
        QString name;
        QString path;
        QString type;
        int displaySec{5};
        bool muted{false};
        bool preserveAspectRatio{false};
    };

    struct RuleItem {
        QString id;
        QString name;
        QString mediaId;
        QString engageType;
        int points{0};
        int count{0};
        bool repeatable{true};
        bool enabled{true};
    };

    struct ProgressState {
        int current{0};
        int target{0};
        int round{0};
        int lastTriggeredRound{-1};
    };

    struct LiveRuleSyncPlan {
        std::vector<QString> ruleIdsToDelete;
        std::vector<RuleItem> rulesToCreate;
    };

    struct PreviewVideoSettingsBackup {
        uint32_t baseW{0};
        uint32_t baseH{0};
        uint32_t outputW{0};
        uint32_t outputH{0};
        bool valid{false};
    };

    void ensureStorageDir();
    QString storageDir() const;
    QString copyToStorage(const QString& srcPath, QString& outError) const;
    static QString inferMediaType(const QString& path);

    void startEngagementsIfNeeded();
    void stopEngagements();
    void pollProgressAsync();
    bool buildLiveRuleSyncPlan(const std::vector<RuleItem>& nextRules, LiveRuleSyncPlan& plan,
                               QString& outError) const;
    bool executeLiveRuleSyncPlan(const LiveRuleSyncPlan& plan, QString& outError);
    static bool isRuleDefinitionChanged(const RuleItem& current, const RuleItem& next);

    void enqueuePlayMedia(const QString& mediaId);
    void startNextPlayback();
    void stopPlayback();
    bool hasActiveRules() const;
    bool shouldKeepOverlaySources() const;
    void syncOverlaySceneItems();
    void removeOverlaySceneItems();
    void ensureOverlaySources();
    void ensureOverlaySceneItem();
    obs_source_t* getActivePreviewSceneSource() const;
    bool applyPreviewCanvas(bool landscape, QString& outError);
    void restorePreviewCanvas();
    void applyOverlayTransform(bool landscape, const nlohmann::json* previewTransform = nullptr);
    void showOverlaySource(bool media);
    void hideOverlaySources();
    void setMediaLooping(bool looping);

    void playVideo(const MediaItem& media);
    void playImage(const MediaItem& media);
    void checkVideoState();

    MediaItem* findMediaById(const QString& id);
    const MediaItem* findMediaById(const QString& id) const;
    RuleItem* findRuleById(const QString& id);

    std::vector<MediaItem> parseMedia(const nlohmann::json& cfg) const;
    std::vector<RuleItem> parseRules(const nlohmann::json& cfg) const;
    nlohmann::json serialize(const std::vector<MediaItem>& media,
                             const std::vector<RuleItem>& rules) const;

    static void videoResetCallback(void* data, calldata_t*);
    void handleVideoReset();

    QMainWindow* mainWindow_{nullptr};
    OneSevenLiveApiWrappers* apiWrapper_{nullptr};
    OneSevenLiveConfigManager* configManager_{nullptr};
    OneSevenLiveStreamManager* streamManager_{nullptr};
    signal_handler_t* obsSignalHandler_{nullptr};

    mutable std::mutex cfgMutex_;
    nlohmann::json cfg_{nlohmann::json::object()};

    QTimer pollTimer_;
    QTimer playbackTimer_;
    QTimer previewTimer_;

    std::vector<MediaItem> media_;
    std::vector<RuleItem> rules_;

    QString liveStreamID_;
    std::map<QString, QString> ruleToEngageID_;
    std::map<QString, ProgressState> engageProgress_;

    std::deque<QString> playQueue_;
    bool playing_{false};
    QString playingMediaId_;
    int64_t playingStartMs_{0};

    obs_source_t* mediaSource_{nullptr};
    obs_source_t* imageSource_{nullptr};
    obs_sceneitem_t* mediaItem_{nullptr};
    obs_sceneitem_t* imageItem_{nullptr};

    bool positionPreviewing_{false};
    bool positionPreviewLandscape_{true};
    bool positionPreviewIsMedia_{true};
    QString positionPreviewMediaId_;
    nlohmann::json positionPreviewTransform_{nlohmann::json::object()};
    bool hasPositionPreviewTransform_{false};

    bool mediaPreviewing_{false};
    bool mediaPreviewLandscape_{true};
    bool mediaPreviewIsMedia_{true};
    QString previewMediaId_;
    MediaItem mediaPreviewSnapshot_;
    bool hasMediaPreviewSnapshot_{false};
    nlohmann::json mediaPreviewTransform_{nlohmann::json::object()};
    bool hasMediaPreviewTransform_{false};

    MediaItem positionPreviewSnapshot_;
    bool hasPositionPreviewSnapshot_{false};
    nlohmann::json playbackPreviewConfig_{nlohmann::json::object()};
    bool hasPlaybackPreviewConfig_{false};
    PreviewVideoSettingsBackup previewVideoSettingsBackup_{};
};
