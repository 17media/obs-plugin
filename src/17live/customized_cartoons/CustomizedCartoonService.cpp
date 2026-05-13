#include "CustomizedCartoonService.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QUuid>

#include <chrono>
#include <future>

#include "../OneSevenLiveConfigManager.hpp"
#include "../api/OneSevenLiveApiWrappers.hpp"
#include "../streaming/OneSevenLiveStreamManager.hpp"
#include "../utility/Common.hpp"

using json = nlohmann::json;

CustomizedCartoonService::CustomizedCartoonService(QMainWindow* mainWindow,
                                                   OneSevenLiveApiWrappers* apiWrapper,
                                                   OneSevenLiveConfigManager* configManager,
                                                   OneSevenLiveStreamManager* streamManager,
                                                   QObject* parent)
    : QObject(parent),
      mainWindow_(mainWindow),
      apiWrapper_(apiWrapper),
      configManager_(configManager),
      streamManager_(streamManager) {
    pollTimer_.setInterval(10 * 1000);
    pollTimer_.setSingleShot(false);
    connect(&pollTimer_, &QTimer::timeout, this, &CustomizedCartoonService::onPollTimer);

    playbackTimer_.setInterval(200);
    playbackTimer_.setSingleShot(false);
    connect(&playbackTimer_, &QTimer::timeout, this, &CustomizedCartoonService::checkVideoState);

    if (streamManager_) {
        connect(streamManager_, &OneSevenLiveStreamManager::streamStatusChanged, this,
                &CustomizedCartoonService::onStreamStatusChanged);
    }

    ensureStorageDir();
    reloadConfig();
}

void CustomizedCartoonService::reloadConfig() {
    if (!configManager_) {
        return;
    }
    const json cfg = configManager_->getCustomizedCartoonsConfig();
    {
        std::lock_guard<std::mutex> lock(cfgMutex_);
        cfg_ = cfg;
    }
    media_ = parseMedia(cfg);
    rules_ = parseRules(cfg);
    emit configChanged();
}

json CustomizedCartoonService::getConfigSnapshot() const {
    std::lock_guard<std::mutex> lock(cfgMutex_);
    return cfg_;
}

bool CustomizedCartoonService::saveConfig(const json& cfg) {
    if (!configManager_) {
        return false;
    }
    if (!configManager_->setCustomizedCartoonsConfig(cfg)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(cfgMutex_);
        cfg_ = cfg;
    }
    media_ = parseMedia(cfg);
    rules_ = parseRules(cfg);
    emit configChanged();
    return true;
}

QString CustomizedCartoonService::storageDir() const {
    return QDir::homePath() + "/.17Live/customized_cartoons";
}

void CustomizedCartoonService::ensureStorageDir() {
    QDir dir(storageDir());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString CustomizedCartoonService::inferMediaType(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    const QStringList videoExts = {"mp4", "mov", "m4v", "mkv", "webm", "avi"};
    if (videoExts.contains(ext)) {
        return "video";
    }
    return "image";
}

QString CustomizedCartoonService::copyToStorage(const QString& srcPath, QString& outError) const {
    outError.clear();
    QFileInfo fi(srcPath);
    if (!fi.exists() || !fi.isFile()) {
        outError = obs_module_text("CustomizedCartoon.Error.FileNotFound");
        return QString();
    }

    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString destName = uuid + "_" + fi.fileName();
    const QString destPath = storageDir() + "/" + destName;

    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }
    if (!QFile::copy(srcPath, destPath)) {
        outError = obs_module_text("CustomizedCartoon.Error.CopyFailed");
        return QString();
    }
    return destPath;
}

