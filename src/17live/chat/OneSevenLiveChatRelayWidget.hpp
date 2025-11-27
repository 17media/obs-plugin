#pragma once
#pragma once
#include <QPointer>
#include <QString>
#include <QWidget>
class QCefView;

class OneSevenLiveChatRelayWidget : public QWidget {
    Q_OBJECT
   public:
    explicit OneSevenLiveChatRelayWidget(QWidget* parent = nullptr);
    ~OneSevenLiveChatRelayWidget() override;
    void startRelay(const QString& roomID, int httpPort, int wsPort);

   private:
    QPointer<QCefView> cefView_;
};
