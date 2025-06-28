#include "QCefView.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>  // For os_event_t, etc.
#include <util/threading.h>

#include <obs.hpp>
#include <util/dstr.hpp>  // For DStr

#include <QResizeEvent>
#include <QWindow>
#include <QDebug>
#include <QScreen>
#include <QTimer>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Windowsx.h>
#endif

#include "plugin-support.h"

#include "moc_QCefView.cpp"

QCefView::QCefView(QWidget *parent) : QWidget(parent), m_browser(nullptr)
{
    // 創建佈局
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    // 創建窗口容器
    m_window = new QWindow();
    m_container = QWidget::createWindowContainer(m_window, this);
    m_layout->addWidget(m_container);

    // 設置窗口屬性
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setFocusPolicy(Qt::StrongFocus);
}

QCefView::~QCefView()
{ 
    if (m_browser)
    {
        m_browser->GetHost()->CloseBrowser(true);
        m_browser = nullptr;
    }
}

void QCefView::loadUrl(const QString &url)
{
    m_currentUrl = url;
    if (m_browser)
    {
        CefString cefUrl(url.toStdString());
        m_browser->GetMainFrame()->LoadURL(cefUrl);
    }
    else
    {
        // 確保窗口已經有正確的大小，但只在widget已經有有效的parent和screen時調用
        adjustSize();
        
        // 使用QTimer延遲創建瀏覽器，確保窗口大小已經正確設置
        QTimer::singleShot(100, this, [this, url]() {
            // 創建瀏覽器窗口
            CefWindowInfo windowInfo;
            
#ifdef Q_OS_WIN
            // 獲取設備像素比例以支持高DPI
            QScreen* currentScreen = screen();
            qreal devicePixelRatio = currentScreen ? currentScreen->devicePixelRatio() : 1.0;
            int scaledWidth = width() * devicePixelRatio;
            int scaledHeight = height() * devicePixelRatio;
            
            windowInfo.SetAsChild((CefWindowHandle)m_window->winId(), 
                                CefRect(0, 0, scaledWidth, scaledHeight));
#else
            windowInfo.SetAsChild((CefWindowHandle)m_window->winId(), 
                                CefRect(0, 0, width(), height()));
#endif

            CefBrowserSettings browserSettings;
            // browserSettings.background_color = CefColorSetARGB(255, 255, 255, 255);
            
            // 啟用高DPI支持
            // browserSettings.windowless_frame_rate = 60;

            CefString cefUrl(url.toStdString());
            CefBrowserHost::CreateBrowser(windowInfo, this, cefUrl, browserSettings, nullptr, nullptr);
            
            // 確保瀏覽器窗口充滿整個容器
            QTimer::singleShot(200, this, [this]() {
                if (m_browser) {
                    resizeEvent(nullptr);
                }
            });
        });
    }
}

QString QCefView::currentUrl() const
{
    return m_currentUrl;
}

void QCefView::back()
{
    if (m_browser && m_browser->CanGoBack())
    {
        m_browser->GoBack();
    }
}

void QCefView::forward()
{
    if (m_browser && m_browser->CanGoForward())
    {
        m_browser->GoForward();
    }
}

void QCefView::reload()
{
    if (m_browser)
    {
        m_browser->Reload();
    }
}

void QCefView::stopLoad()
{
    if (m_browser)
    {
        m_browser->StopLoad();
    }
}

void QCefView::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
{
    rect.x = 0;
    rect.y = 0;
    
#ifdef Q_OS_WIN
    // 獲取設備像素比例以支持高DPI
    QScreen* currentScreen = screen();
    qreal devicePixelRatio = currentScreen ? currentScreen->devicePixelRatio() : 1.0;
    rect.width = width() * devicePixelRatio;
    rect.height = height() * devicePixelRatio;
#else
    rect.width = width();
    rect.height = height();
#endif
}

void QCefView::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, 
                      const RectList &dirtyRects, const void *buffer, 
                      int width, int height)
{
    // 在離屏渲染模式下需要實現，但我們使用的是窗口渲染模式，所以這裡不需要實現
}

void QCefView::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();

    if (!m_browser) {
        m_browser = browser;
    }
}

bool QCefView::DoClose(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();

    if (m_browser && m_browser->GetIdentifier() == browser->GetIdentifier()) {
        m_browser = nullptr;
    }

    return false;
}

void QCefView::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();

    if (m_browser && m_browser->GetIdentifier() == browser->GetIdentifier()) {
        m_browser = nullptr;
    }
}

void QCefView::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString &title)
{
    QString qTitle = QString::fromStdString(title.ToString());
    emit titleChanged(qTitle);
}

void QCefView::OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &url)
{
    if (frame->IsMain())
    {
        m_currentUrl = QString::fromStdString(url.ToString());
        emit urlChanged(m_currentUrl);
    }
}

void QCefView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_browser)
    {
        CefWindowHandle hwnd = m_browser->GetHost()->GetWindowHandle();
        if (hwnd)
        {
#ifdef Q_OS_WIN
            // 獲取設備像素比例以支持高DPI
            QScreen* currentScreen = screen();
            qreal devicePixelRatio = currentScreen ? currentScreen->devicePixelRatio() : 1.0;
            
            // 調整CEF瀏覽器窗口大小
            RECT rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = width() * devicePixelRatio;
            rect.bottom = height() * devicePixelRatio;
            
            // 使用Windows API調整窗口大小
            HDWP hdwp = BeginDeferWindowPos(1);
            hdwp = DeferWindowPos(hdwp, hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
            EndDeferWindowPos(hdwp);
#else
            // 非Windows平台的處理
            m_browser->GetHost()->WasResized();
#endif
        }
    }
}