bool CustomizedCartoonService::importMediaFile(const QString& filePath, QString& outMediaId,
                                               QString& outError) {
    outMediaId.clear();
    outError.clear();

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        outError = obs_module_text("CustomizedCartoon.Error.FileNotFound");
        return false;
    }

    const qint64 maxSize = 200LL * 1024LL * 1024LL;
    if (fi.size() > maxSize) {
        outError = obs_module_text("CustomizedCartoon.Error.FileTooLarge");
        return false;
    }

    ensureStorageDir();
    const QString destPath = copyToStorage(filePath, outError);
    if (destPath.isEmpty()) {
        return false;
    }

    const QString mediaId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString type = inferMediaType(destPath);

    if (type == "video" && obs_source_get_display_name("ffmpeg_source")) {
        const std::string pathStd = destPath.toStdString();
        const std::string probeName =
            "17LiveCartoonDurationProbe_" + std::to_string(QDateTime::currentMSecsSinceEpoch());

        auto prom = std::make_shared<std::promise<int64_t>>();
        auto fut = prom->get_future();

        ScheduleOBSTask([pathStd, probeName, prom]() mutable {
            int64_t durationMs = 0;
            ObsDataPtr settings(obs_data_create());
            obs_data_set_bool(settings.get(), "looping", false);
            obs_data_set_bool(settings.get(), "restart_on_activate", true);
            obs_data_set_bool(settings.get(), "close_when_inactive", true);
            obs_data_set_string(settings.get(), "local_file", pathStd.c_str());

            obs_source_t* probe = obs_source_create("ffmpeg_source", probeName.c_str(), settings.get(), nullptr);
            if (probe) {
                obs_source_media_restart(probe);
                for (int i = 0; i < 40; i++) {
                    durationMs = obs_source_media_get_duration(probe);
                    if (durationMs > 0) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                obs_source_release(probe);
            }
            prom->set_value(durationMs);
        });

        if (fut.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
            const int64_t durationMs = fut.get();
            if (durationMs > 0 && durationMs > 15 * 1000) {
                QFile::remove(destPath);
                outError = obs_module_text("CustomizedCartoon.Error.VideoTooLong");
                return false;
            }
        }
    }

    json cfg = getConfigSnapshot();
    if (!cfg.contains("media") || !cfg["media"].is_array()) {
        cfg["media"] = json::array();
    }
    cfg["media"].push_back({{"id", mediaId.toStdString()},
                            {"name", fi.fileName().toStdString()},
                            {"path", destPath.toStdString()},
                            {"type", type.toStdString()},
                            {"displaySec", 5}});

    if (!saveConfig(cfg)) {
        outError = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
        return false;
    }

    outMediaId = mediaId;
    return true;
}

bool CustomizedCartoonService::deleteMedia(const QString& mediaId, QString& outError) {
    outError.clear();
    json cfg = getConfigSnapshot();
    if (!cfg.contains("media") || !cfg["media"].is_array()) {
        return true;
    }

    json newMedia = json::array();
    QString pathToDelete;

    for (const auto& it : cfg["media"]) {
        if (!it.is_object() || !it.contains("id") || !it["id"].is_string()) {
            continue;
        }
        const QString id = QString::fromStdString(it["id"].get<std::string>());
        if (id == mediaId) {
            if (it.contains("path") && it["path"].is_string()) {
                pathToDelete = QString::fromStdString(it["path"].get<std::string>());
            }
            continue;
        }
        newMedia.push_back(it);
    }

    cfg["media"] = std::move(newMedia);
    if (cfg.contains("rules") && cfg["rules"].is_array()) {
        for (auto& r : cfg["rules"]) {
            if (r.is_object() && r.contains("mediaId") && r["mediaId"].is_string() &&
                QString::fromStdString(r["mediaId"].get<std::string>()) == mediaId) {
                r["mediaId"] = "";
            }
        }
    }

    if (!saveConfig(cfg)) {
        outError = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
        return false;
    }

    if (!pathToDelete.isEmpty() && QFile::exists(pathToDelete)) {
        QFile::remove(pathToDelete);
    }

    return true;
}

