#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <QWidget>
#include <QWindow>
#include <QVBoxLayout>
#include <include/cef_client.h>
#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/cef_render_handler.h>
#include <include/wrapper/cef_helpers.h>

class QCefView : public QWidget, public CefClient, public CefRenderHandler, public CefLifeSpanHandler, public CefDisplayHandler
{
    Q_OBJECT

public:
    explicit QCefView(QWidget *parent = nullptr);
    ~QCefView();

    // Load URL
    void loadUrl(const QString &url);
    // Get current URL
    QString currentUrl() const;
    // Go back to previous page
    void back();
    // Go forward to next page
    void forward();
    // Refresh page
    void reload();
    // Stop loading
    void stopLoad();

    // CefClient interface implementation
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

    // CefRenderHandler interface implementation
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;
    virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, 
                        const RectList &dirtyRects, const void *buffer, 
                        int width, int height) override;

    // CefLifeSpanHandler interface implementation
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefDisplayHandler interface implementation
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString &title) override;
    virtual void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &url) override;

signals:
    void titleChanged(const QString &title);
    void urlChanged(const QString &url);

protected:
    virtual void resizeEvent(QResizeEvent *event) override;

private:
    CefRefPtr<CefBrowser> m_browser;
    QWindow *m_window;
    QWidget *m_container;
    QVBoxLayout *m_layout;
    QString m_currentUrl;

    // CEF reference counting implementation
    IMPLEMENT_REFCOUNTING(QCefView);
};
