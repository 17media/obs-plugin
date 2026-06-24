#pragma once

#include <QDockWidget>
#include <QString>
#include <memory>

class QCloseEvent;
class QHideEvent;
class QLabel;
class QResizeEvent;
class QShowEvent;
class QWidget;
class CefWidgetHost;

class OneSevenLiveChatDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveChatDock(const QString& title, const QString& chatUrl,
                                  QWidget* parent = nullptr);
    ~OneSevenLiveChatDock();

    void setUrl(const QString& url);
    void reload();
    void prepareForDelete();

   protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private slots:
    void onGiftsLoaded();

   private:
    void createBrowser(const QString& url);
    void shutdownBrowser();

    QString title_;
    QString chatUrl_;
    QWidget* contentWidget_ = nullptr;
    QWidget* browserContainer_ = nullptr;
    QWidget* loadingOverlay_ = nullptr;
    QLabel* loadingLabel_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    std::unique_ptr<CefWidgetHost> cefHost_;
    bool deleting_{false};
};
