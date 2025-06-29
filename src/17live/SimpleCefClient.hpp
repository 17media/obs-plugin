#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/cef_client.h>
#include <include/cef_render_handler.h>
#include <include/wrapper/cef_helpers.h>

#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

class SimpleCefClient : public CefClient, public CefLifeSpanHandler {
   public:
    SimpleCefClient() {}

    CefRefPtr<CefBrowser> getBrowser() const {
        return m_browser;
    }

    // CefClient interface implementation
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    // CefLifeSpanHandler interface implementation
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

   private:
    CefRefPtr<CefBrowser> m_browser;

    // CEF reference counting implementation
    IMPLEMENT_REFCOUNTING(SimpleCefClient);
};
