#pragma once

#include <QWidget>
#include <memory>
#include "CefHandler.hpp"

namespace seventeenlive {

class QCefWidget : public QWidget {
    Q_OBJECT

public:
    explicit QCefWidget(QWidget* parent = nullptr);
    ~QCefWidget();

    void loadUrl(const QString& url);
    void reload();
    void stopLoading();

protected:
    // 重写Qt事件
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    WId getWindowHandle() const;

private:
    void initializeCef();
    void createBrowser();

private:
    std::unique_ptr<CefHandler> handler_;
    bool browserCreated_ = false;
};

} // namespace seventeenlive
