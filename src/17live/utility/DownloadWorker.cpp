#include "DownloadWorker.hpp"

#include <obs-module.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include "RemoteTextThread.hpp"
#include "moc_DownloadWorker.cpp"
#include "plugin-support.h"

DownloadWorker::DownloadWorker(const QString& url, const QString& filePath)
    : downloadUrl(url), filePath(filePath), canceled(false) {}

void DownloadWorker::cancel() {
    QMutexLocker locker(&mutex);
    canceled = true;
}

void DownloadWorker::process() {
    std::string error;
    long responseCode = 0;
    std::string content;

    bool success = GetRemoteFile(downloadUrl.toStdString().c_str(), content, error, &responseCode,
                                 nullptr, "GET", nullptr, {}, nullptr, 0, true);

    {
        QMutexLocker locker(&mutex);
        if (canceled) {
            // emit finished(false, "Download canceled");
            return;
        }
    }

    obs_log(LOG_INFO, "Download response code: %ld", responseCode);
    obs_log(LOG_INFO, "Download error: %s", error.c_str());

    if (!success || responseCode != 200) {
        emit finished(false, QString::fromStdString(error));
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit finished(false, "Cannot write file");
        return;
    }
    file.write(content.c_str(), content.size());
    file.close();

    emit finished(true, "");
}
