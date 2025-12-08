#include "OneSevenLiveChatRoomDock.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QHideEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>

#include "../OneSevenLiveCoreManager.hpp"
#include "../utility/QCefView.hpp"

// Note: With AUTOMOC enabled in CMake, we typically don't need to include the moc file explicitly
// unless we want to force compilation order or have Q_OBJECT in the cpp file.
// The original code included it, so we'll follow the pattern if needed, but usually it's not.
// If compilation fails, we might need to check this.
// #include "moc_OneSevenLiveChatRoomDock.cpp" 

OneSevenLiveChatRoomDock::OneSevenLiveChatRoomDock(QWidget* parent, const QString& chatUrl)
    : QDockWidget(obs_module_text("ChatRoom.Title"), parent), chatUrl_(chatUrl) {
    setObjectName("OneSevenLiveChatRoomDock");
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);
    
    cefView_ = new QCefView(this);
    setWidget(cefView_);
    if (!chatUrl_.isEmpty()) {
        cefView_->loadUrl(chatUrl_);
    }

    // Loading overlay
    loadingOverlay = new QWidget(this);
    loadingOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 180);");
    loadingOverlay->hide();

    QVBoxLayout* overlayLayout = new QVBoxLayout(loadingOverlay);
    overlayLayout->setAlignment(Qt::AlignCenter);

    loadingLabel = new QLabel(obs_module_text("ChatRoom.LoadingGifts"), loadingOverlay);
    loadingLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    overlayLayout->addWidget(loadingLabel);

    auto& core = OneSevenLiveCoreManager::getInstance();
    connect(&core, &OneSevenLiveCoreManager::giftsLoaded, this,
            &OneSevenLiveChatRoomDock::onGiftsLoaded);

    if (!core.isGiftsLoaded()) {
        loadingOverlay->show();
        loadingOverlay->raise();
    }
}

OneSevenLiveChatRoomDock::~OneSevenLiveChatRoomDock() {}

void OneSevenLiveChatRoomDock::setUrl(const QString& url) {
    chatUrl_ = url;
    if (cefView_) {
        cefView_->loadUrl(chatUrl_);
    }
}

void OneSevenLiveChatRoomDock::reload() {
    if (cefView_) {
        cefView_->reload();
    }
}

void OneSevenLiveChatRoomDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    if (cefView_) {
        // Force resize to ensure CEF view matches dock contents
        cefView_->setGeometry(contentsRect());
        // Reload with a slight delay to ensure layout is stable
        QTimer::singleShot(100, cefView_, &QCefView::reload);
    }
    if (loadingOverlay && loadingOverlay->isVisible()) {
        loadingOverlay->setGeometry(contentsRect());
        loadingOverlay->raise();
    }
}

void OneSevenLiveChatRoomDock::hideEvent(QHideEvent* event) {
    QDockWidget::hideEvent(event);
}

void OneSevenLiveChatRoomDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    if (loadingOverlay && loadingOverlay->isVisible()) {
        loadingOverlay->setGeometry(contentsRect());
        loadingOverlay->raise();
    }
    if (cefView_) {
         // Ensure cefView matches the dock contents
         cefView_->setGeometry(contentsRect());
    }
}

void OneSevenLiveChatRoomDock::onGiftsLoaded() {
    if (loadingOverlay) {
        loadingOverlay->hide();
    }
}
