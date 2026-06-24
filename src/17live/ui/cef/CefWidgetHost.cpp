#include "CefWidgetHost.hpp"

#include <QCoreApplication>
#include <QLayout>
#include <QWidget>

CefWidgetHost::CefWidgetHost() = default;

CefWidgetHost::~CefWidgetHost() {
    release(true);
    if (cookieManager_) {
        cookieManager_->FlushStore();
        delete cookieManager_;
        cookieManager_ = nullptr;
    }
}

bool CefWidgetHost::available() const {
    return getOrCreateSharedCef() != nullptr;
}

int CefWidgetHost::qcefVersion() const {
    return obs_browser_qcef_version();
}

void CefWidgetHost::setCookieStorage(const QString& storageKey, bool persistSessionCookies) {
    cookieStorageKey_ = storageKey;
    persistSessionCookies_ = persistSessionCookies;
}

QCefWidget* CefWidgetHost::widget() const {
    return widget_.data();
}

bool CefWidgetHost::ensureCreated(QWidget* parent, const QString& url) {
    if (widget_) {
        setUrl(url);
        return true;
    }

    cef_ = getOrCreateSharedCef();
    if (!cef_ || !parent) {
        return false;
    }

    QCefCookieManager* cookies = ensureCookieManager();
    QCefWidget* w = cef_->create_widget(parent, url.toStdString(), cookies);
    if (!w) {
        return false;
    }
    widget_.reset(w);

    browserClosed_ = false;
    releasing_ = false;
    released_ = false;
    if (qcefVersion() >= 1) {
        widget_->allowAllPopups(true);
    }
    widget_->setVisible(true);
    return true;
}

void CefWidgetHost::setUrl(const QString& url) {
    if (widget_) {
        widget_->setURL(url.toStdString());
    }
}

void CefWidgetHost::reload() {
    if (widget_) {
        widget_->reloadPage();
    }
}

void CefWidgetHost::setStartupScript(const QString& script) {
    if (widget_) {
        widget_->setStartupScript(script.toStdString());
    }
}

void CefWidgetHost::release(bool requestCloseBrowser) {
    if (!widget_) {
        return;
    }
    if (releasing_ || released_) {
        return;
    }

    releasing_ = true;

    if (requestCloseBrowser && !browserClosed_ && qcefVersion() >= 2) {
        widget_->closeBrowser();
        browserClosed_ = true;
    }

    widget_->setVisible(false);
    widget_.reset(nullptr);
    released_ = true;
    releasing_ = false;
}

QCef* CefWidgetHost::getOrCreateSharedCef() {
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

QCefCookieManager* CefWidgetHost::ensureCookieManager() {
    if (cookieStorageKey_.isEmpty()) {
        return nullptr;
    }
    if (cookieManager_) {
        return cookieManager_;
    }
    if (!available()) {
        return nullptr;
    }
    cookieManager_ =
        cef_->create_cookie_manager(cookieStorageKey_.toStdString(), persistSessionCookies_);
    return cookieManager_;
}
