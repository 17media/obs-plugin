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

    void reloadConfig();
    nlohmann::json getConfigSnapshot() const;
    bool saveConfig(const nlohmann::json& cfg);

    bool importMediaFile(const QString& filePath, QString& outMediaId, QString& outError);
    bool deleteMedia(const QString& mediaId, QString& outError);

    void previewPlayAll();
    bool startPositionPreview(const QString& mediaId, bool landscape, QString& outError);
    void stopPositionPreview();
    bool getCurrentOverlayTransform(nlohmann::json& outTransform, QString& outError) const;
    void applyOverlayTransformForOrientation(bool landscape);
    std::vector<OneSevenLiveEngagementProgress> getProgressSnapshot() const;

   signals:
    void configChanged();
    void progressUpdated();

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

    void ensureStorageDir();
    QString storageDir() const;
    QString copyToStorage(const QString& srcPath, QString& outError) const;
    static QString inferMediaType(const QString& path);

    void startEngagementsIfNeeded();
    void stopEngagements();
    void pollProgressAsync();

    void enqueuePlayMedia(const QString& mediaId);
    void startNextPlayback();
    void stopPlayback();
    void ensureOverlaySources();
    void ensureOverlaySceneItem();
    void applyOverlayTransform(bool landscape);
    void showOverlaySource(bool media);
    void hideOverlaySources();
    void setMediaLooping(bool looping);

    void playVideo(const MediaItem& media);
    void playImage(const MediaItem& media);
    void checkVideoState();

    MediaItem* findMediaById(const QString& id);
    RuleItem* findRuleById(const QString& id);

    std::vector<MediaItem> parseMedia(const nlohmann::json& cfg) const;
    std::vector<RuleItem> parseRules(const nlohmann::json& cfg) const;
    nlohmann::json serialize(const std::vector<MediaItem>& media,
                             const std::vector<RuleItem>& rules) const;

    QMainWindow* mainWindow_{nullptr};
    OneSevenLiveApiWrappers* apiWrapper_{nullptr};
    OneSevenLiveConfigManager* configManager_{nullptr};
    OneSevenLiveStreamManager* streamManager_{nullptr};

    mutable std::mutex cfgMutex_;
    nlohmann::json cfg_{nlohmann::json::object()};

    QTimer pollTimer_;
    QTimer playbackTimer_;

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
};
