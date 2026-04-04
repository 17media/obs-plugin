#pragma once

#include <obs-module.h>

#include <QPointer>
#include <QWidget>

class QLabel;
class QCefWidget;
struct QCef;

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

   private slots:
    void onGiftsLoaded();

   private:
    QString chatUrl_;
    QPointer<QCefWidget> cefWidget_ = nullptr;
    QCef* cef_ = nullptr;

    QWidget* loadingOverlay = nullptr;
    QLabel* loadingLabel = nullptr;
    QLabel* errorLabel_ = nullptr;

    bool browserClosed_ = false;
};
