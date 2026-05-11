#include "CrashUploadService.hpp"

#include <obs-module.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <regex>
#include <unordered_set>

#include "../OneSevenLiveConfigManager.hpp"
#include "../api/OneSevenLiveApiWrappers.hpp"
#include "../utility/Common.hpp"
#include "../../diag/IDiagnosticsCollector.hpp"
#include "plugin-support.h"

#include "plugin-support.h"

CrashUploadService::CrashUploadService(QMainWindow* mainWindow, OneSevenLiveApiWrappers* apiWrapper,
                                       OneSevenLiveConfigManager* configManager, QObject* parent)
    : QObject(parent),
      mainWindow_(mainWindow),
      apiWrapper_(apiWrapper),
      configManager_(configManager) {}

void CrashUploadService::onLogin(const OneSevenLiveLoginData& loginData, bool previousRunClean) {
    if (prompted_.load()) {
        return;
    }
    if (previousRunClean) {
        return;
    }
    if (!mainWindow_ || !apiWrapper_ || !configManager_) {
        return;
    }

    const std::string userId = loginData.userInfo.userID.toStdString();
    if (userId.empty()) {
        return;
    }

    auto candidates = detectCrashCandidates();
    auto keys = buildRecordKeys(userId, candidates);

    if (!keys.empty() && isAlreadyUploaded(keys)) {
        return;
    }

    bool expected = false;
    if (!prompted_.compare_exchange_strong(expected, true)) {
        return;
    }

    auto* msgBox = new QMessageBox(mainWindow_);
    msgBox->setWindowTitle(obs_module_text("CrashUpload.Confirm.Title"));
    msgBox->setText(obs_module_text("CrashUpload.Confirm.Message"));
    QPushButton* cancelButton =
        msgBox->addButton(obs_module_text("CrashUpload.Confirm.Button.Cancel"), QMessageBox::NoRole);
    QPushButton* uploadButton =
        msgBox->addButton(obs_module_text("CrashUpload.Confirm.Button.Upload"), QMessageBox::YesRole);
    msgBox->setDefaultButton(cancelButton);

    const int64_t crashTimestampSec =
        !candidates.empty() ? candidates.front().mtimeSec
                            : QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

    QPointer<CrashUploadService> self = this;
    QObject::connect(msgBox, &QMessageBox::finished, this,
                     [self, msgBox, uploadButton, loginData, keys, crashTimestampSec](int) mutable {
                         if (!self) {
                             msgBox->deleteLater();
                             return;
                         }
                         if (msgBox->clickedButton() != uploadButton) {
                             msgBox->deleteLater();
                             return;
                         }

                         if (self->mainWindow_) {
                             auto* dlg =
                                 new QProgressDialog(obs_module_text("CrashUpload.Progress.Prepare"),
                                                     obs_module_text("CrashUpload.Progress.Cancel"),
                                                     0, 100, self->mainWindow_);
                             dlg->setWindowTitle(obs_module_text("CrashUpload.Confirm.Title"));
                             dlg->setWindowModality(Qt::ApplicationModal);
                             dlg->setAutoClose(false);
                             dlg->setAutoReset(false);
                             dlg->setMinimumDuration(0);
                             dlg->setValue(0);
                             self->progressDialog_ = dlg;
                             self->cancelFlag_ = std::make_shared<std::atomic<bool>>(false);

                             QPointer<CrashUploadService> s = self;
                             QObject::connect(dlg, &QProgressDialog::canceled, dlg, [s]() {
                                 if (s && s->cancelFlag_) {
                                     s->cancelFlag_->store(true);
                                 }
                                 if (s && s->progressDialog_) {
                                     s->progressDialog_->hide();
                                     s->progressDialog_->deleteLater();
                                     s->progressDialog_.clear();
                                 }
                             });

                             dlg->show();
                         }

                         self->startUploadAsync(loginData, std::move(keys), crashTimestampSec);
                         msgBox->deleteLater();
                     });
    msgBox->open();
}

std::vector<CrashUploadService::CrashCandidate> CrashUploadService::detectCrashCandidates() const {
    std::vector<CrashCandidate> out;

#if defined(__APPLE__)
    const std::string homeDir = QDir::homePath().toStdString();
    const std::vector<std::string> crashDirs = {homeDir + "/Library/Logs/DiagnosticReports",
                                                "/Library/Logs/DiagnosticReports"};

    struct Entry {
        std::string name;
        int64_t mtimeSec{0};
    };
    std::vector<Entry> found;

    for (const auto& dir : crashDirs) {
        try {
            if (!std::filesystem::exists(dir)) {
                continue;
            }
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                const auto ext = entry.path().extension().string();
                if (ext != ".crash" && ext != ".ips") {
                    continue;
                }
                const std::string filename = entry.path().filename().string();
                const bool isOBS = filename.rfind("obs", 0) == 0 || filename.rfind("OBS", 0) == 0;
                if (!isOBS) {
                    continue;
                }

                const auto mtime = std::filesystem::last_write_time(entry.path());
                const auto sctp =
                    std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        mtime - std::filesystem::file_time_type::clock::now() +
                        std::chrono::system_clock::now());
                const auto mtimeSec =
                    std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch())
                        .count();
                found.push_back({filename, mtimeSec});
            }
        } catch (...) {
        }
    }

    std::sort(found.begin(), found.end(),
              [](const Entry& a, const Entry& b) { return a.mtimeSec > b.mtimeSec; });
    if (found.size() > 5) {
        found.resize(5);
    }

    out.reserve(found.size());
    for (const auto& e : found) {
        out.push_back({e.name, e.mtimeSec});
    }