CustomizedCartoonService::MediaItem* CustomizedCartoonService::findMediaById(const QString& id) {
    for (auto& m : media_) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

CustomizedCartoonService::RuleItem* CustomizedCartoonService::findRuleById(const QString& id) {
    for (auto& r : rules_) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

std::vector<CustomizedCartoonService::MediaItem> CustomizedCartoonService::parseMedia(
    const json& cfg) const {
    std::vector<MediaItem> out;
    if (!cfg.contains("media") || !cfg["media"].is_array()) {
        return out;
    }
    for (const auto& it : cfg["media"]) {
        if (!it.is_object())
            continue;
        MediaItem m;
        if (it.contains("id") && it["id"].is_string())
            m.id = QString::fromStdString(it["id"].get<std::string>());
        if (it.contains("name") && it["name"].is_string())
            m.name = QString::fromStdString(it["name"].get<std::string>());
        if (it.contains("path") && it["path"].is_string())
            m.path = QString::fromStdString(it["path"].get<std::string>());
        if (it.contains("type") && it["type"].is_string())
            m.type = QString::fromStdString(it["type"].get<std::string>());
        if (it.contains("displaySec") && it["displaySec"].is_number_integer())
            m.displaySec = it["displaySec"].get<int>();
        if (!m.id.isEmpty() && !m.path.isEmpty()) {
            out.push_back(std::move(m));
        }
    }
    return out;
}

std::vector<CustomizedCartoonService::RuleItem> CustomizedCartoonService::parseRules(
    const json& cfg) const {
    std::vector<RuleItem> out;
    if (!cfg.contains("rules") || !cfg["rules"].is_array()) {
        return out;
    }
    for (const auto& it : cfg["rules"]) {
        if (!it.is_object())
            continue;
        RuleItem r;
        if (it.contains("id") && it["id"].is_string())
            r.id = QString::fromStdString(it["id"].get<std::string>());
        if (it.contains("name") && it["name"].is_string())
            r.name = QString::fromStdString(it["name"].get<std::string>());
        if (it.contains("mediaId") && it["mediaId"].is_string())
            r.mediaId = QString::fromStdString(it["mediaId"].get<std::string>());
        if (it.contains("engageType") && it["engageType"].is_string())
            r.engageType = QString::fromStdString(it["engageType"].get<std::string>());
        if (it.contains("points") && it["points"].is_number_integer())
            r.points = it["points"].get<int>();
        if (it.contains("count") && it["count"].is_number_integer())
            r.count = it["count"].get<int>();
        if (it.contains("repeatable") && it["repeatable"].is_boolean())
            r.repeatable = it["repeatable"].get<bool>();
        if (it.contains("enabled") && it["enabled"].is_boolean())
            r.enabled = it["enabled"].get<bool>();
        if (!r.id.isEmpty()) {
            out.push_back(std::move(r));
        }
    }
    return out;
}

void CustomizedCartoonService::onStreamStatusChanged(OneSevenLiveStreamingStatus status) {
    if (!streamManager_) {
        return;
    }
    if (status == OneSevenLiveStreamingStatus::Streaming) {
        liveStreamID_ = QString::fromStdString(streamManager_->getCurrentLiveStreamID());
        startEngagementsIfNeeded();
        pollTimer_.start();
        onPollTimer();
    } else if (status == OneSevenLiveStreamingStatus::NotStarted) {
        pollTimer_.stop();
        stopEngagements();
        stopPlayback();
        liveStreamID_.clear();
    }
}

void CustomizedCartoonService::startEngagementsIfNeeded() {
    if (!apiWrapper_ || !streamManager_) {
        return;
    }
    if (liveStreamID_.isEmpty()) {
        return;
    }

    std::vector<OneSevenLiveEngagementCreate> creates;
    std::vector<QString> ruleIds;
    creates.reserve(rules_.size());

    for (const auto& r : rules_) {
        if (!r.enabled) {
            continue;
        }
        if (r.mediaId.isEmpty()) {
            continue;
        }
        OneSevenLiveEngagementCreate c;
        if (r.engageType == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE") {
            c.engageType = OneSevenLiveEngagementType::GiftLuckybagFirstPrizeMilestone;
            c.payload = json{{"count", r.count}};
        } else {
            c.engageType = OneSevenLiveEngagementType::GiftAmountMilestone;
            c.payload = json{{"points", r.points}, {"count", r.count}};
        }
        c.isRepeatable = r.repeatable;
        creates.push_back(std::move(c));
        ruleIds.push_back(r.id);
    }

    if (creates.empty()) {
        return;
    }

    const std::string lid = liveStreamID_.toStdString();
    QPointer<CustomizedCartoonService> self = this;

    ScheduleOBSTask([self, lid, creates = std::move(creates), ruleIds = std::move(ruleIds)]() {
        if (!self || !self->apiWrapper_) {
            return;
        }
        std::vector<OneSevenLiveEngagementCreateResult> results;
        const bool ok = self->apiWrapper_->CreateLiveEngagements(lid, creates, results);
        QMetaObject::invokeMethod(
            self,
            [self, ok, results = std::move(results), ruleIds = std::move(ruleIds)]() mutable {
                if (!self) {
                    return;
                }
                if (!ok) {
                    return;
                }
                for (const auto& r : results) {
                    if (r.index < 0 || r.index >= static_cast<int>(ruleIds.size())) {
                        continue;
                    }
                    if (r.engageID.isEmpty()) {
                        continue;
                    }
                    self->ruleToEngageID_[ruleIds[r.index]] = r.engageID;
                }
            },
            Qt::QueuedConnection);
    });
}

void CustomizedCartoonService::stopEngagements() {
    if (!apiWrapper_ || liveStreamID_.isEmpty()) {
        ruleToEngageID_.clear();
        engageProgress_.clear();
        return;
    }
    std::vector<std::string> engageIDs;
    engageIDs.reserve(ruleToEngageID_.size());
    for (const auto& it : ruleToEngageID_) {
        engageIDs.push_back(it.second.toStdString());
    }
    ruleToEngageID_.clear();
    engageProgress_.clear();

    if (engageIDs.empty()) {
        return;
    }

    const std::string lid = liveStreamID_.toStdString();
    QPointer<CustomizedCartoonService> self = this;
    ScheduleOBSTask([self, lid, engageIDs = std::move(engageIDs)]() {
        if (!self || !self->apiWrapper_) {
            return;
        }
        self->apiWrapper_->DeleteLiveEngagements(lid, engageIDs);
    });
}

void CustomizedCartoonService::onPollTimer() {
    pollProgressAsync();
}

void CustomizedCartoonService::pollProgressAsync() {
    if (!apiWrapper_ || liveStreamID_.isEmpty()) {
        return;
    }
    if (ruleToEngageID_.empty()) {
        return;
    }
    const std::string lid = liveStreamID_.toStdString();
    QPointer<CustomizedCartoonService> self = this;
    std::map<QString, QString> ruleToEngage = ruleToEngageID_;

    ScheduleOBSTask([self, lid, ruleToEngage = std::move(ruleToEngage)]() mutable {
        if (!self || !self->apiWrapper_) {
            return;
        }
        std::vector<OneSevenLiveEngagementProgress> progress;
        const bool ok = self->apiWrapper_->GetLiveEngagementProgress(lid, progress);
        QMetaObject::invokeMethod(
            self,
            [self, ok, progress = std::move(progress), ruleToEngage = std::move(ruleToEngage)]() {
                if (!self) {
                    return;
                }
                if (!ok) {
                    return;
                }

                std::map<QString, QString> engageToRule;
                for (const auto& it : ruleToEngage) {
                    engageToRule[it.second] = it.first;
                }

                for (const auto& p : progress) {
                    if (p.engageID.isEmpty()) {
                        continue;
                    }
                    auto& state = self->engageProgress_[p.engageID];
                    const bool prevBelowTarget =
                        state.target > 0 ? (state.current < state.target) : true;
                    const bool nowAtOrAboveTarget = p.target > 0 ? (p.current >= p.target) : false;
                    const bool roundIncreased = p.round > state.round;
                    const bool firstReached = prevBelowTarget && nowAtOrAboveTarget;

                    state.current = p.current;
                    state.target = p.target;
                    state.round = p.round;

                    bool completedNow = roundIncreased || firstReached;
                    if (completedNow && state.lastTriggeredRound != p.round) {
                        state.lastTriggeredRound = p.round;
                        const auto it = engageToRule.find(p.engageID);
                        if (it != engageToRule.end()) {
                            RuleItem* rule = self->findRuleById(it->second);
                            if (rule && rule->enabled) {
                                self->enqueuePlayMedia(rule->mediaId);
                            }
                        }
                    }
                }

                emit self->progressUpdated();
            },
            Qt::QueuedConnection);
    });
}

void CustomizedCartoonService::enqueuePlayMedia(const QString& mediaId) {
    if (mediaId.isEmpty()) {
        return;
    }
    playQueue_.push_back(mediaId);
    if (!playing_) {
        startNextPlayback();
    }
}

void CustomizedCartoonService::previewPlayAll() {
    for (const auto& r : rules_) {
        if (!r.enabled || r.mediaId.isEmpty()) {
            continue;
        }
        playQueue_.push_back(r.mediaId);
    }
    if (!playing_) {
        startNextPlayback();
    }
}

bool CustomizedCartoonService::startPositionPreview(const QString& mediaId, bool landscape,
                                                    QString& outError) {
    outError.clear();
    MediaItem* media = findMediaById(mediaId);
    if (!media) {
        outError = obs_module_text("CustomizedCartoon.Error.MediaNotFound");
        return false;
    }
    if (!QFile::exists(media->path)) {
        outError = obs_module_text("CustomizedCartoon.Error.FileNotFound");
        return false;
    }

    stopPlayback();
    positionPreviewing_ = true;
    positionPreviewLandscape_ = landscape;
    positionPreviewIsMedia_ = (media->type == "video");

    ensureOverlaySources();
    ensureOverlaySceneItem();
    applyOverlayTransform(landscape);

    if (media->type == "video") {
        if (!mediaSource_) {
            outError = obs_module_text("CustomizedCartoon.Error.VideoSourceNotAvailable");
            return false;
        }
        setMediaLooping(true);
        obs_data_t* settings = obs_source_get_settings(mediaSource_);
        if (settings) {
            obs_data_set_string(settings, "local_file", media->path.toStdString().c_str());
            obs_source_update(mediaSource_, settings);
            obs_data_release(settings);
        }
        showOverlaySource(true);
        obs_source_media_restart(mediaSource_);
        playbackTimer_.stop();
        return true;
    }

    if (!imageSource_) {
        outError = obs_module_text("CustomizedCartoon.Error.ImageSourceNotAvailable");
        return false;
    }
    obs_data_t* settings = obs_source_get_settings(imageSource_);
    if (settings) {
        obs_data_set_string(settings, "file", media->path.toStdString().c_str());
        obs_source_update(imageSource_, settings);
        obs_data_release(settings);
    }
    showOverlaySource(false);
    playbackTimer_.stop();
    return true;
}

void CustomizedCartoonService::stopPositionPreview() {
    if (!positionPreviewing_) {
        return;
    }
    positionPreviewing_ = false;
    setMediaLooping(false);
    playbackTimer_.stop();
    hideOverlaySources();
}

bool CustomizedCartoonService::getCurrentOverlayTransform(json& outTransform, QString& outError) const {
    outError.clear();
    outTransform = json::object();

    obs_sceneitem_t* item = nullptr;
    if (positionPreviewIsMedia_) {
        item = mediaItem_;
    } else {
        item = imageItem_;
    }
    if (!item) {
        item = mediaItem_ ? mediaItem_ : imageItem_;
    }
    if (!item) {
        outError = obs_module_text("CustomizedCartoon.Error.OverlayNotAvailable");
        return false;
    }

    obs_transform_info ti{};
    obs_sceneitem_get_info2(item, &ti);
    outTransform["x"] = ti.pos.x;
    outTransform["y"] = ti.pos.y;
    outTransform["scaleX"] = ti.scale.x;
    outTransform["scaleY"] = ti.scale.y;
    outTransform["rot"] = ti.rot;
    outTransform["alignment"] = ti.alignment;
    outTransform["boundsType"] = (int)ti.bounds_type;
    outTransform["boundsAlignment"] = ti.bounds_alignment;
    outTransform["boundsW"] = ti.bounds.x;
    outTransform["boundsH"] = ti.bounds.y;
    outTransform["cropToBounds"] = ti.crop_to_bounds;
    return true;
}

void CustomizedCartoonService::applyOverlayTransformForOrientation(bool landscape) {
    applyOverlayTransform(landscape);
}

std::vector<OneSevenLiveEngagementProgress> CustomizedCartoonService::getProgressSnapshot() const {
    std::vector<OneSevenLiveEngagementProgress> out;
    out.reserve(engageProgress_.size());
    for (const auto& it : engageProgress_) {
        OneSevenLiveEngagementProgress p;
        p.engageID = it.first;
        p.current = it.second.current;
        p.target = it.second.target;
        p.round = it.second.round;
        out.push_back(std::move(p));
    }
    return out;
}

void CustomizedCartoonService::startNextPlayback() {
    if (positionPreviewing_) {
        stopPositionPreview();
    }
    if (playing_) {
        return;
    }
    while (!playQueue_.empty()) {
        const QString mediaId = playQueue_.front();
        playQueue_.pop_front();
        MediaItem* media = findMediaById(mediaId);
        if (!media) {
            continue;
        }
        if (!QFile::exists(media->path)) {
            continue;
        }

        ensureOverlaySources();
        ensureOverlaySceneItem();

        const bool landscape = streamManager_ ? streamManager_->getRoomInfo().landscape : true;
        applyOverlayTransform(landscape);

        playing_ = true;
        playingMediaId_ = mediaId;
        if (media->type == "video") {
            playVideo(*media);
        } else {
            playImage(*media);
        }
        return;
    }
}

void CustomizedCartoonService::stopPlayback() {
    playbackTimer_.stop();
    playing_ = false;
    playingMediaId_.clear();
    playQueue_.clear();
    hideOverlaySources();
}

void CustomizedCartoonService::ensureOverlaySources() {
    if (!mediaSource_) {
        if (obs_source_get_display_name("ffmpeg_source")) {
            obs_data_t* settings = obs_data_create();
            obs_data_set_bool(settings, "looping", false);
            obs_data_set_bool(settings, "restart_on_activate", true);
            obs_data_set_bool(settings, "close_when_inactive", true);
            mediaSource_ =
                obs_source_create("ffmpeg_source", "17LiveCustomizedCartoonMedia", settings, nullptr);
            obs_data_release(settings);
        }
    }
    if (!imageSource_) {
        if (obs_source_get_display_name("image_source")) {
            obs_data_t* settings = obs_data_create();
            imageSource_ =
                obs_source_create("image_source", "17LiveCustomizedCartoonImage", settings, nullptr);
            obs_data_release(settings);
        }
    }
}

void CustomizedCartoonService::ensureOverlaySceneItem() {
    obs_source_t* sceneSource = obs_frontend_get_current_scene();
    if (!sceneSource) {
        return;
    }
    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    if (!scene) {
        obs_source_release(sceneSource);
        return;
    }

    if (mediaSource_ && !mediaItem_) {
        mediaItem_ = obs_scene_find_source(scene, obs_source_get_name(mediaSource_));
        if (!mediaItem_) {
            mediaItem_ = obs_scene_add(scene, mediaSource_);
        }
        if (mediaItem_) {
            obs_sceneitem_set_order(mediaItem_, OBS_ORDER_MOVE_TOP);
            obs_sceneitem_set_visible(mediaItem_, false);
        }
    }
    if (imageSource_ && !imageItem_) {
        imageItem_ = obs_scene_find_source(scene, obs_source_get_name(imageSource_));
        if (!imageItem_) {
            imageItem_ = obs_scene_add(scene, imageSource_);
        }
        if (imageItem_) {
            obs_sceneitem_set_order(imageItem_, OBS_ORDER_MOVE_TOP);
            obs_sceneitem_set_visible(imageItem_, false);
        }
    }

    obs_source_release(sceneSource);
}

void CustomizedCartoonService::applyOverlayTransform(bool landscape) {
    json cfg = getConfigSnapshot();
    if (!cfg.contains("position") || !cfg["position"].is_object()) {
        return;
    }
    const char* key = landscape ? "landscape" : "portrait";
    if (!cfg["position"].contains(key) || !cfg["position"][key].is_object()) {
        return;
    }
    const auto& t = cfg["position"][key];

    obs_transform_info ti{};
    ti.pos.x = t.value("x", 0.0);
    ti.pos.y = t.value("y", 0.0);
    ti.scale.x = t.value("scaleX", 1.0);
    ti.scale.y = t.value("scaleY", 1.0);
    ti.rot = t.value("rot", 0.0);
    ti.alignment = t.value("alignment", static_cast<uint32_t>(OBS_ALIGN_CENTER));
    ti.bounds_type = static_cast<obs_bounds_type>(t.value("boundsType", (int)OBS_BOUNDS_NONE));
    ti.bounds_alignment = t.value("boundsAlignment", static_cast<uint32_t>(OBS_ALIGN_CENTER));
    ti.bounds.x = t.value("boundsW", 0.0);
    ti.bounds.y = t.value("boundsH", 0.0);
    ti.crop_to_bounds = t.value("cropToBounds", false);

    if (mediaItem_) {
        obs_sceneitem_set_info2(mediaItem_, &ti);
    }
    if (imageItem_) {
        obs_sceneitem_set_info2(imageItem_, &ti);
    }
}

void CustomizedCartoonService::showOverlaySource(bool media) {
    if (mediaItem_) {
        obs_sceneitem_set_visible(mediaItem_, media);
        obs_sceneitem_set_order(mediaItem_, OBS_ORDER_MOVE_TOP);
    }
    if (imageItem_) {
        obs_sceneitem_set_visible(imageItem_, !media);
        obs_sceneitem_set_order(imageItem_, OBS_ORDER_MOVE_TOP);
    }
}

void CustomizedCartoonService::hideOverlaySources() {
    if (mediaItem_) {
        obs_sceneitem_set_visible(mediaItem_, false);
    }
    if (imageItem_) {
        obs_sceneitem_set_visible(imageItem_, false);
    }
}

void CustomizedCartoonService::setMediaLooping(bool looping) {
    if (!mediaSource_) {
        return;
    }
    obs_data_t* settings = obs_source_get_settings(mediaSource_);
    if (settings) {
        obs_data_set_bool(settings, "looping", looping);
        obs_source_update(mediaSource_, settings);
        obs_data_release(settings);
    }
}

void CustomizedCartoonService::playVideo(const MediaItem& media) {
    if (!mediaSource_) {
        playing_ = false;
        startNextPlayback();
        return;
    }
    setMediaLooping(false);
    obs_data_t* settings = obs_source_get_settings(mediaSource_);
    if (settings) {
        obs_data_set_string(settings, "local_file", media.path.toStdString().c_str());
        obs_source_update(mediaSource_, settings);
        obs_data_release(settings);
    }

    showOverlaySource(true);
    obs_source_media_restart(mediaSource_);
    playbackTimer_.start();
}

void CustomizedCartoonService::playImage(const MediaItem& media) {
    if (!imageSource_) {
        playing_ = false;
        startNextPlayback();
        return;
    }
    obs_data_t* settings = obs_source_get_settings(imageSource_);
    if (settings) {
        obs_data_set_string(settings, "file", media.path.toStdString().c_str());
        obs_source_update(imageSource_, settings);
        obs_data_release(settings);
    }

    showOverlaySource(false);

    const int sec = std::max(1, media.displaySec);
    QPointer<CustomizedCartoonService> self = this;
    QTimer::singleShot(sec * 1000, this, [self]() {
        if (!self) {
            return;
        }
        self->hideOverlaySources();
        self->playing_ = false;
        self->playingMediaId_.clear();
        self->startNextPlayback();
    });
}

void CustomizedCartoonService::checkVideoState() {
    if (!playing_ || !mediaSource_) {
        playbackTimer_.stop();
        return;
    }
    const obs_media_state s = obs_source_media_get_state(mediaSource_);
    if (s == OBS_MEDIA_STATE_ENDED || s == OBS_MEDIA_STATE_STOPPED || s == OBS_MEDIA_STATE_ERROR) {
        playbackTimer_.stop();
        hideOverlaySources();
        playing_ = false;
        playingMediaId_.clear();
        startNextPlayback();
    }
}

json CustomizedCartoonService::serialize(const std::vector<MediaItem>& media,
                                        const std::vector<RuleItem>& rules) const {
    json cfg = getConfigSnapshot();
    cfg["media"] = json::array();
    for (const auto& m : media) {
        cfg["media"].push_back({{"id", m.id.toStdString()},
                                {"name", m.name.toStdString()},
                                {"path", m.path.toStdString()},
                                {"type", m.type.toStdString()},
                                {"displaySec", m.displaySec}});
    }
    cfg["rules"] = json::array();
    for (const auto& r : rules) {
        cfg["rules"].push_back({{"id", r.id.toStdString()},
                                {"name", r.name.toStdString()},
                                {"mediaId", r.mediaId.toStdString()},
                                {"engageType", r.engageType.toStdString()},
                                {"points", r.points},
                                {"count", r.count},
                                {"repeatable", r.repeatable},
                                {"enabled", r.enabled}});
    }
    return cfg;
}
