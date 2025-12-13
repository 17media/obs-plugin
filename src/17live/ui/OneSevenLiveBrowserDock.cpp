#include "OneSevenLiveBrowserDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QShowEvent>
#include <QVBoxLayout>

#include "../../plugin-support.h"
#include "../chat/cef_panel.hpp"
#include "moc_OneSevenLiveBrowserDock.cpp"

OneSevenLiveBrowserDock::OneSevenLiveBrowserDock(QWidget* parent, const QString& title)
    : QDockWidget(title.isEmpty() ? QStringLiteral("Browser") : title, parent) {
    setObjectName("OneSevenLiveBrowserDock");
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);

    container_ = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setWidget(container_);

    cef_ = obs_browser_init_panel();
    if (cef_) {
        if (!cef_->initialized()) {
            cef_->init_browser();
            cef_->wait_for_browser_init();
        }
        panelCookies_ = cef_->create_cookie_manager("onesevenlive-dock", false);
    } else {
        obs_log(LOG_WARNING, "OneSevenLiveBrowserDock: obs-browser panel unavailable");
    }
}

OneSevenLiveBrowserDock::~OneSevenLiveBrowserDock() {
    destroyBrowser(true);
    if (panelCookies_) {
        panelCookies_->FlushStore();
        delete panelCookies_;
        panelCookies_ = nullptr;
    }
}

void OneSevenLiveBrowserDock::createBrowser(const QString& url) {
    if (!cef_)
        return;
    currentUrl_ = url;
    if (cefWidget_) {
        cefWidget_->setURL(url.toStdString());
        return;
    }
    cefWidget_ = cef_->create_widget(container_, url.toStdString(), panelCookies_);
    if (cefWidget_) {
        int panel_version = obs_browser_qcef_version();
        if (panel_version >= 1) {
            cefWidget_->allowAllPopups(true);
        }
        container_->layout()->addWidget(cefWidget_);
        connect(cefWidget_, SIGNAL(urlChanged(const QString&)), this,
                SIGNAL(urlChanged(const QString&)));
        cefWidget_->show();
    } else {
        obs_log(LOG_ERROR, "OneSevenLiveBrowserDock: Failed to create QCefWidget");
    }
}

void OneSevenLiveBrowserDock::destroyBrowser(bool fullCleanup) {
    if (!cefWidget_)
        return;
    int panel_version = obs_browser_qcef_version();
    if (panel_version >= 2 && !browserClosed_) {
        cefWidget_->closeBrowser();
        browserClosed_ = true;
    }
    if (fullCleanup) {
        cefWidget_->deleteLater();
        cefWidget_ = nullptr;
    }
}

void OneSevenLiveBrowserDock::setUrl(const QString& url) {
    if (!cefWidget_) {
        createBrowser(url);
        return;
    }
    cefWidget_->setURL(url.toStdString());
    currentUrl_ = url;
}

void OneSevenLiveBrowserDock::reload() {
    if (cefWidget_) {
        cefWidget_->reloadPage();
    }
}

void OneSevenLiveBrowserDock::setStartupScript(const QString& script) {
    if (cefWidget_) {
        cefWidget_->setStartupScript(script.toStdString());
    }
}

void OneSevenLiveBrowserDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    if (!cefWidget_ && !currentUrl_.isEmpty()) {
        createBrowser(currentUrl_);
    }
    if (cefWidget_) {
        cefWidget_->setVisible(true);
    }
}

void OneSevenLiveBrowserDock::hideEvent(QHideEvent* event) {
    QDockWidget::hideEvent(event);
    if (cefWidget_) {
        cefWidget_->setVisible(false);
    }
}

void OneSevenLiveBrowserDock::closeEvent(QCloseEvent* event) {
    destroyBrowser(false);
    emit dockClosed();
    QDockWidget::closeEvent(event);
}

void OneSevenLiveBrowserDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
}

