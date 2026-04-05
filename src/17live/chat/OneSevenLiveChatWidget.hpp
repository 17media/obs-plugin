#pragma once

#include <obs-module.h>

#include <QPointer>
#include <QWidget>
#include <memory>

class QLabel;
class CefWidgetHost;

class OneSevenLiveChatWidget : public QWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveChatWidget(QWidget* parent, const QString& chatUrl);
    ~OneSevenLiveChatWidget();

    void setUrl(const QString& url);
    void reload();
    void shutdown();

   protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

   private slots:
    void onGiftsLoaded();

   private:
    QString chatUrl_;
    QWidget* browserContainer_ = nullptr;
    std::unique_ptr<CefWidgetHost> cefHost_;

    QWidget* loadingOverlay = nullptr;
    QLabel* loadingLabel = nullptr;
    QLabel* errorLabel_ = nullptr;
};
