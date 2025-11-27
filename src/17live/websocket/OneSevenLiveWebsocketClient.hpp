#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

struct TLSHandles;

class OneSevenLiveWebsocketClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveWebsocketClient(QObject* parent = nullptr);
    ~OneSevenLiveWebsocketClient();

    void connectUrl(const QString& url);
    void disconnect();
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
    std::thread th;

    std::unique_ptr<TLSHandles> tls;

    std::function<void()> onOpen;
    std::function<void(const std::string&)> onMessage;
    std::function<void()> onClose;
    std::function<void(const std::string&)> onError;
};
