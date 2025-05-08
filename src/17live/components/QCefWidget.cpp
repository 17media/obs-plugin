#include "QCefWidget.hpp"
#include <QResizeEvent>
// #include <include/cef_browser.h>
// #include <include/cef_app.h>
#include "plugin-support.h"
#include <obs-module.h>

namespace seventeenlive {

QCefWidget::QCefWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    
    handler_ = std::make_unique<CefHandler>(this);
    initializeCef();
}

QCefWidget::~QCefWidget()
{
    // CEF清理代码
}

void QCefWidget::initializeCef()
{
    if (!browserCreated_ && isVisible()) {
        createBrowser();
    }
}

void QCefWidget::createBrowser()
{
    CefWindowInfo window_info;
    window_info.SetAsChild(parent(), {0, 0, width(), height()});

    CefBrowserSettings browser_settings;
    CefString url;  // 空URL，加载空白页面

    CefBrowserHost::CreateBrowser(
        window_info,
        handler_.get(),
        url,
        browser_settings,
        nullptr,
        nullptr
    );

    browserCreated_ = true;
}

void QCefWidget::loadUrl(const QString& url)
{
    if (handler_) {
        CefString cef_url(url.toStdString());
        handler_->GetBrowser()->GetMainFrame()->LoadURL(cef_url);
    }
}

WId QCefWidget::getWindowHandle() const
{
#ifdef Q_OS_WIN
    return (WId)winId();
#else
    return winId();
#endif
}

void QCefWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (handler_ && handler_->GetBrowser()) {
        CefWindowHandle hwnd = handler_->GetBrowser()->GetHost()->GetWindowHandle();
        if (hwnd) {
            // 调整CEF浏览器窗口大小
//            SetWindowPos(hwnd, nullptr,
//                        0, 0, width(), height(),
//                        SWP_NOZORDER);
            obs_log(LOG_INFO, "resize trigger");
        }
    }
}

void QCefWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // 你可以在这里添加自己的逻辑
}

void QCefWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // 你可以在这里添加自己的逻辑
}

} // namespace seventeenlive
