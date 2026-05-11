#pragma once

#include <QObject>
#include <QMainWindow>
#include <QPointer>
#include <QProgressDialog>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;
struct OneSevenLiveLoginData;

class CrashUploadService : public QObject {
    Q_OBJECT

   public:
    CrashUploadService(QMainWindow* mainWindow, OneSevenLiveApiWrappers* apiWrapper,
                       OneSevenLiveConfigManager* configManager, QObject* parent = nullptr);

    void onLogin(const OneSevenLiveLoginData& loginData, bool previousRunClean);

   private:
    QPointer<QMainWindow> mainWindow_;
    OneSevenLiveApiWrappers* apiWrapper_{nullptr};
    OneSevenLiveConfigManager* configManager_{nullptr};
    QPointer<QProgressDialog> progressDialog_;
    std::shared_ptr<std::atomic<bool>> cancelFlag_;

    std::atomic<bool> prompted_{false};
    std::atomic<bool> inFlight_{false};

    struct CrashCandidate {
        std::string fileName;
        int64_t mtimeSec{0};
    };

    std::vector<CrashCandidate> detectCrashCandidates() const;
    std::vector<std::string> buildRecordKeys(const std::string& userId,
                                             const std::vector<CrashCandidate>& candidates) const;
    bool isAlreadyUploaded(const std::vector<std::string>& keys) const;

    void startUploadAsync(const OneSevenLiveLoginData& loginData,
                          std::vector<std::string> recordKeys, int64_t crashTimestampSec);

    static std::string normalizeFileName(const std::string& fileName);
    static std::string md5Hex(const std::string& s);
};
