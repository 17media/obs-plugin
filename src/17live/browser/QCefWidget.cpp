#include "QCefWidget.hpp"
#include <QResizeEvent>
#include "CefHandler.hpp"
#include "plugin-support.h"
#include <obs-module.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#endif



extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);

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
    
    // Create the handler after QWidget initia lization is complete
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
    // 防止重复创建
    if (browserCreated_) {
        obs_log(LOG_WARNING, "Browser already created");
        return;
    }

    // 获取平台相关窗口句柄和尺寸
    WId handle = getWindowHandle();  // 假设此函数已返回原生窗口句柄
    QSize size = this->size();
    size *= devicePixelRatioF();  // 支持高分屏

    // 启动异步任务到 CEF 主线程
    bool success = QueueCEFTask([this, handle, size]() {
        // 防止二次创建
        if (cefBrowser_)
            return;

        CefWindowInfo window_info;

#if CHROME_VERSION_BUILD >= 6533
        window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
#endif

#ifdef Q_OS_WIN
        RECT rc = { 0, 0, size.width(), size.height() };
        window_info.SetAsChild((CefWindowHandle)handle, rc);
#elif defined(Q_OS_MAC)
        int width = size.width();
        int height = size.height();
        obs_log(LOG_INFO, "Creating CEF browser with size: %d x %d", width, height);
        if (width < 378) width = 378;
        if (height < 600) height = 600;

        window_info.SetAsChild((CefWindowHandle)handle, CefRect(0, 0, width, height));
#else
        CefRect rc(0, 0, size.width(), size.height());
        window_info.SetAsChild((CefWindowHandle)handle, rc);
#endif

        CefBrowserSettings browser_settings;
        CefString url; // 加载空白页面

        cefBrowser_ = CefBrowserHost::CreateBrowserSync(
            window_info,
            handler_.get(),
            url,
            browser_settings,
            nullptr,
            nullptr
        );

        if (!cefBrowser_) {
            obs_log(LOG_ERROR, "Failed to create CEF browser");
            return;
        }

#ifdef __linux__
        QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
#endif

        obs_log(LOG_INFO, "CEF browser created");
    });

    if (success) {
        browserCreated_ = true;

#ifndef Q_OS_MAC
        // 用 QWidget 封装原生窗口（仅非 macOS）
        if (!container_) {
            container_ = QWidget::createWindowContainer(window_, this);
            container_->show();
        }

        resize(size);  // 同步调整 Qt 显示控件大小
#endif
    } else {
        obs_log(LOG_ERROR, "Failed to queue CEF creation task");
    }
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

    if (!(handler_ && handler_->GetBrowser()))
        return;

    CefRefPtr<CefBrowserHost> host = handler_->GetBrowser()->GetHost();
    CefWindowHandle handle = host->GetWindowHandle();

    if (!handle)
        return;

#ifdef Q_OS_WIN
    QSize size = this->size() * devicePixelRatioF(); // 支持高分屏
    
    SetWindowPos((HWND)handle, nullptr,
                 0, 0,
                 size.width(), size.height(),
                 SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);

    SendMessage((HWND)handle, WM_SIZE, 0, MAKELPARAM(size.width(), size.height()));

#elif defined(Q_OS_LINUX)
    Display* xDisplay = cef_get_xdisplay();
    if (!xDisplay)
        return;

    XWindowChanges changes = {0};
    changes.x = 0;
    changes.y = 0;
    changes.width = size.width();
    changes.height = size.height();

    XConfigureWindow(xDisplay, (Window)handle,
                     CWX | CWY | CWWidth | CWHeight,
                     &changes);

#if CHROME_VERSION_BUILD >= 4638
    XSync(xDisplay, false);
#endif

    host->WasResized();  // 通知 CEF 内容也应刷新布局

#else  // macOS 或其他
    host->WasResized();
#endif
}

void QCefWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!browserCreated_) {
		// obs_browser_initialize();
		// connect(&timer, &QTimer::timeout, this, &QCefWidget::createBrowser);
		// timer.start(500);
		createBrowser();
	}
}

void QCefWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // 你可以在这里添加自己的逻辑
}
