#pragma once

#include <QMainWindow>
#include <QObject>
#include <QPointer>
#include <QProgressDialog>
#include <QString>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;
struct OneSevenLiveLoginData;
class QDialog;
class QListWidget;

class CrashUploadService : public QObject {
    Q_OBJECT

   public:
    CrashUploadService(QMainWindow* mainWindow, OneSevenLiveApiWrappers* apiWrapper,
                       OneSevenLiveConfigManager* configManager, QObject* parent = nullptr);

    void processPreviousRun(bool previousRunClean);
    void onLogin(const OneSevenLiveLoginData& loginData);
    void showCrashRecordsDialog();

   private:
    QPointer<QMainWindow> mainWindow_;
    OneSevenLiveApiWrappers* apiWrapper_{nullptr};
    OneSevenLiveConfigManager* configManager_{nullptr};
    QPointer<QProgressDialog> progressDialog_;
    std::shared_ptr<std::atomic<bool>> cancelFlag_;

    std::atomic<bool> prompted_{false};
    std::atomic<bool> inFlight_{false};
    std::atomic<bool> packagingInFlight_{false};
    std::atomic<bool> abnormalExitDetected_{false};
    std::atomic<bool> userAcceptedUpload_{false};

    struct CrashCandidate {
        std::string fileName;
        int64_t mtimeSec{0};
    };

    struct CrashRecord {
        std::string id;
        std::string archivePath;
        int64_t createdAtSec{0};
        int64_t crashTimestampSec{0};
        bool uploaded{false};
        int64_t uploadedAtSec{0};
        std::vector<std::string> recordKeys;
    };

    std::vector<CrashCandidate> detectCrashCandidates() const;
    std::vector<std::string> buildRecordKeys(const std::vector<CrashCandidate>& candidates) const;
    bool hasExistingRecordForKeys(const std::vector<std::string>& keys) const;
    bool getCurrentLoginData(OneSevenLiveLoginData& loginData) const;
    std::vector<CrashRecord> loadCrashRecords() const;
    std::optional<CrashRecord> findPendingRecord() const;
    bool saveCrashRecord(const CrashRecord& record) const;
    bool updateCrashRecordUploadStatus(const std::string& recordId, bool uploaded,
                                       int64_t uploadedAtSec) const;
    QString buildCrashRecordStatusText(const CrashRecord& record) const;
    void deleteCrashRecordFiles(const CrashRecord& record) const;
    void addCrashRecordRow(QListWidget* list, QDialog* dialog, const CrashRecord& record,
                           bool isLoggedIn, const OneSevenLiveLoginData& loginData);
    void pruneCrashRecords() const;
    void maybePromptUploadForAbnormalExit();
    void promptUploadForAbnormalExit(const OneSevenLiveLoginData& loginData);
    void promptUploadForRecord(const OneSevenLiveLoginData& loginData, const CrashRecord& record);
    void packageCrashRecordAsync(std::vector<CrashCandidate> candidates);
    void onCrashRecordPackaged(const CrashRecord& record);
    void maybeStartAcceptedUpload();

    void startUploadAsync(const OneSevenLiveLoginData& loginData, const CrashRecord& record);
    void updateProgressDialog(const char* textKey, int value);
    void closeProgressDialog();
    void showUploadResultMessage(bool ok, bool cancelled);
    void showUploadFailureMessage(const char* textKey);
    bool reportCrashEvent(const CrashRecord& record,
                          const std::shared_ptr<std::atomic<bool>>& cancelFlag);
    bool uploadCrashArchive(const CrashRecord& record,
                            const std::shared_ptr<std::atomic<bool>>& cancelFlag);

    static std::string crashLogsDirectory();
    static std::string crashArchivePath(const std::string& recordId);
    static std::string crashMetadataPath(const std::string& recordId);

    static std::string normalizeFileName(const std::string& fileName);
    static std::string md5Hex(const std::string& s);
};
