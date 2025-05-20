#include "QCefWidget.hpp"
#include <QResizeEvent>
#include "CefHandler.hpp"
#include "plugin-support.h"
#include <obs-module.h>

QCefWidget::QCefWidget(QWidget* parent)
    : QWidget(parent)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setFocusPolicy(Qt::ClickFocus);

#ifndef __APPLE__
	window = new QWindow();
	window->setFlags(Qt::FramelessWindowHint);
#endif
    
    handler_ = std::make_unique<CefHandler>(this);
    initializeCef();
}

QCefWidget::~QCefWidget()
{
    if (handler_) {
        CefRefPtr<CefBrowser> browser = handler_->GetBrowser();
        if (browser) {
            CefRefPtr<CefBrowserHost> host = browser->GetHost();
            if (host) {
                // 请求关闭浏览器，false表示非强制关闭，允许浏览器执行清理操作
                host->CloseBrowser(false);
            }
        }
    }
    // handler_ (std::unique_ptr) 会在 QCefWidget 销毁时自动删除，
    // 其析构函数（如果CefHandler有）应该处理CefClient和CefLifeSpanHandler等资源的释放。
    // CefLifeSpanHandler::OnBeforeClose() 是实际进行浏览器对象销毁的地方。
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

    QWidget* parent = parentWidget();

    // 针对不同操作系统进行不同处理
#ifdef Q_OS_WIN
    window_info.SetAsChild(getWindowHandle(), {0, 0, width(), height()});
#else
    // 设置宽度和高度，确保width最少为376px，height最少为600px
    int width = 376;
    int height = 600;
    if (parent->width() > width) {
        width = parent->width();
    }
    if (parent->height() > height) {
        height = parent->height();
    }

    // macOS 环境下需要进行类型转换
    window_info.SetAsChild(reinterpret_cast<void*>(getWindowHandle()), {0, 0, width, height});
#endif

    CefBrowserSettings browser_settings;
    CefString url;  // 空URL，加载空白页面

    if (!CefBrowserHost::CreateBrowser(
        window_info,
        handler_.get(),
        url,
        browser_settings,
        nullptr,
        nullptr
    )) {
        obs_log(LOG_ERROR, "Failed to create CEF browser");
        return;
    }

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
#ifdef Q_OS_WIN
        CefWindowHandle hwnd = handler_->GetBrowser()->GetHost()->GetWindowHandle();
        if (hwnd) {
            // 调整CEF浏览器窗口大小 (Windows specific)
           SetWindowPos(hwnd, nullptr,
                       0, 0, width(), height(),
                       SWP_NOZORDER);
            // obs_log(LOG_INFO, "resize trigger for Windows");
        }
#else
        // 通知CEF浏览器窗口大小已更改 (macOS, Linux, etc.)
        handler_->GetBrowser()->GetHost()->WasResized();
        // obs_log(LOG_INFO, "resize trigger for non-Windows");
#endif
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
