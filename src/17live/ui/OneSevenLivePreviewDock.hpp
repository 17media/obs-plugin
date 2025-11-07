#pragma once

#include <QDockWidget>
#include <QPointer>
#include <QString>

#include "../utility/OneSevenLivePreviewConfigLoader.hpp"
#include "OneSevenLivePreviewScreen.hpp"

class OneSevenLivePreviewDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLivePreviewDock(QWidget* parent = nullptr, const QString& overlayUrl = QString());
    ~OneSevenLivePreviewDock();

    void initializePreview();

   protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

   signals:
    void dockClosed();

   private:
    void setupUi();
    void loadConfiguration();

    QPointer<OneSevenLivePreviewScreen> previewScreen;
    QPointer<OneSevenLivePreviewConfigLoader> configLoader;

    bool initialized;
    QString overlayUrl_;
};
