#pragma once

#include <QJsonArray>
#include <QObject>
#include <QProgressDialog>
#include <QString>

class OneSevenLiveUpdateManager : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveUpdateManager(QObject* parent = nullptr);

    void checkForUpdates();
    QString getSystemInfo() const;
    void downloadUpdate(const QString& downloadUrl, const QString& fileName);

   signals:
    void updateAvailable(const QString& latestVersion, const QJsonArray& assets);
    void updateNotAvailable();
    void updateCheckFailed(const QString& error);

   private slots:
    // void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

   private:
    QString getCurrentVersion() const;
    bool compareVersions(const QString& version1, const QString& version2) const;

    QProgressDialog* downloadProgressDialog = nullptr;
};
