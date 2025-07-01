#include "QCefView.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>  // For os_event_t, etc.
#include <util/threading.h>

#include <QDebug>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <obs.hpp>
#include <util/dstr.hpp>  // For DStr

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Windowsx.h>
#endif

#include "SimpleCefClient.hpp"
#include "moc_QCefView.cpp"
#include "plugin-support.h"

QCefView::QCefView(QWidget *parent) : QWidget(parent), m_client(nullptr) {
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
}

QCefView::~QCefView() {
    if (m_client->getBrowser()) {
        obs_log(LOG_INFO, "QCefView::~QCefView()");
        m_client->getBrowser()->GetHost()->CloseBrowser(true);
    }
}

void QCefView::loadUrl(const QString &url) {
    m_currentUrl = url;
    if (m_client && m_client->getBrowser()) {
        CefString cefUrl(url.toStdString());
        m_client->getBrowser()->GetMainFrame()->LoadURL(cefUrl);
    } else {
        // Ensure window has correct size, but only call when widget has valid parent and screen
        adjustSize();

        // Use QTimer to delay browser creation, ensuring window size is properly set
        QTimer::singleShot(100, this, [this, url]() {
            // Create browser window
            CefWindowInfo windowInfo;

#ifdef Q_OS_WIN
            // Get device pixel ratio for high DPI support
            QScreen *currentScreen = screen();
            qreal devicePixelRatio = currentScreen ? currentScreen->devicePixelRatio() : 1.0;
            int scaledWidth = width() * devicePixelRatio;
            int scaledHeight = height() * devicePixelRatio;

            windowInfo.SetAsChild((CefWindowHandle) m_window->winId(),
                                  CefRect(0, 0, scaledWidth, scaledHeight));
#else
            windowInfo.SetAsChild((CefWindowHandle)m_window->winId(), 
                                CefRect(0, 0, width(), height()));
#endif

            CefBrowserSettings browserSettings;

            // Enable high DPI support
            // browserSettings.windowless_frame_rate = 60;

            CefString cefUrl(url.toStdString());

            m_client = new SimpleCefClient();
            CefBrowserHost::CreateBrowser(windowInfo, m_client.get(), cefUrl, browserSettings,
                                          nullptr, nullptr);

            // Ensure browser window fills the entire container
            QTimer::singleShot(200, this, [this]() {
                if (m_client->getBrowser()) {
                    resizeEvent(nullptr);
                }
            });
        });
    }
}

QString QCefView::currentUrl() const {
    return m_currentUrl;
}

void QCefView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_client && m_client->getBrowser()) {
        CefWindowHandle hwnd = m_client->getBrowser()->GetHost()->GetWindowHandle();
        if (hwnd) {
#ifdef Q_OS_WIN
            // Get device pixel ratio for high DPI support
            QScreen *currentScreen = screen();
            qreal devicePixelRatio = currentScreen ? currentScreen->devicePixelRatio() : 1.0;

            // Resize CEF browser window
            RECT rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = width() * devicePixelRatio;
            rect.bottom = height() * devicePixelRatio;

            // Use Windows API to resize window
            HDWP hdwp = BeginDeferWindowPos(1);
            hdwp = DeferWindowPos(hdwp, hwnd, NULL, rect.left, rect.top, rect.right - rect.left,
                                  rect.bottom - rect.top, SWP_NOZORDER);
            EndDeferWindowPos(hdwp);
#else
            // Handle non-Windows platforms
            m_client->getBrowser()->GetHost()->WasResized();
#endif
        }
    }
}
