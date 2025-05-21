#pragma once

#include <QWidget>
#include <memory>
#include <QString>

#include "cef-headers.hpp"

class CefHandler;

class QCefWidget : public QWidget {
    Q_OBJECT

public:
    explicit QCefWidget(QWidget* parent = nullptr);
    ~QCefWidget();

    void loadUrl(const QString& url);
    WId getWindowHandle() const;
    void onBrowserCreated(); // 新增：浏览器创建完成的回调

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void initializeCef();
    void createBrowser();

private:
    std::unique_ptr<CefHandler> handler_;
    CefRefPtr<CefBrowser> cefBrowser_;
    bool browserCreated_ = false;
    QString pendingUrl_; // 新增：保存待加载的URL
};
