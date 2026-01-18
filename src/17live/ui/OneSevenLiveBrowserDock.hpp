#pragma once

#include <QDockWidget>
#include <QPointer>
#include <QString>

struct QCef;
class QCefWidget;
struct QCefCookieManager;

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
    QPointer<QCefWidget> cefWidget_ = nullptr;
    QCef* cef_ = nullptr;
    QCefCookieManager* panelCookies_ = nullptr;
    bool browserClosed_ = false;
    QString currentUrl_;
};
