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

#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>
#include <functional>

class SimpleCefClient : public CefClient, public CefLifeSpanHandler, public CefDisplayHandler {
   public:
    SimpleCefClient() {}

    CefRefPtr<CefBrowser> getBrowser() const {
        return m_browser;
    }

    // Callback to notify URL changes (Qt bridge sets this)
    void setUrlChangedCallback(const std::function<void(QString)> &cb) {
        onUrlChanged_ = cb;
    }

    // CefClient interface implementation
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }

    // CefLifeSpanHandler interface implementation
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefDisplayHandler
    virtual void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                 const CefString &url) override;

   private:
    CefRefPtr<CefBrowser> m_browser;
    std::function<void(QString)> onUrlChanged_;

    // CEF reference counting implementation
    IMPLEMENT_REFCOUNTING(SimpleCefClient);
};
