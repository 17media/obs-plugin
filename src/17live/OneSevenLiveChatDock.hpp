#pragma once

#include <QDockWidget>
#include <QString>
#include <QLabel>

class QCefView;

class OneSevenLiveChatDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveChatDock(QWidget* parent, const QString& chatUrl);
    ~OneSevenLiveChatDock();

    void setUrl(const QString& url);
    void reload();

   protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private slots:
    void onGiftsLoaded();

   private:
    QCefView* cefView_ = nullptr;
    QString chatUrl_;
    QWidget* loadingOverlay = nullptr;
    QLabel* loadingLabel = nullptr;
};
