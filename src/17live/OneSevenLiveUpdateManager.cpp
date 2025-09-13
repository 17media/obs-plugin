#include "OneSevenLiveUpdateManager.hpp"

#include <obs-module.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QUrl>

#include "moc_OneSevenLiveUpdateManager.cpp"
#include "plugin-support.h"
#include "utility/DownloadWorker.hpp"
#include "utility/Meta.hpp"
#include "utility/RemoteTextThread.hpp"

OneSevenLiveUpdateManager::OneSevenLiveUpdateManager(QObject* parent) : QObject(parent) {}

void OneSevenLiveUpdateManager::checkForUpdates() {
    std::string response;
    std::string error;
    long responseCode = 0;
    bool success = GetRemoteFile("https://api.github.com/repos/17media/obs-plugin/releases",
                                 response, error, &responseCode, nullptr, "GET", nullptr,
                                 {"User-Agent: 17Live-OBS-Plugin"}, nullptr, 10, true, 0);

    if (!success || responseCode != 200) {
        emit updateCheckFailed(QString::fromStdString(error));
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(QByteArray::fromStdString(response));
    if (jsonDoc.isNull() || !jsonDoc.isArray()) {
        emit updateCheckFailed("Invalid JSON response for update check");
        return;
    }

    QJsonArray releases = jsonDoc.array();
    if (releases.isEmpty()) {
        emit updateNotAvailable();
        return;
    }

    QJsonObject latestRelease = releases[0].toObject();
    QString latestVersion = latestRelease["tag_name"].toString();
    QString currentVersion = getCurrentVersion();

    if (!compareVersions(currentVersion, latestVersion)) {
        emit updateNotAvailable();
        return;
    }

    emit updateAvailable(latestVersion, latestRelease["assets"].toArray());
}

void OneSevenLiveUpdateManager::downloadUpdate(const QString& downloadUrl,
                                               const QString& fileName) {
    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString filePath = QDir(downloadsPath).absoluteFilePath(fileName);

    if (QFileInfo::exists(filePath)) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(obs_module_text("Update.FileExists"));
        msgBox.setText(QString(obs_module_text("Update.FileExists.Message")).arg(fileName));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

        if (msgBox.exec() == QMessageBox::No) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(downloadsPath));
            QMessageBox::information(
                nullptr, obs_module_text("Update.InstallReminder"),
                QString(obs_module_text("Update.InstallReminder.Message")).arg(fileName));
            return;
        }
    }

    if (downloadProgressDialog) {
        downloadProgressDialog->deleteLater();
        downloadProgressDialog = nullptr;
    }

    downloadProgressDialog = new QProgressDialog(obs_module_text("Update.Downloading"),
                                                 obs_module_text("Update.Cancel"), 0, 0);
    downloadProgressDialog->setWindowModality(Qt::WindowModal);
    downloadProgressDialog->setAutoClose(false);
    downloadProgressDialog->setAutoReset(false);

    QThread* thread = new QThread;
    obs_log(LOG_INFO, "Downloading update from %s", downloadUrl.toUtf8().constData());
    obs_log(LOG_INFO, "Saving to %s", filePath.toUtf8().constData());
    DownloadWorker* worker = new DownloadWorker(downloadUrl, filePath);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &DownloadWorker::process);
    connect(worker, &DownloadWorker::finished, this,
            [this, thread, worker](bool success, const QString& error) {
                if (downloadProgressDialog) {
                    downloadProgressDialog->reset();
                    downloadProgressDialog->deleteLater();
                    downloadProgressDialog = nullptr;
                }
                if (!success) {
                    QMessageBox::warning(
                        nullptr, obs_module_text("Update.DownloadFailed"),
                        QString(obs_module_text("Update.DownloadFailed.NetworkError")).arg(error));
                } else {
                    onDownloadFinished();
                }
                thread->quit();
                thread->wait();
                worker->deleteLater();
                thread->deleteLater();
            });

    connect(downloadProgressDialog, &QProgressDialog::canceled, this,
            [worker]() { worker->cancel(); });

    thread->start();
    downloadProgressDialog->show();
}

// void OneSevenLiveUpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
//     if (downloadProgressDialog && bytesTotal > 0) {
//         int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
//         downloadProgressDialog->setValue(progress);

//         QString progressText = QString(obs_module_text("Update.DownloadProgress"))
//                                .arg(bytesReceived / 1024 / 1024)
//                                .arg(bytesTotal / 1024 / 1024);
//         downloadProgressDialog->setLabelText(progressText);
//     }
// }

void OneSevenLiveUpdateManager::onDownloadFinished() {
    if (downloadProgressDialog) {
        downloadProgressDialog->close();
        downloadProgressDialog->deleteLater();
        downloadProgressDialog = nullptr;
    }

    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDesktopServices::openUrl(QUrl::fromLocalFile(downloadsPath));

    QMessageBox::information(nullptr, obs_module_text("Update.DownloadComplete"),
                             obs_module_text("Update.DownloadComplete.Message"));
}

QString OneSevenLiveUpdateManager::getCurrentVersion() const {
    return QString(PLUGIN_VERSION);
}

QString OneSevenLiveUpdateManager::getSystemInfo() const {
    QString osInfo;

#ifdef Q_OS_MACOS
    osInfo = "macOS";
    QString arch = QSysInfo::currentCpuArchitecture();
    if (arch.contains("arm") || arch.contains("aarch64")) {
        osInfo += " arm64";
    } else {
        osInfo += " x86_64";
    }
#elif defined(Q_OS_WIN)
    osInfo = "Windows";
    osInfo += " " + QSysInfo::currentCpuArchitecture();
#elif defined(Q_OS_LINUX)
    osInfo = "Linux";
    osInfo += " " + QSysInfo::currentCpuArchitecture();
#else
    osInfo = "Unknown";
#endif

    return osInfo;
}

bool OneSevenLiveUpdateManager::compareVersions(const QString& version1,
                                                const QString& version2) const {
    QString v1 = version1.startsWith('v') ? version1.mid(1) : version1;
    QString v2 = version2.startsWith('v') ? version2.mid(1) : version2;

    QStringList parts1 = v1.split('.');
    QStringList parts2 = v2.split('.');

    while (parts1.size() < parts2.size())
        parts1.append("0");
    while (parts2.size() < parts1.size())
        parts2.append("0");

    for (int i = 0; i < parts1.size(); ++i) {
        int num1 = parts1[i].toInt();
        int num2 = parts2[i].toInt();

        if (num2 > num1) {
            return true;
        } else if (num2 < num1) {
            return false;
        }
    }

    return false;
}
