#pragma once

#include <QDockWidget>
#include <QVBoxLayout>
#include <memory>
#include <obs-frontend-api.h>

namespace seventeenlive {

class QCefWidget;

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
