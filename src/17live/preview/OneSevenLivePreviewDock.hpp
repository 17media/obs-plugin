#pragma once

#include <QDockWidget>
#include <QPointer>
#include <QResizeEvent>
#include <QString>

#include "../OneSevenLiveCoreManager.hpp"
#include "OneSevenLivePreviewWidget.hpp"

class OneSevenLivePreviewDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLivePreviewDock(QWidget* parent = nullptr,
                                     const QString& overlayUrl = QString());
    ~OneSevenLivePreviewDock();

    void initializePreview();

   protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

   signals:
    void dockClosed();

   private:
    void setupUi();
    void updatePreviewGeometry();

    QPointer<OneSevenLivePreviewWidget> previewWidget;

    bool initialized;
    QString overlayUrl_;
};
