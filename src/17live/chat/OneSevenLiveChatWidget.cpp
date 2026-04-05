#include "OneSevenLiveChatWidget.hpp"

#include <QCloseEvent>
#include <QHideEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "../OneSevenLiveCoreManager.hpp"
#include "plugin-support.h"
#include "../ui/cef/CefWidgetHost.hpp"

OneSevenLiveChatWidget::OneSevenLiveChatWidget(QWidget* parent, const QString& chatUrl)
    : QWidget(parent), chatUrl_(chatUrl) {
    obs_log(LOG_INFO, "OneSevenLiveChatWidget constructed");

    this->setAttribute(Qt::WA_NativeWindow);

    browserContainer_ = new QWidget(this);
    browserContainer_->setContentsMargins(0, 0, 0, 0);

    cefHost_ = std::make_unique<CefWidgetHost>();

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    if (cefHost_->ensureCreated(browserContainer_, chatUrl_)) {
        QVBoxLayout* browserLayout = new QVBoxLayout(browserContainer_);
        browserLayout->setContentsMargins(0, 0, 0, 0);
        browserLayout->setSpacing(0);
        browserLayout->addWidget(cefHost_->widget());
        rootLayout->addWidget(browserContainer_);
    } else if (errorLabel_) {
        rootLayout->addWidget(errorLabel_);
    } else {
        errorLabel_ = new QLabel("Browser source not available", this);
        errorLabel_->setAlignment(Qt::AlignCenter);
        rootLayout->addWidget(errorLabel_);
    }

    // Loading overlay
    loadingOverlay = new QWidget(this);
    loadingOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 180);");

    QVBoxLayout* overlayLayout = new QVBoxLayout(loadingOverlay);
    overlayLayout->setAlignment(Qt::AlignCenter);

    loadingLabel = new QLabel(obs_module_text("ChatRoom.LoadingGifts"), loadingOverlay);
    loadingLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    overlayLayout->addWidget(loadingLabel);

    // Raise overlay to top
    loadingOverlay->raise();

    auto& core = OneSevenLiveCoreManager::getInstance();
    connect(&core, &OneSevenLiveCoreManager::giftsLoaded, this,
            &OneSevenLiveChatWidget::onGiftsLoaded);

    if (core.isGiftsLoaded()) {
        loadingOverlay->hide();
    } else {
        loadingOverlay->show();
    }
}

OneSevenLiveChatWidget::~OneSevenLiveChatWidget() {
    obs_log(LOG_INFO, "OneSevenLiveChatWidget destructor called");
    if (cefHost_) {
        cefHost_->release(true);
    }
    cefHost_.reset();
}

void OneSevenLiveChatWidget::shutdown() {
    obs_log(LOG_INFO, "OneSevenLiveChatWidget shutdown called");
    if (cefHost_) {
        cefHost_->release(true);
    }
}

void OneSevenLiveChatWidget::setUrl(const QString& url) {
    chatUrl_ = url;
    if (cefHost_) {
        cefHost_->setUrl(url);
    }
}

void OneSevenLiveChatWidget::reload() {
    if (cefHost_) {
        cefHost_->reload();
    }
}

void OneSevenLiveChatWidget::showEvent(QShowEvent* event) {
    // obs_log(LOG_INFO, "OneSevenLiveChatWidget showEvent");
    QWidget::showEvent(event);

    if (cefHost_ && cefHost_->widget()) {
        cefHost_->widget()->setVisible(true);
    }
    if (loadingOverlay && loadingOverlay->isVisible()) {
        loadingOverlay->raise();
    }
}

void OneSevenLiveChatWidget::hideEvent(QHideEvent* event) {
    // obs_log(LOG_INFO, "OneSevenLiveChatWidget hideEvent");
    QWidget::hideEvent(event);
    if (cefHost_ && cefHost_->widget()) {
        cefHost_->widget()->setVisible(false);
    }
}

void OneSevenLiveChatWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (loadingOverlay) {
        loadingOverlay->setGeometry(contentsRect());
    }
    if (errorLabel_) {
        errorLabel_->setGeometry(contentsRect());
    }
}

void OneSevenLiveChatWidget::closeEvent(QCloseEvent* event) {
    shutdown();
    QWidget::closeEvent(event);
}

void OneSevenLiveChatWidget::onGiftsLoaded() {
    if (loadingOverlay) {
        loadingOverlay->hide();
    }
}
