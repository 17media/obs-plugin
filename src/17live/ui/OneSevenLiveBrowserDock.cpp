#include "OneSevenLiveBrowserDock.hpp"

#include <obs-module.h>

#include <QCloseEvent>
#include <QShowEvent>
#include <QVBoxLayout>

#include "../../plugin-support.h"
#include "cef/CefWidgetHost.hpp"
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

    cefHost_ = std::make_unique<CefWidgetHost>();
    cefHost_->setCookieStorage("onesevenlive-dock", false);
}

OneSevenLiveBrowserDock::~OneSevenLiveBrowserDock() {
    destroyBrowser(true);
    cefHost_.reset();
}

void OneSevenLiveBrowserDock::createBrowser(const QString& url) {
    if (!cefHost_ || !cefHost_->available())
        return;
    currentUrl_ = url;
    if (cefHost_->widget()) {
        cefHost_->setUrl(url);
        return;
    }

    if (cefHost_->ensureCreated(container_, url)) {
        container_->layout()->addWidget(cefHost_->widget());
        connect(cefHost_->widget(), SIGNAL(urlChanged(const QString&)), this,
                SIGNAL(urlChanged(const QString&)));
        cefHost_->widget()->show();
    } else {
        obs_log(LOG_ERROR, "OneSevenLiveBrowserDock: Failed to create QCefWidget");
    }
}

void OneSevenLiveBrowserDock::destroyBrowser(bool fullCleanup) {
    if (!cefHost_ || !cefHost_->widget())
        return;
    if (fullCleanup) {
        cefHost_->release(true);
    }
}

void OneSevenLiveBrowserDock::setUrl(const QString& url) {
    if (!cefHost_ || !cefHost_->widget()) {
        createBrowser(url);
        return;
    }
    cefHost_->setUrl(url);
    currentUrl_ = url;
}

void OneSevenLiveBrowserDock::reload() {
    if (cefHost_)
        cefHost_->reload();
}

void OneSevenLiveBrowserDock::setStartupScript(const QString& script) {
    if (cefHost_)
        cefHost_->setStartupScript(script);
}

void OneSevenLiveBrowserDock::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    if ((!cefHost_ || !cefHost_->widget()) && !currentUrl_.isEmpty()) {
        createBrowser(currentUrl_);
    }
    if (cefHost_ && cefHost_->widget()) {
        cefHost_->widget()->setVisible(true);
    }
}

void OneSevenLiveBrowserDock::hideEvent(QHideEvent* event) {
    QDockWidget::hideEvent(event);
    if (cefHost_ && cefHost_->widget()) {
        cefHost_->widget()->setVisible(false);
    }
}

void OneSevenLiveBrowserDock::closeEvent(QCloseEvent* event) {
    // Match OBS YouTube dock style: close path detaches browser widget first.
    destroyBrowser(true);
    emit dockClosed();
    QDockWidget::closeEvent(event);
}

void OneSevenLiveBrowserDock::resizeEvent(QResizeEvent* event) {
    QDockWidget::resizeEvent(event);
}
