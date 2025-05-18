#pragma once

#include "cef-headers.hpp"

class QCefWidget;

class CefHandler : public CefClient,
                      public CefLifeSpanHandler
{
public:
  explicit CefHandler(QCefWidget* widget);
  ~CefHandler() override;

  // Provide access to the single global instance of this object.
  static CefHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  
  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  // 获取浏览器实例
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

private:

  bool is_closing_ = false;
  
  QCefWidget* widget_;
  CefRefPtr<CefBrowser> browser_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(CefHandler);
};
