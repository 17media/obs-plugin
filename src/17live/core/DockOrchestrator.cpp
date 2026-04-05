#include "DockOrchestrator.hpp"

#include "../OneSevenLiveMenuManager.hpp"

bool DockOrchestrator::isDockOpen(QDockWidget* dock) {
    return dock && dock->toggleViewAction() && dock->toggleViewAction()->isChecked();
}

void DockOrchestrator::centerDockOnMainWindow(QDockWidget* dock, QMainWindow* mainWindow) {
    if (!dock || !mainWindow) {
        return;
    }
    const QRect mainWindowGeometry = mainWindow->geometry();
    const int x = mainWindowGeometry.x() + (mainWindowGeometry.width() - dock->width()) / 2;
    const int y = mainWindowGeometry.y() + (mainWindowGeometry.height() - dock->height()) / 2;
    dock->move(x, y);
}

void DockOrchestrator::showDockAsFloating(QDockWidget* dock, QMainWindow* mainWindow,
                                          bool isStartupRestore) {
    if (!dock || isStartupRestore) {
        return;
    }
    dock->setFloating(true);
    dock->setVisible(true);
    centerDockOnMainWindow(dock, mainWindow);
}

void DockOrchestrator::syncMenuDockVisibility(OneSevenLiveMenuManager* menuManager,
                                               QDockWidget* chatDock, QDockWidget* streamingDock,
                                               QDockWidget* liveListDock, QDockWidget* rockZoneDock,
                                               QDockWidget* multiRtmpDock,
                                               QDockWidget* previewDock) const {
    if (!menuManager) {
        return;
    }
    menuManager->updateDockVisibility(isDockOpen(chatDock), isDockOpen(streamingDock),
                                      isDockOpen(liveListDock), isDockOpen(rockZoneDock),
                                      isDockOpen(multiRtmpDock), isDockOpen(previewDock));
}
