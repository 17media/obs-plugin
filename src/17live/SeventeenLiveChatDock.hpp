#pragma once

#include <QDockWidget>
#include <QVBoxLayout>
#include <memory>
#include <obs-frontend-api.h>
#include "utility/cef-headers.hpp"  // 替换为CEF头文件

namespace seventeenlive {

class SeventeenLiveChatDock : public QDockWidget {
    Q_OBJECT

public:
    explicit SeventeenLiveChatDock(QWidget* parent = nullptr);
    ~SeventeenLiveChatDock();

private:
    void setupUi();
    void initializeWebEngine();

private:
    QWidget* containerWidget;
    QVBoxLayout* layout;
    std::unique_ptr<QCefWidget> webView;  // 使用QCefWidget替换QWebView
};

} // namespace seventeenlive
