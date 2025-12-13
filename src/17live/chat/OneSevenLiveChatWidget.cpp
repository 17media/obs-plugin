#include "OneSevenLiveChatWidget.hpp"

#include <QHideEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "../OneSevenLiveCoreManager.hpp"
#include "cef_panel.hpp"
#include "plugin-support.h"

OneSevenLiveChatWidget::OneSevenLiveChatWidget(QWidget* parent, const QString& chatUrl)
    : QWidget(parent), chatUrl_(chatUrl) {
    obs_log(LOG_INFO, "OneSevenLiveChatWidget constructed");

    // Making this a native window often helps with embedding native child windows (CEF)
    this->setAttribute(Qt::WA_NativeWindow);

    static QCef* globalCef = nullptr;
    if (!globalCef) {
        globalCef = obs_browser_init_panel();
        if (globalCef) {
            if (!globalCef->initialized()) {
                globalCef->init_browser();
            }
        }
    }
    cef_ = globalCef;

    if (cef_) {
        // cef_->init_browser(); // Already initialized globally
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

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    if (cefWidget_) {
        rootLayout->addWidget(cefWidget_);
    } else if (errorLabel_) {
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
    if (cefWidget_ && !browserClosed_) {
        int panel_version = obs_browser_qcef_version();
        if (panel_version >= 2) {
            obs_log(LOG_INFO, "Closing CEF browser in destructor");
            cefWidget_->closeBrowser();
            browserClosed_ = true;
        }
    }
}

void OneSevenLiveChatWidget::shutdown() {
    obs_log(LOG_INFO, "OneSevenLiveChatWidget shutdown called");
    if (cefWidget_ && !browserClosed_) {
        int panel_version = obs_browser_qcef_version();
        if (panel_version >= 2) {
            obs_log(LOG_INFO, "Closing CEF browser in shutdown");
            cefWidget_->closeBrowser();
            browserClosed_ = true;
        }
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
    obs_log(LOG_INFO, "OneSevenLiveChatWidget showEvent");
    QWidget::showEvent(event);

    if (cefWidget_) {
        cefWidget_->setVisible(true);
    }
    if (loadingOverlay && loadingOverlay->isVisible()) {
        loadingOverlay->raise();
    }
}

void OneSevenLiveChatWidget::hideEvent(QHideEvent* event) {
    obs_log(LOG_INFO, "OneSevenLiveChatWidget hideEvent");
    QWidget::hideEvent(event);
    if (cefWidget_) {
        cefWidget_->setVisible(false);
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

void OneSevenLiveChatWidget::onGiftsLoaded() {
    if (loadingOverlay) {
        loadingOverlay->hide();
    }
}
