#include "sl-browser-app.hpp"

namespace seventeenlive {

BrowserApp::BrowserApp() = default;

void BrowserApp::OnContextInitialized() {
    // 浏览器进程初始化完成后的回调
    // 这里可以执行一些初始化操作
}

// CefRefPtr<CefClient> BrowserApp::GetDefaultClient() {
//     // Called when a new browser window is created via Chrome style UI.
//     return SimpleHandler::GetInstance();
// }

} // namespace seventeenlive
