#pragma once

#include "cef-headers.hpp"

namespace seventeenlive {

class QCefWidget;

class CefHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler {
 public:
  explicit CefHandler(QCefWidget* widget);
  ~CefHandler() override;

  // Provide access to the single global instance of this object.
  static CefHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override;

  void ShowMainWindow();

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  bool IsClosing() const { return is_closing_; }

  // 获取浏览器实例
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

 private:
  // Platform-specific implementation.
//   void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
//                            const CefString& title);
//   void PlatformShowWindow(CefRefPtr<CefBrowser> browser);

  bool is_closing_ = false;
  
  QCefWidget* widget_;
  CefRefPtr<CefBrowser> browser_;
    

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(CefHandler);
};

} // namespace seventeenlive
