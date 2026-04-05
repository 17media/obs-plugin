#pragma once

#include <QDockWidget>
#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

class QObject;
class OneSevenLiveMenuManager;
class OneSevenLiveCoreContext;

class DockOrchestrator {
   public:
    explicit DockOrchestrator(OneSevenLiveCoreContext* core);

    static bool isDockOpen(QDockWidget* dock);
    static void centerDockOnMainWindow(QDockWidget* dock, QMainWindow* mainWindow);
    static void showDockAsFloating(QDockWidget* dock, QMainWindow* mainWindow, bool isStartupRestore);

    template <typename TDock>
    static bool closeAndDeleteDock(QPointer<TDock>& dock, QObject* owner) {
        bool visible = false;
        if (dock) {
            visible = isDockOpen(dock);
            auto* target = dock.data();
            if (QThread::currentThread() == target->thread()) {
                target->disconnect(owner);
                target->close();
                target->deleteLater();
            } else {
                QMetaObject::invokeMethod(
                    target,
                    [target, owner]() {
                        target->disconnect(owner);
                        target->close();
                        target->deleteLater();
                    },
                    Qt::QueuedConnection);
            }
            dock = nullptr;
        }
        return visible;
    }

    template <typename TDock>
    static bool closeAndDeleteDock(TDock*& dock, QObject* owner) {
        bool visible = false;
        if (dock) {
            visible = isDockOpen(dock);
            auto* target = dock;
            if (QThread::currentThread() == target->thread()) {
                target->disconnect(owner);
                target->close();
                target->deleteLater();
            } else {
                QMetaObject::invokeMethod(
                    target,
                    [target, owner]() {
                        target->disconnect(owner);
                        target->close();
                        target->deleteLater();
                    },
                    Qt::QueuedConnection);
            }
            dock = nullptr;
        }
        return visible;
    }

    template <typename TDock, typename TBeforeDelete>
    static bool closeAndDeleteDock(QPointer<TDock>& dock, QObject* owner,
                                   TBeforeDelete&& beforeDelete) {
        bool visible = false;
        if (dock) {
            visible = isDockOpen(dock);
            auto* target = dock.data();
            if (QThread::currentThread() == target->thread()) {
                target->disconnect(owner);
                beforeDelete(target);
                target->close();
                target->deleteLater();
            } else {
                QMetaObject::invokeMethod(
                    target,
                    [target, owner, beforeDelete = std::forward<TBeforeDelete>(beforeDelete)]() mutable {
                        target->disconnect(owner);
                        beforeDelete(target);
                        target->close();
                        target->deleteLater();
                    },
                    Qt::QueuedConnection);
            }
            dock = nullptr;
        }
        return visible;
    }

    template <typename TDock, typename TBeforeDelete>
    static bool closeAndDeleteDock(TDock*& dock, QObject* owner, TBeforeDelete&& beforeDelete) {
        bool visible = false;
        if (dock) {
            visible = isDockOpen(dock);
            auto* target = dock;
            if (QThread::currentThread() == target->thread()) {
                target->disconnect(owner);
                beforeDelete(target);
                target->close();
                target->deleteLater();
            } else {
                QMetaObject::invokeMethod(
                    target,
                    [target, owner, beforeDelete = std::forward<TBeforeDelete>(beforeDelete)]() mutable {
                        target->disconnect(owner);
                        beforeDelete(target);
                        target->close();
                        target->deleteLater();
                    },
                    Qt::QueuedConnection);
            }
            dock = nullptr;
        }
        return visible;
    }

    void syncMenuDockVisibility(OneSevenLiveMenuManager* menuManager, QDockWidget* chatDock,
                                QDockWidget* streamingDock, QDockWidget* liveListDock,
                                QDockWidget* rockZoneDock, QDockWidget* multiRtmpDock,
                                QDockWidget* previewDock) const;

    void closeAllDocks();
    void handleStreamingClicked();
    void createStreamingDock();
    void handleRockZoneClicked();
    void createRockZoneDock();
    void handleLiveListClicked();
    void saveDockState();
    void handleChatRoomClicked();
    void handleMultiRtmpClicked();
    void createMultiRtmpDock();
    void handlePreviewDockClicked();
    void createPreviewDock();

    void syncMenuDockVisibility();

   private:
    OneSevenLiveCoreContext* core_;
    bool streamingDockFirstLoad_{true};
    bool rockZoneDockFirstLoad_{true};
    bool multiRtmpDockFirstLoad_{true};
    bool previewDockFirstLoad_{true};
};
