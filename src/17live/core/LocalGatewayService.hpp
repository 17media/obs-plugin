#pragma once

#include <QObject>
#include <QPointer>
#include <string>
#include <memory>

class OneSevenLiveCoreManager;
class OneSevenLiveHttpServer;
class OneSevenLiveWebsocketServer;

class LocalGatewayService : public QObject {
    Q_OBJECT
public:
    explicit LocalGatewayService(OneSevenLiveCoreManager* coreManager, QObject* parent = nullptr);
    ~LocalGatewayService() override;

    bool initLocalServers();
    void shutdownLocalServers();

    OneSevenLiveHttpServer* getHttpServer() const;
    OneSevenLiveWebsocketServer* getWebsocketServer() const;

private:
    OneSevenLiveCoreManager* coreManager_;
    std::shared_ptr<OneSevenLiveHttpServer> httpServer_;
    std::shared_ptr<OneSevenLiveWebsocketServer> websocketServer_;
};
