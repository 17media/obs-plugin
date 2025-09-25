#pragma once

#include <QMutex>
#include <QObject>
#include <QThread>
#include <QWaitCondition>

class DownloadWorker : public QObject {
    Q_OBJECT
   public:
    DownloadWorker(const QString& url, const QString& filePath);

    void cancel();

   signals:
    void progress(int value);
    void finished(bool success, const QString& error);

   public slots:
    void process();

   private:
    QString downloadUrl;
    QString filePath;
    bool canceled;
    QMutex mutex;
};
