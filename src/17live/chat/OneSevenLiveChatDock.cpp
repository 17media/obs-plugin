#include "OneSevenLiveChatDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QEvent>
#include <QHideEvent>
#include <QApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>

#include "../OneSevenLiveCoreManager.hpp"
#include "../../plugin-support.h"
#include "moc_OneSevenLiveChatDock.cpp"

OneSevenLiveChatDock::OneSevenLiveChatDock(const QString& title, const QString& chatUrl,
                                           QWidget* parent)
    : QDockWidget(title, parent), title_(title), chatUrl_(chatUrl) {
    setAttribute(Qt::WA_NativeWindow);

    createBrowser(chatUrl_);

    loadingOverlay_ = new QWidget(this);
    loadingOverlay_->setStyleSheet("background-color: rgba(0, 0, 0, 180);");

    QVBoxLayout* overlayLayout = new QVBoxLayout(loadingOverlay_);
    overlayLayout->setAlignment(Qt::AlignCenter);

    loadingLabel_ = new QLabel(obs_module_text("ChatRoom.LoadingGifts"), loadingOverlay_);
    loadingLabel_->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    overlayLayout->addWidget(loadingLabel_);

    loadingOverlay_->raise();
    updateOverlayGeometry();

    auto& core = OneSevenLiveCoreManager::getInstance();
    connect(&core, &OneSevenLiveCoreManager::giftsLoaded, this, &OneSevenLiveChatDock::onGiftsLoaded);

    if (core.isGiftsLoaded()) {
        loadingOverlay_->hide();
    } else {
        loadingOverlay_->show();
    }
}

OneSevenLiveChatDock::~OneSevenLiveChatDock() = default;

QCef* OneSevenLiveChatDock::getOrCreateSharedCef() const {
    static QCef* shared = nullptr;
    if (shared) {
        return shared;
    }

    shared = obs_browser_init_panel();
    if (!shared) {
        return nullptr;
    }

    if (!shared->initialized()) {
        shared->init_browser();
        shared->wait_for_browser_init();
    }

    return shared;
}

int OneSevenLiveChatDock::qcefVersion() const {
    return obs_browser_qcef_version();
}

void OneSevenLiveChatDock::createBrowser(const QString& url) {
    chatUrl_ = url;

    if (cefWidget_) {
        cefWidget_->setURL(url.toStdString());
        return;
    }

    QCef* cef = getOrCreateSharedCef();
    if (!cef) {
        obs_log(LOG_ERROR, "OneSevenLiveChatDock: obs-browser panel is not available");
        return;
    }

    QCefWidget* widget = cef->create_widget(this, url.toStdString(), nullptr);
    if (!widget) {
        if (!errorLabel_) {
            errorLabel_ = new QLabel("Browser source not available", this);
            errorLabel_->setAlignment(Qt::AlignCenter);
            errorLabel_->setStyleSheet("background-color: #202020; color: white;");
        }
        obs_log(LOG_ERROR, "OneSevenLiveChatDock: Failed to create QCefWidget");
        updateOverlayGeometry();
        return;
    }

    cefWidget_.reset(widget);
    setWidget(cefWidget_.data());

    if (qcefVersion() >= 1) {
        cefWidget_->allowAllPopups(true);
    }

    if (errorLabel_) {
        errorLabel_->hide();
    }
    updateOverlayGeometry();
}

void OneSevenLiveChatDock::updateOverlayGeometry() {
    const QRect area = contentsRect();
    if (loadingOverlay_) {
        loadingOverlay_->setGeometry(area);
    }
    if (errorLabel_) {
        errorLabel_->setGeometry(area);
    }
}

void OneSevenLiveChatDock::shutdownBrowser() {
    if (!cefWidget_) {
        return;
    }

    // Match OBS browser-panel guidance: decouple the browser widget from the
    // dock before closing, so CEF does not treat the root window as the host.
    QWidget* browserWidget = cefWidget_.data();
    if (widget() == browserWidget) {
        setWidget(nullptr);
    }
    browserWidget->hide();
    browserWidget->setParent(nullptr);

    if (qcefVersion() >= 2) {
        cefWidget_->closeBrowser();
    }
}

void OneSevenLiveChatDock::setUrl(const QString& url) {
    chatUrl_ = url;

    if (!cefWidget_) {
        createBrowser(url);
        return;
    }

    cefWidget_->setURL(url.toStdString());
}

void OneSevenLiveChatDock::reload() {
    if (cefWidget_) {
        cefWidget_->reloadPage();
    }
}

void OneSevenLiveChatDock::prepareForDelete() {
    if (deleting_) {
        return;
    }

    deleting_ = true;
}

void OneSevenLiveChatDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    setWindowTitle(title_);
    if (!cefWidget_ && !deleting_) {
        createBrowser(chatUrl_);
    }
    if (cefWidget_) {
        cefWidget_->setVisible(true);
    }
    if (loadingOverlay_ && loadingOverlay_->isVisible()) {
        loadingOverlay_->raise();
    }
    if (errorLabel_ && errorLabel_->isVisible()) {
        errorLabel_->raise();
    }
    updateOverlayGeometry();
}

void OneSevenLiveChatDock::hideEvent(QHideEvent* event) {
    QDockWidget::hideEvent(event);
    if (cefWidget_) {
        cefWidget_->setVisible(false);
    }
}

void OneSevenLiveChatDock::closeEvent(QCloseEvent* event) {
    QDockWidget::closeEvent(event);

    if (event && event->isAccepted() && widget()) {
        QEvent widgetEvent(QEvent::Type(QEvent::User + QEvent::Close));
        qApp->sendEvent(widget(), &widgetEvent);
    }

    if (event && event->isAccepted() && cefWidget_) {
        shutdownBrowser();

        if (!deleting_) {
            // Manual close keeps the dock instance around, so recreate the browser next time it opens.
            cefWidget_.reset(nullptr);
        }
    }
}

void OneSevenLiveChatDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
    updateOverlayGeometry();
}

void OneSevenLiveChatDock::onGiftsLoaded() {
    if (loadingOverlay_) {
        loadingOverlay_->hide();
    }
}
