#pragma once

#include <QDockWidget>
#include <QPointer>
#include <QString>
#include <memory>

class CefWidgetHost;

class OneSevenLiveBrowserDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveBrowserDock(QWidget* parent = nullptr, const QString& title = QString());
    ~OneSevenLiveBrowserDock();

    void setUrl(const QString& url);
    void reload();
    void setStartupScript(const QString& script);

   signals:
    void urlChanged(const QString& url);
    void dockClosed();

   protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private:
    void createBrowser(const QString& url);
    void destroyBrowser(bool fullCleanup);

    QWidget* container_ = nullptr;
    std::unique_ptr<CefWidgetHost> cefHost_;
    QString currentUrl_;
};
