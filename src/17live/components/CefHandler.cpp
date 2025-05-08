#include "CefHandler.hpp"
#include "plugin-support.h"
#include "obs-module.h"

namespace seventeenlive {

CefHandler::CefHandler(QObject* parent)
    : QObject(parent) {
    // 可在此初始化
}
  
// CefLifeSpanHandler 接口实现
void CefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    obs_log(LOG_INFO, "Browser created");
    browser_ = browser;
}
  
void CefHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    obs_log(LOG_INFO, "Browser is about to close");
    if (browser_ && browser_->IsSame(browser)) {
        browser_ = nullptr;
    }
}
  
  // CefDisplayHandler 接口实现
  void CefHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                        bool isLoading,
                                        bool canGoBack,
                                        bool canGoForward) {
      Q_UNUSED(browser);
      Q_UNUSED(canGoBack);
      Q_UNUSED(canGoForward);
  
      obs_log(LOG_INFO, "Loading state changed: %d", isLoading);
      emit loadingStateChanged(isLoading);
  }
} // namespace seventeenlive
