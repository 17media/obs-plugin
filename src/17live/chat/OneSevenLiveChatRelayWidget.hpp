#pragma once
#pragma once
#include <QWidget>
#include <QPointer>
#include <QString>
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
