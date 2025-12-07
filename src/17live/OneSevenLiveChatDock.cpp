#include "OneSevenLiveChatDock.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QHideEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>

#include "OneSevenLiveCoreManager.hpp"
#include "moc_OneSevenLiveChatDock.cpp"
#include "utility/QCefView.hpp"

OneSevenLiveChatDock::OneSevenLiveChatDock(QWidget* parent, const QString& chatUrl)
    : QDockWidget(obs_module_text("ChatRoom.Title"), parent), chatUrl_(chatUrl) {
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
            &OneSevenLiveChatDock::onGiftsLoaded);

    if (!core.isGiftsLoaded()) {
        loadingOverlay->show();
        loadingOverlay->raise();
    }
}

OneSevenLiveChatDock::~OneSevenLiveChatDock() {}

void OneSevenLiveChatDock::setUrl(const QString& url) {
    chatUrl_ = url;
    if (cefView_) {
        cefView_->loadUrl(chatUrl_);
    }
}

void OneSevenLiveChatDock::reload() {
    if (cefView_) {
        cefView_->reload();
    }
}

void OneSevenLiveChatDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    if (cefView_) {
        QTimer::singleShot(0, cefView_, &QCefView::reload);
    }
    if (loadingOverlay && loadingOverlay->isVisible() && cefView_) {
        loadingOverlay->resize(cefView_->size());
        loadingOverlay->move(cefView_->pos());
        loadingOverlay->raise();
    }
}

void OneSevenLiveChatDock::hideEvent(QHideEvent* event) {
    QDockWidget::hideEvent(event);
}

void OneSevenLiveChatDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    if (loadingOverlay && loadingOverlay->isVisible() && cefView_) {
        loadingOverlay->resize(cefView_->size());
        loadingOverlay->move(cefView_->pos());
        loadingOverlay->raise();
    }
}

void OneSevenLiveChatDock::onGiftsLoaded() {
    if (loadingOverlay) {
        loadingOverlay->hide();
    }
}
