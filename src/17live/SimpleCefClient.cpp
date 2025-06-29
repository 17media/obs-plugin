#include "SimpleCefClient.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>  // For os_event_t, etc.
#include <util/threading.h>

#include <obs.hpp>
#include <util/dstr.hpp>  // For DStr

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Windowsx.h>
#endif

#include "plugin-support.h"

void SimpleCefClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (!m_browser) {
        m_browser = browser;
    }
}

bool SimpleCefClient::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (m_browser && m_browser->GetIdentifier() == browser->GetIdentifier()) {
        m_browser = nullptr;
    }

    return false;
}

void SimpleCefClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (m_browser && m_browser->GetIdentifier() == browser->GetIdentifier()) {
        m_browser = nullptr;
    }
}
