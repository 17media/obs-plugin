#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QPointer>
#include <QResizeEvent>
#include <QString>

class QTimer;

#include "../OneSevenLiveCoreManager.hpp"
#include "OneSevenLivePreviewWidget.hpp"

class OneSevenLivePreviewDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLivePreviewDock(QWidget* parent = nullptr,
                                     const QString& overlayUrl = QString(),
                                     const QString& enterAnimUrl = QString());
    ~OneSevenLivePreviewDock();

    void initializePreview();
    void syncLayoutToObsCanvas();

   protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

   signals:
    void dockClosed();

   private:
    void setupUi();
    void updatePreviewGeometry();

    QPointer<OneSevenLivePreviewWidget> previewWidget = nullptr;
    QWidget* container = nullptr;
    QWidget* previewContainer = nullptr;
    QLabel* notificationLabel = nullptr;
    QWidget* loadingOverlay = nullptr;
    QLabel* loadingLabel = nullptr;
    QLabel* placeholderLabel = nullptr;

    bool initialized = false;
    QString overlayUrl_;
    QString enterAnimUrl_;

   private slots:
    void onGiftsLoaded();
    void onDisplayCreated(bool created);
    void onTopLevelChanged(bool floating);
    void onDockLocationChanged(Qt::DockWidgetArea area);
};
