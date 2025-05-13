#pragma once

#include "cef-headers.hpp"

namespace seventeenlive {

class BrowserApp : public CefApp, public CefBrowserProcessHandler {
public:
    BrowserApp();
    // CefApp 接口实现
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }
    // CefBrowserProcessHandler 接口实现
    virtual void OnContextInitialized() override;
    // CefRefPtr<CefClient> GetDefaultClient() override;
    
    IMPLEMENT_REFCOUNTING(BrowserApp);
};

} // namespace seventeenlive
