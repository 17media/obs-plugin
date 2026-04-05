#pragma once

#include <QDockWidget>
#include <QMainWindow>
#include <QPointer>

class QObject;
class OneSevenLiveMenuManager;

class DockOrchestrator {
   public:
    static bool isDockOpen(QDockWidget* dock);
    static void centerDockOnMainWindow(QDockWidget* dock, QMainWindow* mainWindow);
    static void showDockAsFloating(QDockWidget* dock, QMainWindow* mainWindow, bool isStartupRestore);

    template <typename TDock>
    static bool closeAndDeleteDock(QPointer<TDock>& dock, QObject* owner) {
        bool visible = false;
        if (dock) {
            visible = isDockOpen(dock);
            dock->disconnect(owner);
            dock->close();
            delete dock;
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
            dock->disconnect(owner);
            beforeDelete(dock.data());
            dock->close();
            delete dock;
            dock = nullptr;
        }
        return visible;
    }

    void syncMenuDockVisibility(OneSevenLiveMenuManager* menuManager, QDockWidget* chatDock,
                                QDockWidget* streamingDock, QDockWidget* liveListDock,
                                QDockWidget* rockZoneDock, QDockWidget* multiRtmpDock,
                                QDockWidget* previewDock) const;
};
