#pragma once

#include <QScopedPointer>
#include <QPointer>
#include <QString>

class QLayout;
class QWidget;

#include "../../chat/cef_panel.hpp"

class CefWidgetHost {
   public:
    CefWidgetHost();
    ~CefWidgetHost();

    CefWidgetHost(const CefWidgetHost&) = delete;
    CefWidgetHost& operator=(const CefWidgetHost&) = delete;

    bool available() const;
    int qcefVersion() const;

    void setCookieStorage(const QString& storageKey, bool persistSessionCookies);
    QCefWidget* widget() const;

    bool ensureCreated(QWidget* parent, const QString& url);
    void setUrl(const QString& url);
    void reload();
    void setStartupScript(const QString& script);

    void release(bool requestCloseBrowser);

   private:
    QCef* cef_{nullptr};
    QScopedPointer<QCefWidget> widget_{nullptr};
    QCefCookieManager* cookieManager_{nullptr};
    bool browserClosed_{false};
    QString cookieStorageKey_;
    bool persistSessionCookies_{false};

    static QCef* getOrCreateSharedCef();
    QCefCookieManager* ensureCookieManager();
};
