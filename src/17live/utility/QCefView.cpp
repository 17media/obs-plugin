#include "QCefView.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>  // For os_event_t, etc.
#include <util/threading.h>

#include <QDebug>
#include <QResizeEvent>
#include <QScreen>
#include <QCloseEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWindow>
#include <chrono>
#include <obs.hpp>
#include <thread>
#include <util/dstr.hpp>  // For DStr

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Windowsx.h>
#endif

#include "SimpleCefClient.hpp"
#include "CefDummy.hpp"
#include "../OneSevenLiveCoreManager.hpp"
#include "moc_QCefView.cpp"
#include "plugin-support.h"
#include <atomic>
static std::atomic<int> g_qcef_alive_count{0};

QCefView::QCefView(QWidget *parent) : QWidget(parent), m_client(nullptr) {
    // Create layout
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    m_container = new QWidget(this);
    m_container->setAttribute(Qt::WA_NativeWindow, true);
    m_layout->addWidget(m_container);

    // Set window attributes
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_DontCreateNativeAncestors, false);
    setAttribute(Qt::WA_DeleteOnClose, true);
    g_qcef_alive_count.fetch_add(1, std::memory_order_relaxed);
}

QCefView::~QCefView() {
    m_client = nullptr;
    g_qcef_alive_count.fetch_sub(1, std::memory_order_relaxed);
}

void QCefView::loadUrl(const QString &url) {
    m_currentUrl = url;
    if (m_closing)
        return;
    if (OneSevenLiveCoreManager::getInstance().isShuttingDown())
        return;
    if (!cef_is_initialized())
        return;
    if (m_client && m_client->getBrowser()) {
        CefString cefUrl(url.toStdString());
        m_client->getBrowser()->GetMainFrame()->LoadURL(cefUrl);
    } else {
        // Ensure window has correct size, but only call when widget has valid parent and screen
        adjustSize();

        // Use QTimer to delay browser creation, ensuring window size is properly set
        QTimer::singleShot(100, this, [this, url]() {
            if (m_closing || OneSevenLiveCoreManager::getInstance().isShuttingDown()) {
                return;
            }
            if (!cef_is_initialized()) {
                return;
            }
            // Create browser window
            CefWindowInfo windowInfo;

#ifdef Q_OS_WIN
            QScreen *currentScreen = screen();
            qreal devicePixelRatio = currentScreen ? currentScreen->devicePixelRatio() : 1.0;
            int scaledWidth = m_container->width() * devicePixelRatio;
            int scaledHeight = m_container->height() * devicePixelRatio;
            windowInfo.SetAsChild((CefWindowHandle)m_container->winId(),
                                  CefRect(0, 0, scaledWidth, scaledHeight));
#else
            windowInfo.SetAsChild((CefWindowHandle)m_container->winId(),
                                CefRect(0, 0, m_container->width(), m_container->height()));
#endif

            CefBrowserSettings browserSettings;

            // Enable high DPI support
            // browserSettings.windowless_frame_rate = 60;

            CefString cefUrl(url.toStdString());

            m_client = new SimpleCefClient();
            // Forward URL changes from CEF to Qt layer safely
            m_client->setUrlChangedCallback([this](const QString &newUrl) {
                QTimer::singleShot(0, this, [this, newUrl]() {
                    m_currentUrl = newUrl;
                    emit urlChanged(newUrl);
                });
            });
            CefBrowserHost::CreateBrowser(windowInfo, m_client.get(), cefUrl, browserSettings,
                                          nullptr, nullptr);

            // Ensure browser window fills the entire container
            QTimer::singleShot(200, this, [this]() {
                if (m_closing || OneSevenLiveCoreManager::getInstance().isShuttingDown()) {
                    return;
                }
                if (m_client->getBrowser()) {
                    resizeEvent(nullptr);
                }
            });
        });
    }
}

int QCefView::aliveCount() {
    return g_qcef_alive_count.load(std::memory_order_relaxed);
}

QString QCefView::currentUrl() const {
    return m_currentUrl;
}

void QCefView::reload() {
    if (m_closing || OneSevenLiveCoreManager::getInstance().isShuttingDown())
        return;
    if (!cef_is_initialized())
        return;
    if (m_client && m_client->getBrowser()) {
        obs_log(LOG_INFO, "QCefView::reload() - Reloading current page");
        m_client->getBrowser()->Reload();
    } else {
        obs_log(LOG_WARNING, "QCefView::reload() - Browser not initialized, cannot reload");
    }
}

void QCefView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_closing || OneSevenLiveCoreManager::getInstance().isShuttingDown())
        return;
    if (!cef_is_initialized())
        return;
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
            rect.right = m_container->width() * devicePixelRatio;
            rect.bottom = m_container->height() * devicePixelRatio;

            // Ensure browser window is parented to the container HWND
            HWND hParent = reinterpret_cast<HWND>(m_container->winId());
            if (GetParent(hwnd) != hParent) {
                SetParent(hwnd, hParent);
            }

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
void QCefView::closeEvent(QCloseEvent *event) {
    m_closing = true;
    QWidget::closeEvent(event);
}

void QCefView::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (m_closing || OneSevenLiveCoreManager::getInstance().isShuttingDown())
        return;
    if (!cef_is_initialized())
        return;
    if (m_client && m_client->getBrowser()) {
#ifdef Q_OS_WIN
        // Reparent to container in case tabify changed native parentage
        HWND hwnd = m_client->getBrowser()->GetHost()->GetWindowHandle();
        HWND hParent = reinterpret_cast<HWND>(m_container->winId());
        if (hwnd && hParent && GetParent(hwnd) != hParent) {
            SetParent(hwnd, hParent);
        }
#endif
        m_client->getBrowser()->GetHost()->WasHidden(false);
        m_client->getBrowser()->GetHost()->SetFocus(true);
        resizeEvent(nullptr);
    }
}

void QCefView::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (!cef_is_initialized())
        return;
    if (m_client && m_client->getBrowser()) {
        m_client->getBrowser()->GetHost()->WasHidden(true);
        m_client->getBrowser()->GetHost()->SetFocus(false);
    }
}

bool QCefView::event(QEvent *e) {
    if (e->type() == QEvent::ShowToParent) {
        if (m_container)
            m_container->show();
        if (!m_closing && cef_is_initialized() && m_client && m_client->getBrowser()) {
#ifdef Q_OS_WIN
            // Reparent to container to keep HWND hierarchy stable
            HWND hwnd = m_client->getBrowser()->GetHost()->GetWindowHandle();
            HWND hParent = reinterpret_cast<HWND>(m_container->winId());
            if (hwnd && hParent && GetParent(hwnd) != hParent) {
                SetParent(hwnd, hParent);
            }
#endif
            m_client->getBrowser()->GetHost()->WasHidden(false);
            m_client->getBrowser()->GetHost()->SetFocus(true);
            resizeEvent(nullptr);
        }
    } else if (e->type() == QEvent::HideToParent) {
        if (m_client && m_client->getBrowser() && cef_is_initialized()) {
            m_client->getBrowser()->GetHost()->WasHidden(true);
            m_client->getBrowser()->GetHost()->SetFocus(false);
        }
    }
    return QWidget::event(e);
}
