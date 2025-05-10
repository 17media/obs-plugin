#pragma once

// #include <include/cef_client.h>
// #include <include/cef_life_span_handler.h>
#include "browser/cef-headers.hpp"

namespace seventeenlive {

class QCefWidget;

class CefHandler : public CefClient, public CefLifeSpanHandler {
public:
    explicit CefHandler(QCefWidget* widget);
    
    // CefClient 方法
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    
    // CefLifeSpanHandler 方法
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    
    // 获取浏览器实例
    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
    
private:
    QCefWidget* widget_;
    CefRefPtr<CefBrowser> browser_;
    
    // 实现 CefBase 的引用计数
    IMPLEMENT_REFCOUNTING(CefHandler);
};

} // namespace seventeenlive
