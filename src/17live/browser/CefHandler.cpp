#include "CefHandler.hpp"
#include "QCefWidget.hpp"
#include <obs-module.h>

#include "plugin-support.h"

namespace seventeenlive {

CefHandler* g_instance = nullptr;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
    return "data:" + mime_type + ";base64," +
           CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
               .ToString();
}

CefHandler::CefHandler(QCefWidget* widget)
    : widget_(widget), browser_(nullptr) {
  DCHECK(!g_instance);
  g_instance = this;
}

CefHandler::~CefHandler() {
  g_instance = nullptr;
}

// static
CefHandler* CefHandler::GetInstance() {
  return g_instance;
}

void CefHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  if (auto browser_view = CefBrowserView::GetForBrowser(browser)) {
    // Set the title of the window using the Views framework.
    CefRefPtr<CefWindow> window = browser_view->GetWindow();
    if (window) {
      window->SetTitle(title);
    }
  }
}

void CefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
           
    browser_ = browser;
    obs_log(LOG_INFO, "Browser created successfully");

    if (widget_) {
        widget_->onBrowserCreated();
    }
}

bool CefHandler::DoClose([[maybe_unused]] CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  is_closing_ = true;
  
  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void CefHandler::OnBeforeClose([[maybe_unused]] CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  browser_ = nullptr;

  CefQuitMessageLoop();
}

void CefHandler::OnLoadError([[maybe_unused]] CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED) {
    return;
  }

  // Display a load error message using a data: URI.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";

  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}

void CefHandler::ShowMainWindow() {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&CefHandler::ShowMainWindow, this));
    return;
  }

  if (!browser_) {
    return;
  }

  auto main_browser = browser_;

  if (auto browser_view = CefBrowserView::GetForBrowser(main_browser)) {
    // Show the window using the Views framework.
    if (auto window = browser_view->GetWindow()) {
      window->Show();
    }
  }
}

void CefHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&CefHandler::CloseAllBrowsers, this,
                                       force_close));
    return;
  }

  if (!browser_) {
    return;
  }

  browser_->GetHost()->CloseBrowser(force_close);
}


// void CefHandler::PlatformShowWindow(CefRefPtr<CefBrowser> browser) {
//   CEF_REQUIRE_UI_THREAD();

//   // TODO: update 

//     if (widget_) {
//         widget_->onBrowserCreated();
//     }
// }

} // namespace seventeenlive
