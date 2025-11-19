#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <atomic>
#include <thread>

struct mbedtls_ssl_context;
struct mbedtls_net_context;
struct mbedtls_ssl_config;
struct mbedtls_ctr_drbg_context;
struct mbedtls_entropy_context;
struct mbedtls_x509_crt;

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

    mbedtls_ssl_context* ssl{nullptr};
    mbedtls_net_context* server_fd{nullptr};
    mbedtls_ssl_config* conf{nullptr};
    mbedtls_ctr_drbg_context* ctr_drbg{nullptr};
    mbedtls_entropy_context* entropy{nullptr};
    mbedtls_x509_crt* cacert{nullptr};

    std::function<void()> onOpen;
    std::function<void(const std::string&)> onMessage;
    std::function<void()> onClose;
    std::function<void(const std::string&)> onError;
};

