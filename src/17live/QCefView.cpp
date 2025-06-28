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
    // Create layout
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    // Create window container
    m_window = new QWindow();
    m_container = QWidget::createWindowContainer(m_window, this);
    m_layout->addWidget(m_container);

    // Set window attributes
    setAttribute(Qt::WA_NativeWindow, true);
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
        // Ensure window has correct size, but only call when widget has valid parent and screen
        adjustSize();
        
        // Use QTimer to delay browser creation, ensuring window size is properly set
        QTimer::singleShot(100, this, [this, url]() {
            // Create browser window
            CefWindowInfo windowInfo;
            
#ifdef Q_OS_WIN
            // Get device pixel ratio for high DPI support
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
            
            // Enable high DPI support
            // browserSettings.windowless_frame_rate = 60;

            CefString cefUrl(url.toStdString());
            CefBrowserHost::CreateBrowser(windowInfo, this, cefUrl, browserSettings, nullptr, nullptr);
            
            // Ensure browser window fills the entire container
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
    // Get device pixel ratio for high DPI support
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
    // Implementation needed for off-screen rendering mode, but we use window rendering mode, so no implementation needed here
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
            // Get device pixel ratio for high DPI support
            QScreen* currentScreen = screen();
            qreal devicePixelRatio = currentScreen ? currentScreen->devicePixelRatio() : 1.0;
            
            // Resize CEF browser window
            RECT rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = width() * devicePixelRatio;
            rect.bottom = height() * devicePixelRatio;
            
            // Use Windows API to resize window
            HDWP hdwp = BeginDeferWindowPos(1);
            hdwp = DeferWindowPos(hdwp, hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
            EndDeferWindowPos(hdwp);
#else
            // Handle non-Windows platforms
            m_browser->GetHost()->WasResized();
#endif
        }
    }
}
