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
    obs_log(LOG_INFO, "QCefWidget::initializeCef");
    if (!browserCreated_) {
        createBrowser();
    }
}

void QCefWidget::createBrowser()
{
    CefWindowInfo window_info;
    
    // 针对不同操作系统进行不同处理
#ifdef Q_OS_WIN
    window_info.SetAsChild(getWindowHandle(), {0, 0, width(), height()});
#else
    // macOS 环境下需要进行类型转换
    window_info.SetAsChild(reinterpret_cast<void*>(getWindowHandle()), {0, 0, width(), height()});
#endif

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
    obs_log(LOG_INFO, "Browser creation initiated");
}

void QCefWidget::loadUrl(const QString& url)
{
    pendingUrl_ = url;
    if (handler_) {
        CefRefPtr<CefBrowser> browser = handler_->GetBrowser();
        if (browser) {
            CefString cef_url(url.toStdString());
            browser->GetMainFrame()->LoadURL(cef_url);
            obs_log(LOG_INFO, "URL loaded immediately: %s", url.toStdString().c_str());
        } else {
            obs_log(LOG_WARNING, "CEF browser not ready, will load URL after creation: %s", url.toStdString().c_str());
        }
    }
}

void QCefWidget::onBrowserCreated()
{
    if (!pendingUrl_.isEmpty()) {
        CefRefPtr<CefBrowser> browser = handler_->GetBrowser();
        if (browser) {
            CefString cef_url(pendingUrl_.toStdString());
            browser->GetMainFrame()->LoadURL(cef_url);
            obs_log(LOG_INFO, "URL loaded after browser creation: %s", pendingUrl_.toStdString().c_str());
        }
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
