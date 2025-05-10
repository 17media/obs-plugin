#include "CefHandler.hpp"
#include "QCefWidget.hpp"
#include <obs-module.h>

#include "plugin-support.h"

namespace seventeenlive {

CefHandler::CefHandler(QCefWidget* widget)
    : widget_(widget), browser_(nullptr)
{
}

void CefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    browser_ = browser;
    obs_log(LOG_INFO, "Browser created successfully");
    
    if (widget_) {
        widget_->onBrowserCreated();
    }
}

} // namespace seventeenlive
