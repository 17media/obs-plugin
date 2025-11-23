#pragma once

#include <QDockWidget>
#include <QString>

class QCefView;

class OneSevenLiveChatDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit OneSevenLiveChatDock(QWidget* parent, const QString& chatUrl);
    ~OneSevenLiveChatDock();

    void setUrl(const QString& url);
    void reload();
    void setSuppressClosePrompt(bool suppress);

   private:
    QCefView* cefView_ = nullptr;
    QString chatUrl_;
};