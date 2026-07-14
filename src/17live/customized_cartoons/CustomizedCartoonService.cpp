#include "CustomizedCartoonService.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QUuid>

#include <algorithm>
#include <chrono>
#include <future>

#include "../OneSevenLiveConfigManager.hpp"
#include "../api/OneSevenLiveApiWrappers.hpp"
#include "../streaming/OneSevenLiveStreamManager.hpp"
#include "../utility/Common.hpp"
#include "../../plugin-support.h"

using json = nlohmann::json;

namespace {

constexpr const char* kCustomizedCartoonMediaSourceName = "17LiveCustomizedCartoonMedia";
constexpr const char* kCustomizedCartoonImageSourceName = "17LiveCustomizedCartoonImage";
constexpr uint32_t kPreviewLandscapeCanvasW = 1280;
constexpr uint32_t kPreviewLandscapeCanvasH = 720;
constexpr uint32_t kPreviewPortraitCanvasW = 720;
constexpr uint32_t kPreviewPortraitCanvasH = 1280;

static obs_transform_info DefaultOverlayTransform(bool landscape) {
    const int canvasW = landscape ? (int)kPreviewLandscapeCanvasW : (int)kPreviewPortraitCanvasW;
    const int canvasH = landscape ? (int)kPreviewLandscapeCanvasH : (int)kPreviewPortraitCanvasH;
    const int minSize = 20;
    const int normalizedWidth = std::clamp(500, minSize, canvasW);
    const int normalizedHeight = std::clamp(500, minSize, canvasH);
    const int maxX = std::max(0, canvasW - normalizedWidth);
    const int maxY = std::max(0, canvasH - normalizedHeight);
    const int normalizedX = std::clamp(200, 0, maxX);
    const int normalizedY = std::clamp(300, 0, maxY);

    obs_transform_info ti{};
    ti.pos.x = (float)normalizedX;
    ti.pos.y = (float)normalizedY;
    ti.scale.x = 1.0f;
    ti.scale.y = 1.0f;
    ti.rot = 0.0f;
    ti.alignment = (uint32_t)(OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
    ti.bounds_type = OBS_BOUNDS_STRETCH;
    ti.bounds_alignment = (uint32_t)(OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
    ti.bounds.x = (float)normalizedWidth;
    ti.bounds.y = (float)normalizedHeight;
    ti.crop_to_bounds = true;
    return ti;
}

int defaultDisplaySecForType(const QString& type) {
    return type == "video" ? 15 : 5;
}

}  // namespace

QStringList CustomizedCartoonService::supportedVideoExtensions() {
    return {"mp4", "mov", "m4v", "mkv", "webm", "avi"};
}

QStringList CustomizedCartoonService::supportedImageExtensions() {
    return {"png", "jpg", "jpeg", "gif", "bmp"};
}

qint64 CustomizedCartoonService::maxMediaFileSizeBytes() { return 200LL * 1024LL * 1024LL; }

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

    previewTimer_.setSingleShot(true);
    connect(&previewTimer_, &QTimer::timeout, this, [this]() {
        if (mediaPreviewing_) {
            stopMediaPreview();
        }
    });

    if (streamManager_) {
        connect(streamManager_, &OneSevenLiveStreamManager::streamStatusChanged, this,
                &CustomizedCartoonService::onStreamStatusChanged);
    }

    ensureStorageDir();
    reloadConfig();

    obsSignalHandler_ = obs_get_signal_handler();
    if (obsSignalHandler_) {
        signal_handler_connect(obsSignalHandler_, "video_reset", videoResetCallback, this);
    }
}

CustomizedCartoonService::~CustomizedCartoonService() {
    if (obsSignalHandler_) {
        signal_handler_disconnect(obsSignalHandler_, "video_reset", videoResetCallback, this);
        obsSignalHandler_ = nullptr;
    }
}

void CustomizedCartoonService::videoResetCallback(void* data, calldata_t*) {
    auto* self = static_cast<CustomizedCartoonService*>(data);
    if (!self) {
        return;
    }
    QMetaObject::invokeMethod(self, &CustomizedCartoonService::handleVideoReset, Qt::QueuedConnection);
}

void CustomizedCartoonService::handleVideoReset() {
    if (positionPreviewing_) {
        applyOverlayTransform(positionPreviewLandscape_,
                              hasPositionPreviewTransform_ ? &positionPreviewTransform_ : nullptr);
        return;
    }
    if (mediaPreviewing_) {
        applyOverlayTransform(mediaPreviewLandscape_,
                              hasMediaPreviewTransform_ ? &mediaPreviewTransform_ : nullptr);
        return;
    }
    if (playing_) {
        const bool landscape = streamManager_ ? streamManager_->getRoomInfo().landscape : true;
        applyOverlayTransform(landscape, nullptr);
    }
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
    syncOverlaySceneItems();
    emit configChanged();
}

json CustomizedCartoonService::getConfigSnapshot() const {
    std::lock_guard<std::mutex> lock(cfgMutex_);
    return cfg_;
}

bool CustomizedCartoonService::saveConfig(const json& cfg, QString* outError) {
    if (outError) {
        outError->clear();
    }
    if (!configManager_) {
        return false;
    }

    const bool shouldSyncLiveRules = !liveStreamID_.isEmpty();
    const json previousCfg = getConfigSnapshot();
    LiveRuleSyncPlan liveRuleSyncPlan;
    QString liveRuleSyncError;
    if (shouldSyncLiveRules &&
        !buildLiveRuleSyncPlan(parseRules(cfg), liveRuleSyncPlan, liveRuleSyncError)) {
        if (outError) {
            *outError = liveRuleSyncError;
        }
        return false;
    }

    if (!configManager_->setCustomizedCartoonsConfig(cfg)) {
        if (outError) {
            *outError = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
        }
        return false;
    }

    if (shouldSyncLiveRules && !executeLiveRuleSyncPlan(liveRuleSyncPlan, liveRuleSyncError)) {
        configManager_->setCustomizedCartoonsConfig(previousCfg);
        if (outError) {
            *outError = liveRuleSyncError.isEmpty()
                            ? QString(obs_module_text("CustomizedCartoon.Error.SaveConfigFailed"))
                            : liveRuleSyncError;
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(cfgMutex_);
        cfg_ = cfg;
    }
    media_ = parseMedia(cfg);
    rules_ = parseRules(cfg);
    syncOverlaySceneItems();
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
    if (supportedVideoExtensions().contains(ext)) {
        return "video";
    }
    if (supportedImageExtensions().contains(ext)) {
        return "image";
    }
    return QString();
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

bool CustomizedCartoonService::prepareMediaDraftEntry(const QString& filePath, json& outMedia,
                                                      QString& outError) {
    outMedia = json::object();
    outError.clear();

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        outError = obs_module_text("CustomizedCartoon.Error.FileNotFound");
        return false;
    }

    if (fi.size() > maxMediaFileSizeBytes()) {
        outError = obs_module_text("CustomizedCartoon.Error.FileTooLarge");
        return false;
    }

    const QString sourceType = inferMediaType(filePath);
    if (sourceType.isEmpty()) {
        outError = obs_module_text("CustomizedCartoon.Error.UnsupportedMediaFormat");
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

    outMedia = {{"id", mediaId.toStdString()},
                {"name", fi.fileName().toStdString()},
                {"path", destPath.toStdString()},
                {"type", type.toStdString()},
                {"displaySec", defaultDisplaySecForType(type)},
                {"muted", type == "video"},
                {"preserveAspectRatio", false}};
    return true;
}

bool CustomizedCartoonService::importMediaFile(const QString& filePath, QString& outMediaId,
                                               QString& outError) {
    outMediaId.clear();
    json mediaEntry;
    if (!prepareMediaDraftEntry(filePath, mediaEntry, outError)) {
        return false;
    }

    json cfg = getConfigSnapshot();
    if (!cfg.contains("media") || !cfg["media"].is_array()) {
        cfg["media"] = json::array();
    }
    cfg["media"].push_back(mediaEntry);

    if (!saveConfig(cfg)) {
        outError = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
        if (mediaEntry.contains("path") && mediaEntry["path"].is_string()) {
            const QString path = QString::fromStdString(mediaEntry["path"].get<std::string>());
            if (!path.isEmpty() && QFile::exists(path)) {
                QFile::remove(path);
            }
        }
        return false;
    }

    outMediaId = mediaEntry.contains("id") && mediaEntry["id"].is_string()
                     ? QString::fromStdString(mediaEntry["id"].get<std::string>())
                     : QString();
    return true;
}

bool CustomizedCartoonService::deleteMedia(const QString& mediaId, QString& outError) {
    outError.clear();
    if (mediaPreviewing_ && previewMediaId_ == mediaId) {
        stopMediaPreview();
    }
    if (playing_ && playingMediaId_ == mediaId) {
        stopPlayback();
    }
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

const CustomizedCartoonService::MediaItem* CustomizedCartoonService::findMediaById(
    const QString& id) const {
    for (const auto& m : media_) {
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
        m.displaySec = defaultDisplaySecForType(m.type);
        if (it.contains("displaySec") && it["displaySec"].is_number_integer())
            m.displaySec = it["displaySec"].get<int>();
        if (it.contains("muted") && it["muted"].is_boolean()) {
            m.muted = it["muted"].get<bool>();
        } else {
            m.muted = (m.type == "video");
        }
        if (it.contains("preserveAspectRatio") && it["preserveAspectRatio"].is_boolean()) {
            m.preserveAspectRatio = it["preserveAspectRatio"].get<bool>();
        }
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
        syncOverlaySceneItems();
        pollTimer_.start();
        onPollTimer();
    } else if (status == OneSevenLiveStreamingStatus::NotStarted) {
        pollTimer_.stop();
        stopEngagements();
        stopPlayback();
        liveStreamID_.clear();
        syncOverlaySceneItems();
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

bool CustomizedCartoonService::isRuleDefinitionChanged(const RuleItem& current, const RuleItem& next) {
    return current.name != next.name || current.mediaId != next.mediaId ||
           current.engageType != next.engageType || current.points != next.points ||
           current.count != next.count || current.repeatable != next.repeatable;
}

bool CustomizedCartoonService::buildLiveRuleSyncPlan(const std::vector<RuleItem>& nextRules,
                                                     LiveRuleSyncPlan& plan,
                                                     QString& outError) const {
    outError.clear();
    plan.ruleIdsToDelete.clear();
    plan.rulesToCreate.clear();

    std::map<QString, const RuleItem*> currentRulesById;
    for (const auto& rule : rules_) {
        currentRulesById[rule.id] = &rule;
    }

    std::map<QString, const RuleItem*> nextRulesById;
    for (const auto& rule : nextRules) {
        nextRulesById[rule.id] = &rule;
    }

    for (const auto& [ruleId, currentRule] : currentRulesById) {
        const auto nextIt = nextRulesById.find(ruleId);
        if (nextIt == nextRulesById.end()) {
            if (ruleToEngageID_.find(ruleId) != ruleToEngageID_.end()) {
                plan.ruleIdsToDelete.push_back(ruleId);
            }
            continue;
        }

        const RuleItem& nextRule = *nextIt->second;
        if (isRuleDefinitionChanged(*currentRule, nextRule)) {
            outError = obs_module_text("CustomizedCartoon.Error.RuleContentLockedStreaming");
            return false;
        }

        if (currentRule->enabled && !nextRule.enabled) {
            if (ruleToEngageID_.find(ruleId) != ruleToEngageID_.end()) {
                plan.ruleIdsToDelete.push_back(ruleId);
            }
        } else if (!currentRule->enabled && nextRule.enabled && !nextRule.mediaId.isEmpty()) {
            plan.rulesToCreate.push_back(nextRule);
        }
    }

    for (const auto& [ruleId, nextRule] : nextRulesById) {
        if (currentRulesById.find(ruleId) == currentRulesById.end() && nextRule->enabled &&
            !nextRule->mediaId.isEmpty()) {
            plan.rulesToCreate.push_back(*nextRule);
        }
    }

    return true;
}

bool CustomizedCartoonService::executeLiveRuleSyncPlan(const LiveRuleSyncPlan& plan, QString& outError) {
    outError.clear();
    if (!apiWrapper_ || liveStreamID_.isEmpty()) {
        return true;
    }

    if (!plan.ruleIdsToDelete.empty()) {
        std::vector<std::string> engageIDs;
        std::vector<QString> deletedRuleIds;
        engageIDs.reserve(plan.ruleIdsToDelete.size());
        deletedRuleIds.reserve(plan.ruleIdsToDelete.size());

        for (const auto& ruleId : plan.ruleIdsToDelete) {
            const auto it = ruleToEngageID_.find(ruleId);
            if (it == ruleToEngageID_.end() || it->second.isEmpty()) {
                continue;
            }
            engageIDs.push_back(it->second.toStdString());
            deletedRuleIds.push_back(ruleId);
        }

        if (!engageIDs.empty()) {
            struct DeleteResult {
                bool ok{false};
                QString error;
            };

            auto promise = std::make_shared<std::promise<DeleteResult>>();
            auto future = promise->get_future();
            const std::string liveStreamID = liveStreamID_.toStdString();
            QPointer<CustomizedCartoonService> self = this;

            ScheduleOBSTask([self, liveStreamID, engageIDs = std::move(engageIDs), promise]() mutable {
                DeleteResult result;
                if (!self || !self->apiWrapper_) {
                    result.error = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
                    promise->set_value(std::move(result));
                    return;
                }

                result.ok = self->apiWrapper_->DeleteLiveEngagements(liveStreamID, engageIDs);
                if (!result.ok) {
                    result.error = self->apiWrapper_->getLastErrorMessage();
                }
                promise->set_value(std::move(result));
            });

            if (future.wait_for(std::chrono::seconds(15)) != std::future_status::ready) {
                outError = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
                return false;
            }

            const DeleteResult result = future.get();
            if (!result.ok) {
                outError = result.error.isEmpty()
                               ? QString(obs_module_text("CustomizedCartoon.Error.SaveConfigFailed"))
                               : result.error;
                return false;
            }

            for (const auto& ruleId : deletedRuleIds) {
                const auto engageIt = ruleToEngageID_.find(ruleId);
                if (engageIt != ruleToEngageID_.end()) {
                    engageProgress_.erase(engageIt->second);
                    ruleToEngageID_.erase(engageIt);
                }
            }
        }
    }

    if (!plan.rulesToCreate.empty()) {
        std::vector<OneSevenLiveEngagementCreate> creates;
        std::vector<QString> ruleIds;
        creates.reserve(plan.rulesToCreate.size());
        ruleIds.reserve(plan.rulesToCreate.size());

        for (const auto& rule : plan.rulesToCreate) {
            OneSevenLiveEngagementCreate create;
            if (rule.engageType == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE") {
                create.engageType = OneSevenLiveEngagementType::GiftLuckybagFirstPrizeMilestone;
                create.payload = json{{"count", rule.count}};
            } else {
                create.engageType = OneSevenLiveEngagementType::GiftAmountMilestone;
                create.payload = json{{"points", rule.points}, {"count", rule.count}};
            }
            create.isRepeatable = rule.repeatable;
            creates.push_back(std::move(create));
            ruleIds.push_back(rule.id);
        }

        struct CreateResult {
            bool ok{false};
            QString error;
            std::vector<OneSevenLiveEngagementCreateResult> results;
        };

        auto promise = std::make_shared<std::promise<CreateResult>>();
        auto future = promise->get_future();
        const std::string liveStreamID = liveStreamID_.toStdString();
        QPointer<CustomizedCartoonService> self = this;

        ScheduleOBSTask([self, liveStreamID, creates = std::move(creates), promise]() mutable {
            CreateResult result;
            if (!self || !self->apiWrapper_) {
                result.error = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
                promise->set_value(std::move(result));
                return;
            }

            result.ok = self->apiWrapper_->CreateLiveEngagements(liveStreamID, creates, result.results);
            if (!result.ok) {
                result.error = self->apiWrapper_->getLastErrorMessage();
            }
            promise->set_value(std::move(result));
        });

        if (future.wait_for(std::chrono::seconds(15)) != std::future_status::ready) {
            outError = obs_module_text("CustomizedCartoon.Error.SaveConfigFailed");
            return false;
        }

        CreateResult result = future.get();
        if (!result.ok) {
            outError = result.error.isEmpty()
                           ? QString(obs_module_text("CustomizedCartoon.Error.SaveConfigFailed"))
                           : result.error;
            return false;
        }

        for (const auto& item : result.results) {
            if (item.index < 0 || item.index >= static_cast<int>(ruleIds.size()) || item.engageID.isEmpty()) {
                continue;
            }
            ruleToEngageID_[ruleIds[item.index]] = item.engageID;
        }
    }

    return true;
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

void CustomizedCartoonService::previewPlayAll(const json* previewConfig) {
    playbackPreviewConfig_ = previewConfig ? *previewConfig : json::object();
    hasPlaybackPreviewConfig_ = (previewConfig != nullptr);
    const auto rules = previewConfig ? parseRules(*previewConfig) : rules_;
    for (const auto& r : rules) {
        if (!r.enabled || r.mediaId.isEmpty()) {
            continue;
        }
        playQueue_.push_back(r.mediaId);
    }
    if (!playing_) {
        startNextPlayback();
    }
}

bool CustomizedCartoonService::startMediaPreview(const QString& mediaId, bool landscape,
                                                 const json* previewTransform,
                                                 const json* previewConfig, QString& outError) {
    outError.clear();
    MediaItem previewMedia;
    const MediaItem* media = nullptr;
    if (previewConfig) {
        const auto previewMediaList = parseMedia(*previewConfig);
        for (const auto& item : previewMediaList) {
            if (item.id == mediaId) {
                previewMedia = item;
                media = &previewMedia;
                break;
            }
        }
    }
    if (!media) {
        media = findMediaById(mediaId);
    }
    if (!media) {
        outError = obs_module_text("CustomizedCartoon.Error.MediaNotFound");
        return false;
    }
    if (!QFile::exists(media->path)) {
        outError = obs_module_text("CustomizedCartoon.Error.FileNotFound");
        return false;
    }

    stopPlayback();
    previewTimer_.stop();
    if (positionPreviewing_) {
        stopPositionPreview();
    }

    mediaPreviewing_ = true;
    mediaPreviewLandscape_ = landscape;
    mediaPreviewIsMedia_ = (media->type == "video");
    previewMediaId_ = mediaId;
    mediaPreviewSnapshot_ = *media;
    hasMediaPreviewSnapshot_ = true;
    if (previewTransform) {
        mediaPreviewTransform_ = *previewTransform;
        hasMediaPreviewTransform_ = true;
    } else {
        mediaPreviewTransform_ = json::object();
        hasMediaPreviewTransform_ = false;
    }

    if (!applyPreviewCanvas(mediaPreviewLandscape_, outError)) {
        mediaPreviewing_ = false;
        previewMediaId_.clear();
        hasMediaPreviewSnapshot_ = false;
        return false;
    }

    syncOverlaySceneItems();
    applyOverlayTransform(mediaPreviewLandscape_, previewTransform);

    if (media->type == "video") {
        if (!mediaSource_) {
            mediaPreviewing_ = false;
            previewMediaId_.clear();
            hasMediaPreviewSnapshot_ = false;
            syncOverlaySceneItems();
            restorePreviewCanvas();
            outError = obs_module_text("CustomizedCartoon.Error.VideoSourceNotAvailable");
            emit previewStateChanged();
            return false;
        }
        setMediaLooping(true);
        obs_source_set_muted(mediaSource_, media->muted);
        obs_data_t* settings = obs_source_get_settings(mediaSource_);
        if (settings) {
            obs_data_set_string(settings, "local_file", media->path.toStdString().c_str());
            obs_source_update(mediaSource_, settings);
            obs_data_release(settings);
        }
        showOverlaySource(true);
        obs_source_media_restart(mediaSource_);
    } else {
        if (!imageSource_) {
            mediaPreviewing_ = false;
            previewMediaId_.clear();
            hasMediaPreviewSnapshot_ = false;
            syncOverlaySceneItems();
            restorePreviewCanvas();
            outError = obs_module_text("CustomizedCartoon.Error.ImageSourceNotAvailable");
            emit previewStateChanged();
            return false;
        }
        obs_data_t* settings = obs_source_get_settings(imageSource_);
        if (settings) {
            obs_data_set_string(settings, "file", media->path.toStdString().c_str());
            obs_source_update(imageSource_, settings);
            obs_data_release(settings);
        }
        showOverlaySource(false);
    }

    previewTimer_.start(std::max(1, media->displaySec) * 1000);
    playbackTimer_.stop();
    emit previewStateChanged();
    return true;
}

void CustomizedCartoonService::stopMediaPreview() {
    if (!mediaPreviewing_) {
        return;
    }
    mediaPreviewing_ = false;
    previewMediaId_.clear();
    hasMediaPreviewSnapshot_ = false;
    hasMediaPreviewTransform_ = false;
    mediaPreviewTransform_ = json::object();
    previewTimer_.stop();
    setMediaLooping(false);
    playbackTimer_.stop();
    hideOverlaySources();
    syncOverlaySceneItems();
    restorePreviewCanvas();
    emit previewStateChanged();
}

bool CustomizedCartoonService::isMediaPreviewing() const { return mediaPreviewing_; }

QString CustomizedCartoonService::previewingMediaId() const { return previewMediaId_; }

bool CustomizedCartoonService::isPositionPreviewing() const { return positionPreviewing_; }

bool CustomizedCartoonService::startPositionPreview(const QString& mediaId, bool landscape,
                                                    const json* previewTransform,
                                                    const json* previewConfig, QString& outError) {
    outError.clear();
    MediaItem previewMedia;
    const MediaItem* media = nullptr;
    if (previewConfig) {
        const auto previewMediaList = parseMedia(*previewConfig);
        for (const auto& item : previewMediaList) {
            if (item.id == mediaId) {
                previewMedia = item;
                media = &previewMedia;
                break;
            }
        }
    }
    if (!media) {
        media = findMediaById(mediaId);
    }
    if (!media) {
        outError = obs_module_text("CustomizedCartoon.Error.MediaNotFound");
        return false;
    }
    if (!QFile::exists(media->path)) {
        outError = obs_module_text("CustomizedCartoon.Error.FileNotFound");
        return false;
    }

    stopPlayback();
    if (mediaPreviewing_) {
        stopMediaPreview();
    }
    positionPreviewing_ = true;
    positionPreviewLandscape_ = landscape;
    positionPreviewIsMedia_ = (media->type == "video");
    positionPreviewMediaId_ = mediaId;
    positionPreviewSnapshot_ = *media;
    hasPositionPreviewSnapshot_ = true;
    if (previewTransform) {
        positionPreviewTransform_ = *previewTransform;
        hasPositionPreviewTransform_ = true;
    } else {
        positionPreviewTransform_ = json::object();
        hasPositionPreviewTransform_ = false;
    }

    if (!applyPreviewCanvas(positionPreviewLandscape_, outError)) {
        positionPreviewing_ = false;
        positionPreviewMediaId_.clear();
        hasPositionPreviewSnapshot_ = false;
        return false;
    }

    syncOverlaySceneItems();
    applyOverlayTransform(landscape, previewTransform);

    if (media->type == "video") {
        if (!mediaSource_) {
            positionPreviewing_ = false;
            positionPreviewMediaId_.clear();
            hasPositionPreviewSnapshot_ = false;
            syncOverlaySceneItems();
            restorePreviewCanvas();
            outError = obs_module_text("CustomizedCartoon.Error.VideoSourceNotAvailable");
            return false;
        }
        setMediaLooping(true);
        obs_source_set_muted(mediaSource_, media->muted);
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
        positionPreviewing_ = false;
        positionPreviewMediaId_.clear();
        hasPositionPreviewSnapshot_ = false;
        syncOverlaySceneItems();
        restorePreviewCanvas();
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
    positionPreviewMediaId_.clear();
    hasPositionPreviewSnapshot_ = false;
    hasPositionPreviewTransform_ = false;
    positionPreviewTransform_ = json::object();
    setMediaLooping(false);
    playbackTimer_.stop();
    hideOverlaySources();
    syncOverlaySceneItems();
    restorePreviewCanvas();
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

    bool useLandscapeConfig = positionPreviewing_ ? positionPreviewLandscape_ : true;
    double actualCanvasW = useLandscapeConfig ? 1280.0 : 720.0;
    double actualCanvasH = useLandscapeConfig ? 720.0 : 1280.0;

    obs_video_info ovi{};
    if (obs_get_video_info(&ovi) && ovi.base_width > 0 && ovi.base_height > 0) {
        actualCanvasW = static_cast<double>(ovi.base_width);
        actualCanvasH = static_cast<double>(ovi.base_height);
    }

    const double referenceCanvasW = useLandscapeConfig ? 1280.0 : 720.0;
    const double referenceCanvasH = useLandscapeConfig ? 720.0 : 1280.0;
    const double scaleX = referenceCanvasW > 0.0 ? actualCanvasW / referenceCanvasW : 1.0;
    const double scaleY = referenceCanvasH > 0.0 ? actualCanvasH / referenceCanvasH : 1.0;
    const double uniformScale = std::min(scaleX, scaleY);

    outTransform["x"] = uniformScale > 0.0 ? ti.pos.x / uniformScale : ti.pos.x;
    outTransform["y"] = uniformScale > 0.0 ? ti.pos.y / uniformScale : ti.pos.y;
    outTransform["scaleX"] = ti.scale.x;
    outTransform["scaleY"] = ti.scale.y;
    outTransform["rot"] = ti.rot;
    outTransform["alignment"] = ti.alignment;
    outTransform["boundsType"] = (int)ti.bounds_type;
    outTransform["boundsAlignment"] = ti.bounds_alignment;
    outTransform["boundsW"] = uniformScale > 0.0 ? ti.bounds.x / uniformScale : ti.bounds.x;
    outTransform["boundsH"] = uniformScale > 0.0 ? ti.bounds.y / uniformScale : ti.bounds.y;
    outTransform["cropToBounds"] = ti.crop_to_bounds;
    return true;
}

void CustomizedCartoonService::applyOverlayTransformForOrientation(bool landscape,
                                                                   const json* previewTransform,
                                                                   const QString& previewMediaId,
                                                                   const json* previewConfig) {
    applyOverlayTransform(landscape, previewTransform, previewMediaId, previewConfig);
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
    if (mediaPreviewing_) {
        stopMediaPreview();
    }
    if (playing_) {
        return;
    }
    while (!playQueue_.empty()) {
        const QString mediaId = playQueue_.front();
        playQueue_.pop_front();
        MediaItem previewMedia;
        const MediaItem* media = nullptr;
        if (hasPlaybackPreviewConfig_) {
            const auto previewMediaList = parseMedia(playbackPreviewConfig_);
            for (const auto& item : previewMediaList) {
                if (item.id == mediaId) {
                    previewMedia = item;
                    media = &previewMedia;
                    break;
                }
            }
        }
        if (!media) {
            media = findMediaById(mediaId);
        }
        if (!media) {
            continue;
        }
        if (!QFile::exists(media->path)) {
            continue;
        }

        ensureOverlaySources();
        ensureOverlaySceneItem();

        const bool landscape = streamManager_ ? streamManager_->getRoomInfo().landscape : true;

        playingMediaId_ = mediaId;
        playing_ = true;
        applyOverlayTransform(landscape, nullptr, QString(), nullptr);
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
    playingStartMs_ = 0;
    playQueue_.clear();
    playbackPreviewConfig_ = json::object();
    hasPlaybackPreviewConfig_ = false;
    hideOverlaySources();
    syncOverlaySceneItems();
}

bool CustomizedCartoonService::hasActiveRules() const {
    for (const auto& rule : rules_) {
        if (rule.enabled && !rule.mediaId.isEmpty()) {
            return true;
        }
    }
    return false;
}

bool CustomizedCartoonService::shouldKeepOverlaySources() const {
    if (mediaPreviewing_ || positionPreviewing_ || playing_ || !playQueue_.empty()) {
        return true;
    }
    return streamManager_ &&
           streamManager_->getCurrentStreamingStatus() == OneSevenLiveStreamingStatus::Streaming &&
           hasActiveRules();
}

void CustomizedCartoonService::syncOverlaySceneItems() {
    if (!shouldKeepOverlaySources()) {
        removeOverlaySceneItems();
        return;
    }

    ensureOverlaySources();
    ensureOverlaySceneItem();
}

void CustomizedCartoonService::removeOverlaySceneItems() {
    hideOverlaySources();

    if (mediaItem_) {
        obs_sceneitem_remove(mediaItem_);
        mediaItem_ = nullptr;
    }
    if (imageItem_) {
        obs_sceneitem_remove(imageItem_);
        imageItem_ = nullptr;
    }

    obs_source_t* sceneSource = getActivePreviewSceneSource();
    if (!sceneSource) {
        return;
    }

    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    if (scene) {
        if (obs_sceneitem_t* item = obs_scene_find_source(scene, kCustomizedCartoonMediaSourceName)) {
            obs_sceneitem_remove(item);
        }
        if (obs_sceneitem_t* item = obs_scene_find_source(scene, kCustomizedCartoonImageSourceName)) {
            obs_sceneitem_remove(item);
        }
    }

    obs_source_release(sceneSource);
}

void CustomizedCartoonService::ensureOverlaySources() {
    if (!mediaSource_) {
        mediaSource_ = obs_get_source_by_name(kCustomizedCartoonMediaSourceName);
    }
    if (!mediaSource_) {
        if (obs_source_get_display_name("ffmpeg_source")) {
            obs_data_t* settings = obs_data_create();
            obs_data_set_bool(settings, "looping", false);
            obs_data_set_bool(settings, "restart_on_activate", true);
            obs_data_set_bool(settings, "close_when_inactive", true);
            mediaSource_ = obs_source_create("ffmpeg_source", kCustomizedCartoonMediaSourceName,
                                             settings, nullptr);
            obs_data_release(settings);
        }
    }
    if (!imageSource_) {
        imageSource_ = obs_get_source_by_name(kCustomizedCartoonImageSourceName);
    }
    if (!imageSource_) {
        if (obs_source_get_display_name("image_source")) {
            obs_data_t* settings = obs_data_create();
            imageSource_ =
                obs_source_create("image_source", kCustomizedCartoonImageSourceName, settings, nullptr);
            obs_data_release(settings);
        }
    }
}

obs_source_t* CustomizedCartoonService::getActivePreviewSceneSource() const {
    obs_source_t* sceneSource = nullptr;
    if (obs_frontend_preview_program_mode_active()) {
        sceneSource = obs_frontend_get_current_preview_scene();
    }
    if (!sceneSource) {
        sceneSource = obs_frontend_get_current_scene();
    }
    return sceneSource;
}

bool CustomizedCartoonService::applyPreviewCanvas(bool landscape, QString& outError) {
    outError.clear();

    config_t* cfg = obs_frontend_get_profile_config();
    if (!cfg) {
        outError = obs_module_text("CustomizedCartoon.Error.PreviewCanvasApplyFailed");
        return false;
    }

    if (!previewVideoSettingsBackup_.valid) {
        previewVideoSettingsBackup_.baseW = static_cast<uint32_t>(config_get_uint(cfg, "Video", "BaseCX"));
        previewVideoSettingsBackup_.baseH = static_cast<uint32_t>(config_get_uint(cfg, "Video", "BaseCY"));
        previewVideoSettingsBackup_.outputW =
            static_cast<uint32_t>(config_get_uint(cfg, "Video", "OutputCX"));
        previewVideoSettingsBackup_.outputH =
            static_cast<uint32_t>(config_get_uint(cfg, "Video", "OutputCY"));
        previewVideoSettingsBackup_.valid = true;
    }

    const uint32_t targetBaseW = landscape ? kPreviewLandscapeCanvasW : kPreviewPortraitCanvasW;
    const uint32_t targetBaseH = landscape ? kPreviewLandscapeCanvasH : kPreviewPortraitCanvasH;
    const uint32_t targetOutputW = targetBaseW;
    const uint32_t targetOutputH = targetBaseH;

    obs_video_info ovi{};
    if (obs_get_video_info(&ovi) && ovi.base_width == targetBaseW && ovi.base_height == targetBaseH &&
        ovi.output_width == targetOutputW && ovi.output_height == targetOutputH) {
        return true;
    }

    config_set_uint(cfg, "Video", "BaseCX", targetBaseW);
    config_set_uint(cfg, "Video", "BaseCY", targetBaseH);
    config_set_uint(cfg, "Video", "OutputCX", targetOutputW);
    config_set_uint(cfg, "Video", "OutputCY", targetOutputH);

    if (config_save(cfg) < 0) {
        outError = obs_module_text("CustomizedCartoon.Error.PreviewCanvasApplyFailed");
        return false;
    }

    obs_frontend_reset_video();

    if (!obs_get_video_info(&ovi) || ovi.base_width != targetBaseW || ovi.base_height != targetBaseH ||
        ovi.output_width != targetOutputW || ovi.output_height != targetOutputH) {
        outError = obs_module_text("CustomizedCartoon.Error.PreviewCanvasApplyFailed");
        restorePreviewCanvas();
        return false;
    }

    return true;
}

void CustomizedCartoonService::restorePreviewCanvas() {
    if (!previewVideoSettingsBackup_.valid) {
        return;
    }

    config_t* cfg = obs_frontend_get_profile_config();
    if (!cfg) {
        previewVideoSettingsBackup_.valid = false;
        return;
    }

    config_set_uint(cfg, "Video", "BaseCX", previewVideoSettingsBackup_.baseW);
    config_set_uint(cfg, "Video", "BaseCY", previewVideoSettingsBackup_.baseH);
    config_set_uint(cfg, "Video", "OutputCX", previewVideoSettingsBackup_.outputW);
    config_set_uint(cfg, "Video", "OutputCY", previewVideoSettingsBackup_.outputH);

    if (config_save(cfg) >= 0) {
        obs_frontend_reset_video();
    }

    previewVideoSettingsBackup_.valid = false;
}

void CustomizedCartoonService::ensureOverlaySceneItem() {
    obs_source_t* sceneSource = getActivePreviewSceneSource();
    if (!sceneSource) {
        return;
    }
    obs_scene_t* scene = obs_scene_from_source(sceneSource);
    if (!scene) {
        obs_source_release(sceneSource);
        return;
    }

    if (mediaSource_) {
        mediaItem_ = obs_scene_find_source(scene, obs_source_get_name(mediaSource_));
        if (!mediaItem_) {
            mediaItem_ = obs_scene_add(scene, mediaSource_);
        }
        if (mediaItem_) {
            obs_sceneitem_set_order(mediaItem_, OBS_ORDER_MOVE_TOP);
            obs_sceneitem_set_visible(mediaItem_, false);
            obs_sceneitem_select(mediaItem_, false);
        }
    }
    if (imageSource_) {
        imageItem_ = obs_scene_find_source(scene, obs_source_get_name(imageSource_));
        if (!imageItem_) {
            imageItem_ = obs_scene_add(scene, imageSource_);
        }
        if (imageItem_) {
            obs_sceneitem_set_order(imageItem_, OBS_ORDER_MOVE_TOP);
            obs_sceneitem_set_visible(imageItem_, false);
            obs_sceneitem_select(imageItem_, false);
        }
    }

    obs_source_release(sceneSource);
}

void CustomizedCartoonService::applyOverlayTransform(bool landscape, const json* previewTransform,
                                                     const QString& previewMediaId,
                                                     const json* previewConfig) {
    bool useLandscapeConfig = landscape;
    double actualCanvasW = useLandscapeConfig ? 1280.0 : 720.0;
    double actualCanvasH = useLandscapeConfig ? 720.0 : 1280.0;

    obs_video_info ovi{};
    if (obs_get_video_info(&ovi) && ovi.base_width > 0 && ovi.base_height > 0) {
        actualCanvasW = static_cast<double>(ovi.base_width);
        actualCanvasH = static_cast<double>(ovi.base_height);
    }

    json transform;
    bool hasTransform = false;
    if (previewTransform) {
        transform = *previewTransform;
        hasTransform = true;
    } else {
        json cfg = getConfigSnapshot();
        if (cfg.contains("position") && cfg["position"].is_object()) {
            const char* key = useLandscapeConfig ? "landscape" : "portrait";
            if (cfg["position"].contains(key) && cfg["position"][key].is_object()) {
                transform = cfg["position"][key];
                hasTransform = true;
            }
        }
    }

    const double referenceCanvasW = useLandscapeConfig ? 1280.0 : 720.0;
    const double referenceCanvasH = useLandscapeConfig ? 720.0 : 1280.0;
    const double scaleX = referenceCanvasW > 0.0 ? actualCanvasW / referenceCanvasW : 1.0;
    const double scaleY = referenceCanvasH > 0.0 ? actualCanvasH / referenceCanvasH : 1.0;
    const double uniformScale = std::min(scaleX, scaleY);

    obs_transform_info ti{};
    if (!hasTransform) {
        ti = DefaultOverlayTransform(useLandscapeConfig);
        ti.pos.x = ti.pos.x * (float)uniformScale;
        ti.pos.y = ti.pos.y * (float)uniformScale;
        ti.bounds.x = ti.bounds.x * (float)uniformScale;
        ti.bounds.y = ti.bounds.y * (float)uniformScale;
    } else {
        ti.pos.x = static_cast<float>(transform.value("x", 0.0) * uniformScale);
        ti.pos.y = static_cast<float>(transform.value("y", 0.0) * uniformScale);
        ti.scale.x = transform.value("scaleX", 1.0);
        ti.scale.y = transform.value("scaleY", 1.0);
        ti.rot = transform.value("rot", 0.0);
        ti.alignment = transform.value("alignment", static_cast<uint32_t>(OBS_ALIGN_CENTER));
        ti.bounds_type =
            static_cast<obs_bounds_type>(transform.value("boundsType", (int)OBS_BOUNDS_NONE));
        ti.bounds_alignment =
            transform.value("boundsAlignment", static_cast<uint32_t>(OBS_ALIGN_CENTER));
        ti.bounds.x = static_cast<float>(transform.value("boundsW", 0.0) * uniformScale);
        ti.bounds.y = static_cast<float>(transform.value("boundsH", 0.0) * uniformScale);
        ti.crop_to_bounds = transform.value("cropToBounds", false);
    }

    MediaItem activePreviewPlaybackMedia;
    MediaItem applyPreviewMedia;
    const MediaItem* activeMedia = nullptr;
    if (positionPreviewing_ && !positionPreviewMediaId_.isEmpty()) {
        activeMedia = hasPositionPreviewSnapshot_ ? &positionPreviewSnapshot_
                                                  : findMediaById(positionPreviewMediaId_);
    } else if (mediaPreviewing_ && !previewMediaId_.isEmpty()) {
        activeMedia = hasMediaPreviewSnapshot_ ? &mediaPreviewSnapshot_ : findMediaById(previewMediaId_);
    } else if (playing_ && !playingMediaId_.isEmpty()) {
        if (hasPlaybackPreviewConfig_) {
            const auto previewMediaList = parseMedia(playbackPreviewConfig_);
            for (const auto& item : previewMediaList) {
                if (item.id == playingMediaId_) {
                    activePreviewPlaybackMedia = item;
                    activeMedia = &activePreviewPlaybackMedia;
                    break;
                }
            }
        }
        if (!activeMedia) {
            activeMedia = findMediaById(playingMediaId_);
        }
    }
    if (!activeMedia && !previewMediaId.isEmpty()) {
        if (previewConfig) {
            const auto previewMediaList = parseMedia(*previewConfig);
            for (const auto& item : previewMediaList) {
                if (item.id == previewMediaId) {
                    applyPreviewMedia = item;
                    activeMedia = &applyPreviewMedia;
                    break;
                }
            }
        }
        if (!activeMedia) {
            activeMedia = findMediaById(previewMediaId);
        }
    }
    if (activeMedia && activeMedia->preserveAspectRatio) {
        ti.bounds_type = OBS_BOUNDS_SCALE_INNER;
        ti.bounds_alignment = OBS_ALIGN_CENTER;
        ti.crop_to_bounds = false;
    }

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
    obs_source_set_muted(mediaSource_, media.muted);
    obs_data_t* settings = obs_source_get_settings(mediaSource_);
    if (settings) {
        obs_data_set_string(settings, "local_file", media.path.toStdString().c_str());
        obs_source_update(mediaSource_, settings);
        obs_data_release(settings);
    }

    showOverlaySource(true);
    playingStartMs_ = QDateTime::currentMSecsSinceEpoch();
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
    const QString expectedMediaId = media.id;
    playingStartMs_ = QDateTime::currentMSecsSinceEpoch();
    QPointer<CustomizedCartoonService> self = this;
    QTimer::singleShot(sec * 1000, this, [self, expectedMediaId]() {
        if (!self || !self->playing_ || self->playingMediaId_ != expectedMediaId) {
            return;
        }
        self->hideOverlaySources();
        self->playing_ = false;
        self->playingMediaId_.clear();
        self->playingStartMs_ = 0;
        self->startNextPlayback();
    });
}

void CustomizedCartoonService::checkVideoState() {
    if (!playing_ || !mediaSource_) {
        playbackTimer_.stop();
        return;
    }
    MediaItem previewMedia;
    const MediaItem* media = nullptr;
    if (hasPlaybackPreviewConfig_) {
        const auto previewMediaList = parseMedia(playbackPreviewConfig_);
        for (const auto& item : previewMediaList) {
            if (item.id == playingMediaId_) {
                previewMedia = item;
                media = &previewMedia;
                break;
            }
        }
    }
    if (!media) {
        media = findMediaById(playingMediaId_);
    }
    if (media) {
        const int durationMs = std::max(1, media->displaySec) * 1000;
        if (playingStartMs_ > 0 &&
            (QDateTime::currentMSecsSinceEpoch() - playingStartMs_) >= durationMs) {
            playbackTimer_.stop();
            hideOverlaySources();
            playing_ = false;
            playingMediaId_.clear();
            playingStartMs_ = 0;
            startNextPlayback();
            return;
        }
    }
    const obs_media_state s = obs_source_media_get_state(mediaSource_);
    if (s == OBS_MEDIA_STATE_ENDED || s == OBS_MEDIA_STATE_STOPPED || s == OBS_MEDIA_STATE_ERROR) {
        playbackTimer_.stop();
        hideOverlaySources();
        playing_ = false;
        playingMediaId_.clear();
        playingStartMs_ = 0;
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
                                {"displaySec", m.displaySec},
                                {"muted", m.muted},
                                {"preserveAspectRatio", m.preserveAspectRatio}});
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
