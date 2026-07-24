#include "CrashUploadService.hpp"

#include <obs-module.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMetaObject>
#include <QProgressDialog>
#include <QPushButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <regex>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "../OneSevenLiveConfigManager.hpp"
#include "../api/OneSevenLiveApiWrappers.hpp"
#include "../utility/Common.hpp"
#include "../../diag/IDiagnosticsCollector.hpp"
#include "plugin-support.h"

using json = nlohmann::json;

namespace {

constexpr size_t kMaxCrashRecords = 5;
constexpr const char* kCrashRecordPrefix = "crash_record_";

class CrashRecordListResizeFilter final : public QObject {
public:
    explicit CrashRecordListResizeFilter(QListWidget* list)
        : QObject(list),
          list_(list) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (!list_) {
            return QObject::eventFilter(watched, event);
        }
        if (event && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            updateItemWidths();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void updateItemWidths() {
        const int viewportWidth = list_->viewport() ? list_->viewport()->width() : 0;
        if (viewportWidth <= 0) {
            return;
        }

        for (int i = 0; i < list_->count(); ++i) {
            auto* item = list_->item(i);
            auto* row = list_->itemWidget(item);
            if (!row) {
                continue;
            }
            row->setFixedWidth(viewportWidth);
            if (row->layout()) {
                row->layout()->activate();
            }
            item->setSizeHint(QSize(viewportWidth, row->sizeHint().height()));
        }
    }

    QListWidget* list_{nullptr};
};

QString formatCrashTime(int64_t tsSec) {
    return QDateTime::fromSecsSinceEpoch(tsSec, Qt::UTC).toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
}

std::string sentinelKey() {
    try {
        const std::filesystem::path dir =
            std::filesystem::path(QDir::homePath().toStdString()) / ".17Live" / ".sentinel";
        if (!std::filesystem::exists(dir)) {
            return "sentinel|missing";
        }

        struct Entry {
            std::string name;
            int64_t mtimeSec{0};
        };
        std::vector<Entry> entries;
        for (const auto& it : std::filesystem::directory_iterator(dir)) {
            if (!it.is_regular_file()) {
                continue;
            }
            const std::string name = it.path().filename().u8string();
            if (name.rfind("run_", 0) != 0) {
                continue;
            }
            const auto mtime = std::filesystem::last_write_time(it.path());
            const auto sctp =
                std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    mtime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            const auto mtimeSec =
                std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
            entries.push_back({name, mtimeSec});
        }

        if (entries.empty()) {
            return "sentinel|empty";
        }

        std::sort(entries.begin(), entries.end(),
                  [](const Entry& a, const Entry& b) { return a.mtimeSec < b.mtimeSec; });
        return "sentinel|" + entries.front().name;
    } catch (...) {
        return "sentinel|error";
    }
}

std::string buildRecordFileStem(const std::string& recordId) {
    return std::string(kCrashRecordPrefix) + recordId;
}

}  // namespace

CrashUploadService::CrashUploadService(QMainWindow* mainWindow, OneSevenLiveApiWrappers* apiWrapper,
                                       OneSevenLiveConfigManager* configManager, QObject* parent)
    : QObject(parent),
      mainWindow_(mainWindow),
      apiWrapper_(apiWrapper),
      configManager_(configManager) {}

void CrashUploadService::processPreviousRun(bool previousRunClean) {
    if (previousRunClean || !configManager_) {
        return;
    }

    abnormalExitDetected_.store(true);

    const auto candidates = detectCrashCandidates();
    packageCrashRecordAsync(candidates);
    maybePromptUploadForAbnormalExit();
}

void CrashUploadService::onLogin(const OneSevenLiveLoginData& loginData) {
    Q_UNUSED(loginData);
    maybePromptUploadForAbnormalExit();
}

void CrashUploadService::showCrashRecordsDialog() {
    if (!mainWindow_) {
        return;
    }

    OneSevenLiveLoginData loginData;
    const bool isLoggedIn = getCurrentLoginData(loginData);
    const auto records = loadCrashRecords();

    QDialog dialog(mainWindow_);
    dialog.setWindowTitle(obs_module_text("CrashUpload.History.Title"));
    dialog.setModal(true);
    dialog.setMinimumWidth(560);
    dialog.setStyleSheet(
        "QDialog { background-color: #4A5568; color: #FFFFFF; }"
        "QLabel#historyTitle { color: #FFFFFF; font-size: 16px; font-weight: 700; }"
        "QLabel#historyHint { color: #D0D7E2; font-size: 12px; }"
        "QFrame#historyRow { background-color: rgba(255,255,255,0.06); border-radius: 6px; }"
        "QLabel#recordName { color: #FFFFFF; font-size: 14px; font-weight: 600; }"
        "QLabel#recordMeta, QLabel#recordStatus { color: #D0D7E2; font-size: 12px; }"
        "QLabel#recordTooLarge { color: #FFB020; font-size: 12px; }"
        "QPushButton { min-width: 88px; min-height: 32px; padding: 0px 12px; }"
        "QPushButton#uploadButton { background-color: #007AFF; color: #FFFFFF; border: none; "
        "border-radius: 2px; font-size: 14px; font-weight: 600; }"
        "QPushButton#uploadButton:hover { background-color: #0A84FF; }"
        "QPushButton#deleteButton, QPushButton#openFolderButton { background-color: #3C404D; color: #FFFFFF; "
        "border: 1px solid #757575; border-radius: 2px; font-size: 14px; font-weight: 600; }"
        "QPushButton#deleteButton:hover, QPushButton#openFolderButton:hover { background-color: #4A4F5E; }"
        "QPushButton#closeButton { background-color: #3C404D; color: #FFFFFF; border: 1px solid #757575; "
        "border-radius: 2px; font-size: 14px; font-weight: 600; }"
        "QPushButton#closeButton:hover { background-color: #4A4F5E; }"
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { background: transparent; border: none; }");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(obs_module_text("CrashUpload.History.Title"), &dialog);
    titleLabel->setObjectName("historyTitle");
    layout->addWidget(titleLabel);

    if (!isLoggedIn) {
        auto* hintLabel = new QLabel(obs_module_text("CrashUpload.History.LoginRequired"), &dialog);
        hintLabel->setObjectName("historyHint");
        hintLabel->setWordWrap(true);
        layout->addWidget(hintLabel);
    }

    if (records.empty()) {
        auto* emptyLabel = new QLabel(obs_module_text("CrashUpload.History.Empty"), &dialog);
        emptyLabel->setObjectName("historyHint");
        emptyLabel->setWordWrap(true);
        layout->addWidget(emptyLabel);
    } else {
        auto* list = new QListWidget(&dialog);
        list->setSpacing(8);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setWordWrap(true);
        list->setResizeMode(QListView::Adjust);
        list->viewport()->installEventFilter(new CrashRecordListResizeFilter(list));

        for (const auto& record : records) {
            auto* item = new QListWidgetItem();
            auto* row = new QFrame(list);
            row->setObjectName("historyRow");

            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(12, 10, 12, 10);
            rowLayout->setSpacing(12);

            auto* textLayout = new QVBoxLayout();
            textLayout->setContentsMargins(0, 0, 0, 0);
            textLayout->setSpacing(4);

            auto* nameLabel =
                new QLabel(QString::fromStdString(std::filesystem::path(record.archivePath).filename().string()), row);
            nameLabel->setObjectName("recordName");
            nameLabel->setWordWrap(true);
            nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            textLayout->addWidget(nameLabel);

            auto* metaLabel =
                new QLabel(QString(obs_module_text("CrashUpload.History.RecordMeta"))
                               .arg(formatCrashTime(record.createdAtSec))
                               .arg(formatCrashTime(record.crashTimestampSec)),
                           row);
            metaLabel->setObjectName("recordMeta");
            textLayout->addWidget(metaLabel);

            bool archiveExists = false;
            bool tooLarge = false;
            try {
                archiveExists = std::filesystem::exists(record.archivePath);
                if (archiveExists) {
                    const auto fileSize = std::filesystem::file_size(record.archivePath);
                    tooLarge = fileSize > 25ULL * 1024ULL * 1024ULL;
                }
            } catch (...) {
            }

            const QString statusText =
                tooLarge ? QString(obs_module_text("CrashUpload.History.StatusTooLarge"))
                : record.uploaded
                    ? QString(obs_module_text("CrashUpload.History.StatusUploaded"))
                          .arg(formatCrashTime(record.uploadedAtSec))
                    : QString(obs_module_text("CrashUpload.History.StatusPending"));
            auto* statusLabel = new QLabel(statusText, row);
            statusLabel->setObjectName("recordStatus");
            textLayout->addWidget(statusLabel);

            rowLayout->addLayout(textLayout, 1);

            auto* uploadButton = new QPushButton(
                record.uploaded ? obs_module_text("CrashUpload.History.ButtonUploaded")
                                : obs_module_text("CrashUpload.History.ButtonUpload"),
                row);
            uploadButton->setObjectName("uploadButton");
            uploadButton->setEnabled(isLoggedIn && !record.uploaded && archiveExists && !tooLarge &&
                                     !inFlight_.load());
            rowLayout->addWidget(uploadButton, 0, Qt::AlignVCenter);

            if (!record.uploaded) {
                QObject::connect(uploadButton, &QPushButton::clicked, &dialog,
                                 [this, &dialog, isLoggedIn, loginData, record]() {
                                     if (!isLoggedIn) {
                                         QMessageBox::information(
                                             mainWindow_, obs_module_text("CrashUpload.Result.Title"),
                                             obs_module_text("CrashUpload.History.LoginRequired"),
                                             QMessageBox::Ok);
                                         return;
                                     }
                                     dialog.accept();
                                     promptUploadForRecord(loginData, record);
                                 });
            }

            auto* deleteButton =
                new QPushButton(obs_module_text("CrashUpload.History.ButtonDelete"), row);
            deleteButton->setObjectName("deleteButton");
            deleteButton->setEnabled(!inFlight_.load());
            rowLayout->addWidget(deleteButton, 0, Qt::AlignVCenter);
            QObject::connect(deleteButton, &QPushButton::clicked, &dialog,
                             [this, list, item, row, record]() {
                                 if (!mainWindow_ || !list || !item || !row) {
                                     return;
                                 }
                                 const auto confirm =
                                     QMessageBox::question(mainWindow_,
                                                           obs_module_text("CrashUpload.History.DeleteConfirm.Title"),
                                                           obs_module_text("CrashUpload.History.DeleteConfirm.Message"),
                                                           QMessageBox::Yes | QMessageBox::No);
                                 if (confirm != QMessageBox::Yes) {
                                     return;
                                 }
                                 try {
                                     std::filesystem::remove(record.archivePath);
                                 } catch (...) {
                                 }
                                 try {
                                     std::filesystem::remove(crashMetadataPath(record.id));
                                 } catch (...) {
                                 }
                                 const int rowIndex = list->row(item);
                                 auto* taken = list->takeItem(rowIndex);
                                 list->removeItemWidget(taken);
                                 row->deleteLater();
                                 delete taken;
                             });

            item->setSizeHint(row->sizeHint());
            list->addItem(item);
            list->setItemWidget(item, row);
        }

        layout->addWidget(list, 1);
    }

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* openFolderButton = buttonBox->addButton(
        obs_module_text("CrashUpload.History.ButtonOpenFolder"), QDialogButtonBox::ActionRole);
    if (openFolderButton) {
        openFolderButton->setObjectName("openFolderButton");
    }
    if (auto* closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setObjectName("closeButton");
        closeButton->setText(obs_module_text("CrashUpload.History.ButtonClose"));
    }
    QObject::connect(buttonBox, &QDialogButtonBox::clicked, &dialog,
                     [openFolderButton](QAbstractButton* button) {
                         if (button != openFolderButton) {
                             return;
                         }
                         const QString dirPath =
                             QString::fromStdString(CrashUploadService::crashLogsDirectory());
                         QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
                     });
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    dialog.exec();
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
                    std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
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
    const std::vector<CrashCandidate>& candidates) const {
    std::vector<std::string> out;
    if (candidates.empty()) {
        out.push_back(sentinelKey());
        return out;
    }

    out.reserve(candidates.size());

    for (const auto& c : candidates) {
        const std::string base = normalizeFileName(c.fileName);
        const std::string hash = md5Hex(c.fileName + "|" + std::to_string(c.mtimeSec));
        out.push_back(base + "|" + hash);
    }

    return out;
}

bool CrashUploadService::hasExistingRecordForKeys(const std::vector<std::string>& keys) const {
    if (keys.empty()) {
        return false;
    }

    const auto records = loadCrashRecords();
    for (const auto& record : records) {
        std::unordered_set<std::string> existing(record.recordKeys.begin(), record.recordKeys.end());
        bool allMatched = true;
        for (const auto& key : keys) {
            if (existing.find(key) == existing.end()) {
                allMatched = false;
                break;
            }
        }
        if (allMatched) {
            return true;
        }
    }
    return false;
}

bool CrashUploadService::getCurrentLoginData(OneSevenLiveLoginData& loginData) const {
    if (!configManager_ || !apiWrapper_) {
        return false;
    }
    if (!configManager_->getLoginData(loginData)) {
        return false;
    }
    if (loginData.userInfo.userID.isEmpty() || loginData.jwtAccessToken.isEmpty()) {
        return false;
    }
    return !apiWrapper_->getToken().empty();
}

std::optional<CrashUploadService::CrashRecord> CrashUploadService::findPendingRecord() const {
    const auto records = loadCrashRecords();
    auto it =
        std::find_if(records.begin(), records.end(), [](const CrashRecord& record) { return !record.uploaded; });
    if (it == records.end()) {
        return std::nullopt;
    }
    return *it;
}

std::vector<CrashUploadService::CrashRecord> CrashUploadService::loadCrashRecords() const {
    std::vector<CrashRecord> out;
    const QString dirPath = QString::fromStdString(crashLogsDirectory());
    QDir dir(dirPath);
    if (!dir.exists()) {
        return out;
    }

    const QStringList files =
        dir.entryList(QStringList() << "crash_record_*.json", QDir::Files, QDir::Time);
    for (const auto& fileName : files) {
        QFile file(dir.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        try {
            const json j = json::parse(file.readAll().constData());
            CrashRecord record;
            record.id = j.value("id", "");
            record.archivePath = j.value("archivePath", "");
            record.createdAtSec = j.value("createdAtSec", 0LL);
            record.crashTimestampSec = j.value("crashTimestampSec", 0LL);
            record.uploaded = j.value("uploaded", false);
            record.uploadedAtSec = j.value("uploadedAtSec", 0LL);
            if (j.contains("recordKeys") && j["recordKeys"].is_array()) {
                for (const auto& key : j["recordKeys"]) {
                    if (key.is_string()) {
                        record.recordKeys.push_back(key.get<std::string>());
                    }
                }
            }

            if (record.id.empty() || record.archivePath.empty()) {
                continue;
            }
            if (!QFileInfo::exists(QString::fromStdString(record.archivePath))) {
                continue;
            }

            out.push_back(std::move(record));
        } catch (...) {
        }
    }

    std::sort(out.begin(), out.end(),
              [](const CrashRecord& a, const CrashRecord& b) { return a.createdAtSec > b.createdAtSec; });
    return out;
}

bool CrashUploadService::saveCrashRecord(const CrashRecord& record) const {
    QDir dir(QString::fromStdString(crashLogsDirectory()));
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    json j = {{"id", record.id},
              {"archivePath", record.archivePath},
              {"createdAtSec", record.createdAtSec},
              {"crashTimestampSec", record.crashTimestampSec},
              {"uploaded", record.uploaded},
              {"uploadedAtSec", record.uploadedAtSec},
              {"recordKeys", record.recordKeys}};

    QFile file(QString::fromStdString(crashMetadataPath(record.id)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray data = QByteArray::fromStdString(j.dump(2));
    return file.write(data) == data.size();
}

bool CrashUploadService::updateCrashRecordUploadStatus(const std::string& recordId, bool uploaded,
                                                       int64_t uploadedAtSec) const {
    const QString path = QString::fromStdString(crashMetadataPath(recordId));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    json j;
    try {
        j = json::parse(file.readAll().constData());
    } catch (...) {
        return false;
    }
    file.close();

    j["uploaded"] = uploaded;
    j["uploadedAtSec"] = uploadedAtSec;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray data = QByteArray::fromStdString(j.dump(2));
    return file.write(data) == data.size();
}

void CrashUploadService::pruneCrashRecords() const {
    auto records = loadCrashRecords();
    if (records.size() <= kMaxCrashRecords) {
        return;
    }

    for (size_t i = kMaxCrashRecords; i < records.size(); ++i) {
        try {
            std::filesystem::remove(records[i].archivePath);
        } catch (...) {
        }
        try {
            std::filesystem::remove(crashMetadataPath(records[i].id));
        } catch (...) {
        }
    }
}

void CrashUploadService::maybePromptUploadForAbnormalExit() {
    if (prompted_.load()) {
        return;
    }
    if (!abnormalExitDetected_.load()) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!getCurrentLoginData(loginData)) {
        return;
    }

    bool expected = false;
    if (!prompted_.compare_exchange_strong(expected, true)) {
        return;
    }

    promptUploadForAbnormalExit(loginData);
}

void CrashUploadService::promptUploadForAbnormalExit(const OneSevenLiveLoginData& loginData) {
    if (!mainWindow_) {
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

    QPointer<CrashUploadService> self = this;
    QObject::connect(msgBox, &QMessageBox::finished, this,
                     [self, msgBox, uploadButton, loginData](int) mutable {
                         if (!self) {
                             msgBox->deleteLater();
                             return;
                         }
                         if (msgBox->clickedButton() != uploadButton) {
                             msgBox->deleteLater();
                             return;
                         }

                         self->userAcceptedUpload_.store(true);

                         if (self->mainWindow_) {
                             auto* dlg =
                                 new QProgressDialog(obs_module_text("CrashUpload.Progress.Collecting"),
                                                     obs_module_text("CrashUpload.Progress.Cancel"), 0, 100,
                                                     self->mainWindow_);
                             dlg->setWindowTitle(obs_module_text("CrashUpload.Confirm.Title"));
                             dlg->setWindowModality(Qt::ApplicationModal);
                             dlg->setAutoClose(false);
                             dlg->setAutoReset(false);
                             dlg->setMinimumDuration(0);
                             dlg->setValue(10);
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

                         self->packageCrashRecordAsync(self->detectCrashCandidates());
                         self->maybeStartAcceptedUpload();
                         msgBox->deleteLater();
                     });
    msgBox->open();
}

void CrashUploadService::promptUploadForRecord(const OneSevenLiveLoginData& loginData,
                                               const CrashRecord& record) {
    if (!mainWindow_) {
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

    QPointer<CrashUploadService> self = this;
    QObject::connect(msgBox, &QMessageBox::finished, this,
                     [self, msgBox, uploadButton, loginData, record](int) mutable {
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
                                                     obs_module_text("CrashUpload.Progress.Cancel"), 0, 100,
                                                     self->mainWindow_);
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

                         self->startUploadAsync(loginData, record);
                         msgBox->deleteLater();
                     });
    msgBox->open();
}

void CrashUploadService::packageCrashRecordAsync(std::vector<CrashCandidate> candidates) {
    bool expected = false;
    if (!packagingInFlight_.compare_exchange_strong(expected, true)) {
        return;
    }

    const auto recordKeys = buildRecordKeys(candidates);
    if (recordKeys.empty()) {
        packagingInFlight_.store(false);
        return;
    }

    QPointer<CrashUploadService> self = this;
    ScheduleOBSTask([self, candidates = std::move(candidates), recordKeys]() mutable {
        if (!self || !self->configManager_) {
            if (self) {
                self->packagingInFlight_.store(false);
            }
            return;
        }

        try {
            std::filesystem::create_directories(CrashUploadService::crashLogsDirectory());
        } catch (...) {
        }

        const int64_t nowSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        const int64_t crashTimestampSec =
            !candidates.empty() ? candidates.front().mtimeSec : nowSec;
        const std::string recordId =
            std::to_string(nowSec) + "_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        const std::string outPath = CrashUploadService::crashArchivePath(recordId);

        seventeen::diag::DiagnosticConfig cfg;
        cfg.enablePrivacyFilter = true;
        cfg.includeSensitiveData = false;
        cfg.categories = {seventeen::diag::DiagnosticCategory::OBS_LOGS,
                          seventeen::diag::DiagnosticCategory::PLUGIN_LOGS,
                          seventeen::diag::DiagnosticCategory::CRASH_INFO,
                          seventeen::diag::DiagnosticCategory::CONFIG_SNAPSHOT,
                          seventeen::diag::DiagnosticCategory::SYSTEM_INFO};
        cfg.outputDirectory = outPath;

        auto collector = seventeen::diag::createDiagnosticsCollector();
        if (!collector || !collector->isSupported()) {
            obs_log(LOG_WARNING, "CrashUpload: diagnostics collector unavailable");
            self->packagingInFlight_.store(false);
            return;
        }

        const auto result = collector->collect(cfg);
        if (result.status != seventeen::diag::CollectStatus::SUCCESS) {
            obs_log(LOG_WARNING, "CrashUpload: background package failed: %s",
                    result.message.c_str());
            self->packagingInFlight_.store(false);
            return;
        }

        CrashRecord record;
        record.id = recordId;
        record.archivePath = result.outputPath;
        record.createdAtSec = nowSec;
        record.crashTimestampSec = crashTimestampSec;
        record.uploaded = false;
        record.uploadedAtSec = 0;
        record.recordKeys = recordKeys;

        if (!self->saveCrashRecord(record)) {
            obs_log(LOG_WARNING, "CrashUpload: failed to persist crash record metadata");
            self->packagingInFlight_.store(false);
            return;
        }

        self->pruneCrashRecords();

        QMetaObject::invokeMethod(
            self,
            [self, record]() {
                if (self) {
                    self->onCrashRecordPackaged(record);
                }
            },
            Qt::QueuedConnection);

        self->packagingInFlight_.store(false);
    });
}

void CrashUploadService::onCrashRecordPackaged(const CrashRecord& record) {
    Q_UNUSED(record);
    maybeStartAcceptedUpload();
}

void CrashUploadService::maybeStartAcceptedUpload() {
    if (!userAcceptedUpload_.load()) {
        return;
    }
    if (inFlight_.load()) {
        return;
    }

    OneSevenLiveLoginData loginData;
    if (!getCurrentLoginData(loginData)) {
        return;
    }

    const auto pending = findPendingRecord();
    if (!pending) {
        return;
    }

    try {
        if (!std::filesystem::exists(pending->archivePath)) {
            return;
        }
        const auto fileSize = std::filesystem::file_size(pending->archivePath);
        if (fileSize > 25ULL * 1024ULL * 1024ULL) {
            if (mainWindow_) {
                if (progressDialog_) {
                    progressDialog_->hide();
                    progressDialog_->deleteLater();
                    progressDialog_.clear();
                }
                QMessageBox::warning(mainWindow_, obs_module_text("CrashUpload.Result.Title"),
                                     obs_module_text("CrashUpload.Result.TooLarge"), QMessageBox::Ok);
            }
            userAcceptedUpload_.store(false);
            return;
        }
    } catch (...) {
    }

    // Consume the accepted abnormal-exit upload once we have a concrete pending record to upload.
    userAcceptedUpload_.store(false);
    startUploadAsync(loginData, *pending);
}

void CrashUploadService::startUploadAsync(const OneSevenLiveLoginData& loginData,
                                          const CrashRecord& record) {
    bool expected = false;
    if (!inFlight_.compare_exchange_strong(expected, true)) {
        if (mainWindow_) {
            QMessageBox::information(mainWindow_, obs_module_text("CrashUpload.Result.Title"),
                                     obs_module_text("CrashUpload.History.UploadBusy"), QMessageBox::Ok);
        }
        return;
    }

    QPointer<CrashUploadService> self = this;
    auto cancelFlag = self ? self->cancelFlag_ : nullptr;
    ScheduleOBSTask([self, cancelFlag, loginData, record]() mutable {
        if (!self || !self->apiWrapper_ || !self->configManager_) {
            if (self) {
                self->inFlight_.store(false);
            }
            return;
        }

        if (!std::filesystem::exists(record.archivePath)) {
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (self && self->mainWindow_) {
                        if (self->progressDialog_) {
                            self->progressDialog_->hide();
                            self->progressDialog_->deleteLater();
                            self->progressDialog_.clear();
                        }
                        QMessageBox::warning(self->mainWindow_, obs_module_text("CrashUpload.Result.Title"),
                                             obs_module_text("CrashUpload.Result.Failed"), QMessageBox::Ok);
                    }
                },
                Qt::QueuedConnection);
            self->inFlight_.store(false);
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self]() {
                if (self && self->progressDialog_) {
                    self->progressDialog_->setLabelText(obs_module_text("CrashUpload.Progress.Report"));
                    self->progressDialog_->setValue(30);
                }
            },
            Qt::QueuedConnection);

        try {
            const auto fileSize = std::filesystem::file_size(record.archivePath);
            if (fileSize > 25ULL * 1024ULL * 1024ULL) {
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

        std::string liveStreamID;
        std::string streamUrl;
        std::string streamKey;
        self->configManager_->getStreamingInfo(liveStreamID, streamUrl, streamKey);

        bool ok = true;
        if (cancelFlag && cancelFlag->load()) {
            ok = false;
        } else if (!self->apiWrapper_->ReportObsCrashEvent(liveStreamID, record.crashTimestampSec)) {
            ok = false;
        } else if (!(cancelFlag && cancelFlag->load())) {
            QMetaObject::invokeMethod(
                self,
                [self]() {
                    if (self && self->progressDialog_) {
                        self->progressDialog_->setLabelText(obs_module_text("CrashUpload.Progress.Uploading"));
                        self->progressDialog_->setValue(50);
                    }
                },
                Qt::QueuedConnection);

            ok = self->apiWrapper_->UploadObsLogsFile(
                record.archivePath,
                [self](double progress) {
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

        if (ok) {
            self->updateCrashRecordUploadStatus(
                record.id, true, QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
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
                    QMessageBox::information(self->mainWindow_, obs_module_text("CrashUpload.Result.Title"),
                                             obs_module_text("CrashUpload.Result.Success"), QMessageBox::Ok);
                } else {
                    QMessageBox::warning(self->mainWindow_, obs_module_text("CrashUpload.Result.Title"),
                                         obs_module_text("CrashUpload.Result.Failed"), QMessageBox::Ok);
                }
            },
            Qt::QueuedConnection);

        self->inFlight_.store(false);
    });
}

std::string CrashUploadService::crashLogsDirectory() {
    return (QDir::homePath() + "/.17Live/logs").toStdString();
}

std::string CrashUploadService::crashArchivePath(const std::string& recordId) {
    return (std::filesystem::path(crashLogsDirectory()) / (buildRecordFileStem(recordId) + ".zip")).string();
}

std::string CrashUploadService::crashMetadataPath(const std::string& recordId) {
    return (std::filesystem::path(crashLogsDirectory()) / (buildRecordFileStem(recordId) + ".json")).string();
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
