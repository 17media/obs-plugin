#pragma once

#include "cef-headers.hpp"

namespace seventeenlive {

class BrowserApp : public CefApp, public CefBrowserProcessHandler {
public:
    BrowserApp() {}
    // CefApp 接口实现
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }
    // CefBrowserProcessHandler 接口实现
    virtual void OnContextInitialized() override;
    virtual void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line) override;
    // virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> extra_info) override;

    IMPLEMENT_REFCOUNTING(BrowserApp);
};

} // namespace seventeenlive