#endif

    return out;
}

std::vector<std::string> CrashUploadService::buildRecordKeys(
    const std::string& userId, const std::vector<CrashCandidate>& candidates) const {
    std::vector<std::string> out;
    out.reserve(candidates.size());

    for (const auto& c : candidates) {
        const std::string base = normalizeFileName(c.fileName);
        const std::string hash = md5Hex(c.fileName);
        out.push_back(userId + "|" + base + "|" + hash);
    }

    return out;
}

bool CrashUploadService::isAlreadyUploaded(const std::vector<std::string>& keys) const {
    if (!configManager_) {
        return false;
    }
    const auto history = configManager_->getCrashUploadHistory();
    if (history.empty()) {
        return false;
    }

    std::unordered_set<std::string> set(history.begin(), history.end());
    for (const auto& k : keys) {
        if (set.find(k) == set.end()) {
            return false;
        }
    }
    return true;
}

void CrashUploadService::startUploadAsync(const OneSevenLiveLoginData& loginData,
                                         std::vector<std::string> recordKeys,
                                         int64_t crashTimestampSec) {
    bool expected = false;
    if (!inFlight_.compare_exchange_strong(expected, true)) {
        return;
    }

    QPointer<CrashUploadService> self = this;
    auto cancelFlag = self ? self->cancelFlag_ : nullptr;
    ScheduleOBSTask(
        [self, cancelFlag, loginData, recordKeys = std::move(recordKeys), crashTimestampSec]() mutable {
        if (!self || !self->apiWrapper_ || !self->configManager_) {
            if (self) {
                self->inFlight_.store(false);
            }
            return;
        }

        if (cancelFlag && cancelFlag->load()) {
            self->inFlight_.store(false);
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self]() {
                if (self && self->progressDialog_) {
                    self->progressDialog_->setLabelText(
                        obs_module_text("CrashUpload.Progress.Collecting"));
                    self->progressDialog_->setValue(5);
                }
            },
            Qt::QueuedConnection);

        obs_log(LOG_INFO, "CrashUpload: collecting diagnostics package");

        std::string liveStreamID;
        std::string streamUrl;
        std::string streamKey;
        self->configManager_->getStreamingInfo(liveStreamID, streamUrl, streamKey);

        seventeen::diag::DiagnosticConfig cfg;
        cfg.enablePrivacyFilter = true;
        cfg.includeSensitiveData = false;
        cfg.categories = {seventeen::diag::DiagnosticCategory::OBS_LOGS,
                          seventeen::diag::DiagnosticCategory::PLUGIN_LOGS,
                          seventeen::diag::DiagnosticCategory::CRASH_INFO,
                          seventeen::diag::DiagnosticCategory::CONFIG_SNAPSHOT,
                          seventeen::diag::DiagnosticCategory::SYSTEM_INFO};

        const int64_t ts = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        std::string outPath =
            (std::filesystem::temp_directory_path() / ("17live_diagnostics_" + std::to_string(ts) +
                                                      ".zip"))
                .string();
        cfg.outputDirectory = outPath;

        auto collector = seventeen::diag::createDiagnosticsCollector();
        if (!collector || !collector->isSupported()) {
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (self && self->mainWindow_) {
                        if (self->progressDialog_) {
                            self->progressDialog_->hide();
                            self->progressDialog_->deleteLater();
                            self->progressDialog_.clear();
                        }
                        QMessageBox::warning(self->mainWindow_,
                                             obs_module_text("CrashUpload.Result.Title"),
                                             obs_module_text("CrashUpload.Result.Failed"),
                                             QMessageBox::Ok);
                    }
                },
                Qt::QueuedConnection);
            self->inFlight_.store(false);
            return;
        }

        const auto result = collector->collect(cfg);
        if (result.status != seventeen::diag::CollectStatus::SUCCESS) {
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (self && self->mainWindow_) {
                        if (self->progressDialog_) {
                            self->progressDialog_->hide();
                            self->progressDialog_->deleteLater();
                            self->progressDialog_.clear();
                        }
                        QMessageBox::warning(self->mainWindow_,
                                             obs_module_text("CrashUpload.Result.Title"),
                                             obs_module_text("CrashUpload.Result.Failed"),
                                             QMessageBox::Ok);
                    }
                },
                Qt::QueuedConnection);
            self->inFlight_.store(false);
            return;
        }

        obs_log(LOG_INFO, "CrashUpload: diagnostics package created: %s",
                result.outputPath.empty() ? "" : result.outputPath.c_str());

        if (cancelFlag && cancelFlag->load()) {
            try {
                std::filesystem::remove(result.outputPath);
            } catch (...) {
            }
            self->inFlight_.store(false);
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self]() {
                if (self && self->progressDialog_) {
                    self->progressDialog_->setLabelText(obs_module_text("CrashUpload.Progress.Report"));
                    self->progressDialog_->setValue(40);
                }
            },
            Qt::QueuedConnection);

        try {
            const auto fileSize = std::filesystem::file_size(result.outputPath);
            if (fileSize > 25ULL * 1024ULL * 1024ULL) {
                std::filesystem::remove(result.outputPath);
                QMetaObject::invokeMethod(
                    self,
                    [self]() {
                        if (self && self->mainWindow_) {
                            if (self->progressDialog_) {
                                self->progressDialog_->hide();
                                self->progressDialog_->deleteLater();
                                self->progressDialog_.clear();
                            }
                            QMessageBox::warning(self->mainWindow_,
                                                 obs_module_text("CrashUpload.Result.Title"),
                                                 obs_module_text("CrashUpload.Result.TooLarge"),
                                                 QMessageBox::Ok);
                        }
                    },
                    Qt::QueuedConnection);
                self->inFlight_.store(false);
                return;
            }
        } catch (...) {
        }

        bool ok = true;
        if (cancelFlag && cancelFlag->load()) {
            ok = false;
        } else if (!self->apiWrapper_->ReportObsCrashEvent(liveStreamID, crashTimestampSec)) {
            ok = false;
        } else if (!(cancelFlag && cancelFlag->load())) {
            obs_log(LOG_INFO, "CrashUpload: uploading diagnostics package");
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (self && self->progressDialog_) {
                        self->progressDialog_->setLabelText(
                            obs_module_text("CrashUpload.Progress.Uploading"));
                        self->progressDialog_->setValue(50);
                    }
                },
                Qt::QueuedConnection);

            ok = self->apiWrapper_->UploadObsLogsFile(
                result.outputPath, [self](double progress) {
                    if (!self) {
                        return;
                    }
                    const int pct = 50 + static_cast<int>(progress * 50.0);
                    QMetaObject::invokeMethod(
                        self,
                        [self, pct]() {
                            if (self && self->progressDialog_) {
                                self->progressDialog_->setValue(std::min(100, std::max(50, pct)));
                            }
                        },
                        Qt::QueuedConnection);
                },
                cancelFlag ? cancelFlag.get() : nullptr);
        }

        if (ok && !recordKeys.empty()) {
            self->configManager_->addCrashUploadHistory(recordKeys);
        }

        try {
            std::filesystem::remove(result.outputPath);
        } catch (...) {
        }

        QMetaObject::invokeMethod(
            self,
            [self, ok, cancelFlag]() {
                if (!self || !self->mainWindow_) {
                    return;
                }
                if (self->progressDialog_) {
                    self->progressDialog_->setValue(100);
                    self->progressDialog_->hide();
                    self->progressDialog_->deleteLater();
                    self->progressDialog_.clear();
                }
                if (cancelFlag && cancelFlag->load()) {
                    QMessageBox::information(self->mainWindow_,
                                             obs_module_text("CrashUpload.Result.Title"),
                                             obs_module_text("CrashUpload.Result.Cancelled"),
                                             QMessageBox::Ok);
                    return;
                }
                if (ok) {
                    QMessageBox::information(self->mainWindow_,
                                             obs_module_text("CrashUpload.Result.Title"),
                                             obs_module_text("CrashUpload.Result.Success"),
                                             QMessageBox::Ok);
                } else {
                    QMessageBox::warning(self->mainWindow_,
                                         obs_module_text("CrashUpload.Result.Title"),
                                         obs_module_text("CrashUpload.Result.Failed"),
                                         QMessageBox::Ok);
                }
            },
            Qt::QueuedConnection);

        self->inFlight_.store(false);
    });
}

std::string CrashUploadService::normalizeFileName(const std::string& fileName) {
    std::string s = fileName;
    static const std::regex patterns[] = {
        std::regex(R"(\d{8}_\d{6})"),
        std::regex(R"(\d{4}-\d{2}-\d{2}[-_]\d{2}-\d{2}-\d{2})"),
        std::regex(R"(\d{4}-\d{2}-\d{2})"),
        std::regex(R"(\d{14})"),
    };

    for (const auto& re : patterns) {
        s = std::regex_replace(s, re, "");
    }

    while (s.find("__") != std::string::npos) {
        s = std::regex_replace(s, std::regex("__"), "_");
    }
    while (s.find("--") != std::string::npos) {
        s = std::regex_replace(s, std::regex("--"), "-");
    }
    s = std::regex_replace(s, std::regex(R"([ _-]+(\.))"), "$1");
    s = std::regex_replace(s, std::regex(R"(^[ _-]+)"), "");
    s = std::regex_replace(s, std::regex(R"([ _-]+$)"), "");
    return s;
}

std::string CrashUploadService::md5Hex(const std::string& s) {
    const QByteArray input = QByteArray::fromStdString(s);
    const QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Md5);
    return QString(hash.toHex()).toStdString();
}
