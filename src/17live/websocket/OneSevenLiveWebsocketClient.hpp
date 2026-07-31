#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <atomic>
#include <functional>
#include <memory>

struct TLSHandles;

class OneSevenLiveWebsocketClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveWebsocketClient(QObject* parent = nullptr);
    ~OneSevenLiveWebsocketClient();

    void connectUrl(const QString& url);
    void disconnect();
    void disconnectAsync();
    bool isConnected() const;
    void sendText(const QString& text);

    void setOpenCallback(const std::function<void()>& cb);
    void setMessageCallback(const std::function<void(const std::string&)>& cb);
    void setCloseCallback(const std::function<void()>& cb);
    void setErrorCallback(const std::function<void(const std::string&)>& cb);

   private:
    void startThread(const QString& host, const QString& port, const QString& path);
    void stopThread();
    void threadFunc(const QString& host, const QString& port, const QString& path);
    bool sendTLS(const std::string& data);
    void cleanupTLS();

    std::atomic<bool> connected{false};
    std::atomic<bool> running{false};
    QPointer<QThread> thread_;
    QPointer<QObject> runner_;

    std::unique_ptr<TLSHandles> tls;

    std::function<void()> onOpen;
    std::function<void(const std::string&)> onMessage;
    std::function<void()> onClose;
    std::function<void(const std::string&)> onError;
};
