#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <vector>

#include "OneSevenLiveApiWrappers.hpp"
#include "../websocket/OneSevenLiveWebsocketClient.hpp"

class OneSevenLiveAblyChatClient : public QObject {
    Q_OBJECT

   public:
    explicit OneSevenLiveAblyChatClient(QObject* parent = nullptr);
    ~OneSevenLiveAblyChatClient();

    void setRoomId(const QString& roomId);
    void setAblyToken(const QString& token);

    void setOnOpen(const std::function<void()>& cb);
    void setOnMessage(const std::function<void(const std::string&)>& cb);
    void setOnClose(const std::function<void()>& cb);
    void setOnError(const std::function<void(const std::string&)>& cb);

    bool connect();
    void disconnect();
    bool isConnected() const;

   private:
    void tryConnectWithFallbackHosts();
    void attachChannel();

    QString m_roomId;
    QString m_token;
    std::unique_ptr<OneSevenLiveWebsocketClient> m_wsClient;
    std::vector<QString> m_hosts;
    int m_hostIndex = 0;

    std::function<void()> m_onOpen;
    std::function<void(const std::string&)> m_onMessage;
    std::function<void()> m_onClose;
    std::function<void(const std::string&)> m_onError;
};