#pragma once

// #include <include/cef_client.h>
#include "cef-headers.hpp"
#include <QObject>

namespace seventeenlive {

class CefHandler : 
    public QObject,
    public CefClient,
    public CefLifeSpanHandler,
    public CefDisplayHandler,
    public CefLoadHandler {
    Q_OBJECT

public:
    explicit CefHandler(QObject* parent = nullptr);

    // fetch browser
    CefRefPtr<CefBrowser> GetBrowser() { return browser_; }
    
    // CefClient接口
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

    // CefLifeSpanHandler接口
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefDisplayHandler接口
    virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) override;

signals:
    void loadingStateChanged(bool isLoading);

private:
    CefRefPtr<CefBrowser> browser_;

    IMPLEMENT_REFCOUNTING(CefHandler);
};

} // namespace seventeenlive
