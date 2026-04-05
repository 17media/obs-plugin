#pragma once

#include <QObject>

class QMainWindow;
class QDockWidget;

class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;
class OneSevenLiveMenuManager;
class OneSevenLiveHttpServer;
class OneSevenLiveWebsocketServer;
class OneSevenLiveStreamManager;

class OneSevenLiveStreamingDock;
class OneSevenLiveStreamListDock;
class OneSevenLiveRockZoneDock;
class OneSevenLiveMultiRtmpDock;
class OneSevenLivePreviewDock;

enum class OneSevenLiveStreamingStatus;

class OneSevenLiveCoreContext {
public:
    virtual ~OneSevenLiveCoreContext() = default;

    virtual QObject* getUiOwner() = 0;
    virtual QMainWindow* getMainWindow() const = 0;
    virtual OneSevenLiveMenuManager* getMenuManager() const = 0;
    virtual OneSevenLiveConfigManager* getConfigManager() const = 0;
    virtual OneSevenLiveApiWrappers* getApiWrapper() const = 0;
    virtual OneSevenLiveStreamManager* getStreamManager() const = 0;
    virtual OneSevenLiveStreamingStatus getStreamingStatus() const = 0;

    virtual bool getStartupRestore() const = 0;
    virtual void setStartupRestore(bool v) = 0;

    virtual OneSevenLiveHttpServer* getHttpServer() const = 0;
    virtual OneSevenLiveWebsocketServer* getWebsocketServer() const = 0;

    virtual void requestFlushChatEventQueue() = 0;

    virtual OneSevenLiveStreamingDock* getStreamingDock() const = 0;
    virtual void setStreamingDock(OneSevenLiveStreamingDock* dock) = 0;

    virtual QDockWidget* getChatDock() const = 0;
    virtual void setChatDock(QDockWidget* dock) = 0;

    virtual OneSevenLiveStreamListDock* getLiveListDock() const = 0;
    virtual void setLiveListDock(OneSevenLiveStreamListDock* dock) = 0;

    virtual OneSevenLiveRockZoneDock* getRockZoneDock() const = 0;
    virtual void setRockZoneDock(OneSevenLiveRockZoneDock* dock) = 0;

    virtual OneSevenLiveMultiRtmpDock* getMultiRtmpDock() const = 0;
    virtual void setMultiRtmpDock(OneSevenLiveMultiRtmpDock* dock) = 0;

    virtual OneSevenLivePreviewDock* getPreviewDock() const = 0;
    virtual void setPreviewDock(OneSevenLivePreviewDock* dock) = 0;
};
