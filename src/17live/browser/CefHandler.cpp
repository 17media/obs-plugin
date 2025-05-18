#include "CefHandler.hpp"
#include "QCefWidget.hpp"
#include <obs-module.h>

#include "plugin-support.h"

CefHandler* g_instance = nullptr;
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

void CefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (!browser_) {
    browser_ = browser;
  }
  obs_log(LOG_INFO, "CEF Browser created.");

  if (widget_) {
    widget_->onBrowserCreated();
  }
}

bool CefHandler::DoClose([[maybe_unused]] CefRefPtr<CefBrowser> browser) {

  CEF_REQUIRE_UI_THREAD();
  if (browser_ && browser_->GetIdentifier() == browser->GetIdentifier()) {
    browser_ = nullptr;
    if (widget_) {
      // This will trigger OnBeforeClose
      // For a QWidget hosted CEF, we might need to close the QWidget
    }
  }
        
  return false; // Allow close
}

void CefHandler::OnBeforeClose([[maybe_unused]] CefRefPtr<CefBrowser> browser) {

  CEF_REQUIRE_UI_THREAD();
  obs_log(LOG_INFO, "CEF Browser closing.");
  if (browser_ && browser_->GetIdentifier() == browser->GetIdentifier()) {
    browser_ = nullptr;
  }
  if (widget_) {
    // If CEF is embedded in a QWidget, ensure the QWidget is closed and cleaned up.
    // For a top-level CEF window, this might be where you'd hide or destroy it.
    // Since we are using a QMainWindow, we'll close it.
    widget_->close(); // This should trigger the QMainWindow's close event
  }
}
