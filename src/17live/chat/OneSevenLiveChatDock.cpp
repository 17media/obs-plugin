#include "OneSevenLiveChatDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QHideEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>

#include "../OneSevenLiveCoreManager.hpp"
#include "../ui/cef/CefWidgetHost.hpp"
#include "../../plugin-support.h"
#include "moc_OneSevenLiveChatDock.cpp"

OneSevenLiveChatDock::OneSevenLiveChatDock(const QString& title, const QString& chatUrl,
                                           QWidget* parent)
    : QDockWidget(title, parent), title_(title), chatUrl_(chatUrl) {
    obs_log(LOG_INFO, "OneSevenLiveChatDock constructed");

    setAttribute(Qt::WA_NativeWindow);

    contentWidget_ = new QWidget(this);
    contentWidget_->setContentsMargins(0, 0, 0, 0);
    setWidget(contentWidget_);

    browserContainer_ = new QWidget(contentWidget_);
    browserContainer_->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* rootLayout = new QVBoxLayout(contentWidget_);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    cefHost_ = std::make_unique<CefWidgetHost>();
    createBrowser(chatUrl_);

    loadingOverlay_ = new QWidget(contentWidget_);
    loadingOverlay_->setStyleSheet("background-color: rgba(0, 0, 0, 180);");

    QVBoxLayout* overlayLayout = new QVBoxLayout(loadingOverlay_);
    overlayLayout->setAlignment(Qt::AlignCenter);

    loadingLabel_ = new QLabel(obs_module_text("ChatRoom.LoadingGifts"), loadingOverlay_);
    loadingLabel_->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    overlayLayout->addWidget(loadingLabel_);

    loadingOverlay_->raise();

    auto& core = OneSevenLiveCoreManager::getInstance();
    connect(&core, &OneSevenLiveCoreManager::giftsLoaded, this, &OneSevenLiveChatDock::onGiftsLoaded);

    if (core.isGiftsLoaded()) {
        loadingOverlay_->hide();
    } else {
        loadingOverlay_->show();
    }
}

OneSevenLiveChatDock::~OneSevenLiveChatDock() {
    obs_log(LOG_INFO, "OneSevenLiveChatDock destructor called");
    prepareForDelete();
}

void OneSevenLiveChatDock::createBrowser(const QString& url) {
    if (!contentWidget_) {
        return;
    }

    chatUrl_ = url;

    if (!cefHost_) {
        cefHost_ = std::make_unique<CefWidgetHost>();
    }

    if (!cefHost_->ensureCreated(browserContainer_, url)) {
        if (!errorLabel_) {
            errorLabel_ = new QLabel("Browser source not available", contentWidget_);
            errorLabel_->setAlignment(Qt::AlignCenter);
            if (auto* layout = qobject_cast<QVBoxLayout*>(contentWidget_->layout())) {
                layout->addWidget(errorLabel_);
            }
        }
        obs_log(LOG_ERROR, "OneSevenLiveChatDock: Failed to create QCefWidget");
        return;
    }

    if (auto* layout = qobject_cast<QVBoxLayout*>(browserContainer_->layout())) {
        if (layout->indexOf(cefHost_->widget()) == -1) {
            layout->addWidget(cefHost_->widget());
        }
    } else {
        QVBoxLayout* browserLayout = new QVBoxLayout(browserContainer_);
        browserLayout->setContentsMargins(0, 0, 0, 0);
        browserLayout->setSpacing(0);
        browserLayout->addWidget(cefHost_->widget());
    }

    if (auto* layout = qobject_cast<QVBoxLayout*>(contentWidget_->layout())) {
        if (layout->indexOf(browserContainer_) == -1) {
            layout->addWidget(browserContainer_);
        }
    }

    if (errorLabel_) {
        errorLabel_->hide();
    }
}

void OneSevenLiveChatDock::shutdownBrowser() {
    if (cefHost_) {
        cefHost_->release(true);
        cefHost_.reset();
    }
}

void OneSevenLiveChatDock::setUrl(const QString& url) {
    chatUrl_ = url;

    if (!cefHost_ || !cefHost_->widget()) {
        createBrowser(url);
        return;
    }

    cefHost_->setUrl(url);
}

void OneSevenLiveChatDock::reload() {
    if (cefHost_) {
        cefHost_->reload();
    }
}

void OneSevenLiveChatDock::prepareForDelete() {
    if (deleting_) {
        obs_log(LOG_INFO, "OneSevenLiveChatDock prepareForDelete skipped: already deleting");
        return;
    }

    deleting_ = true;
    obs_log(LOG_INFO, "OneSevenLiveChatDock prepareForDelete");
    shutdownBrowser();
}

void OneSevenLiveChatDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    setWindowTitle(title_);
    if (!cefHost_ && !deleting_) {
        createBrowser(chatUrl_);
    }
    if (cefHost_ && cefHost_->widget()) {
        cefHost_->widget()->setVisible(true);
    }
    if (loadingOverlay_ && loadingOverlay_->isVisible()) {
        loadingOverlay_->raise();
    }
}

void OneSevenLiveChatDock::hideEvent(QHideEvent* event) {
    QDockWidget::hideEvent(event);
    if (cefHost_ && cefHost_->widget()) {
        cefHost_->widget()->setVisible(false);
    }
}

void OneSevenLiveChatDock::closeEvent(QCloseEvent* event) {
    if (!deleting_) {
        obs_log(LOG_INFO, "OneSevenLiveChatDock closeEvent: releasing browser for later recreate");
        shutdownBrowser();
    }
    QDockWidget::closeEvent(event);
}

void OneSevenLiveChatDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    if (contentWidget_) {
        if (loadingOverlay_) {
            loadingOverlay_->setGeometry(contentWidget_->contentsRect());
        }
        if (errorLabel_) {
            errorLabel_->setGeometry(contentWidget_->contentsRect());
        }
    }
}

void OneSevenLiveChatDock::onGiftsLoaded() {
    if (loadingOverlay_) {
        loadingOverlay_->hide();
    }
}
