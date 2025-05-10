#include "sl-browser-app.hpp"

namespace seventeenlive {

void BrowserApp::OnContextInitialized() {
    // 浏览器进程初始化完成后的回调
    // 这里可以执行一些初始化操作
}

void BrowserApp::OnBeforeChildProcessLaunch([[maybe_unused]] CefRefPtr<CefCommandLine> command_line) {
    // 在启动子进程之前的回调
    // 可以在这里修改命令行参数
}

// void BrowserApp::OnRenderProcessThreadCreated([[maybe_unused]] CefRefPtr<CefListValue> extra_info) {
//     // 渲染进程线程创建时的回调
//     // 可以在这里传递额外信息给渲染进程
// }

} // namespace seventeenlive
