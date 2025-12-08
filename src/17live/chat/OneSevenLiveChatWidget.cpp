#include "OneSevenLiveChatWidget.hpp"
#include "cef_panel.hpp"
#include "../OneSevenLiveCoreManager.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>

#include "plugin-support.h"

OneSevenLiveChatWidget::OneSevenLiveChatWidget(QWidget* parent, const QString& chatUrl)
    : QWidget(parent), chatUrl_(chatUrl) {
    
    // Making this a native window often helps with embedding native child windows (CEF)
    this->setAttribute(Qt::WA_NativeWindow);
    
    cef_ = obs_browser_init_panel();
    if (cef_) {
        cef_->init_browser();
        cefWidget_ = cef_->create_widget(this, chatUrl_.toStdString());
        if (cefWidget_) {
            int panel_version = obs_browser_qcef_version();
            if (panel_version >= 1) {
                cefWidget_->allowAllPopups(true);
            }
        } else {
             obs_log(LOG_ERROR, "Failed to create QCefWidget");
             errorLabel_ = new QLabel("Failed to create CEF widget", this);
             errorLabel_->setAlignment(Qt::AlignCenter);
        }
    } else {
        obs_log(LOG_WARNING, "Browser panels unavailable (obs-browser missing or Wayland)");
        errorLabel_ = new QLabel("Browser source not available", this);
        errorLabel_->setAlignment(Qt::AlignCenter);
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
    if (cefWidget_) {
        cefWidget_->closeBrowser();
    }
}

void OneSevenLiveChatWidget::setUrl(const QString& url) {
    chatUrl_ = url;
    if (cefWidget_) {
        cefWidget_->setURL(chatUrl_.toStdString());
    }
}

void OneSevenLiveChatWidget::reload() {
    if (cefWidget_) {
        cefWidget_->reloadPage();
    }
}

void OneSevenLiveChatWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (cefWidget_) {
        cefWidget_->setVisible(true);
        cefWidget_->setGeometry(rect());
        QTimer::singleShot(100, [this]() {
            if (cefWidget_) cefWidget_->reloadPage();
        });
    }
    if (loadingOverlay && loadingOverlay->isVisible()) {
        loadingOverlay->raise();
    }
}

void OneSevenLiveChatWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (cefWidget_) {
        cefWidget_->setVisible(false);
    }
}

void OneSevenLiveChatWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (cefWidget_) {
        cefWidget_->setGeometry(rect());
    }
    if (loadingOverlay) {
        loadingOverlay->setGeometry(rect());
    }
    if (errorLabel_) {
        errorLabel_->setGeometry(rect());
    }
}

void OneSevenLiveChatWidget::onGiftsLoaded() {
    if (loadingOverlay) {
        loadingOverlay->hide();
    }
}
