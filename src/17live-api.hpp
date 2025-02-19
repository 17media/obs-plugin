#pragma once
#include <QObject>
#include <QString>
#include <memory>

class SeventeenLiveAPI : public QObject {
    Q_OBJECT

public:
    static SeventeenLiveAPI& getInstance();

    bool authenticate(const QString &username, const QString &password);
    void logout();
    bool isAuthenticated() const;

    bool updateStreamSettings(const QString &title, const QString &hashtags);
    QString generateStreamKey();

    bool connectToChat(const QString &streamId);
    void disconnectFromChat();

signals:
    void chatMessageReceived(const QString &username, const QString &message);
    void chatConnectionStateChanged(bool connected);
    void streamKeyGenerated(const QString &key);

private:
    SeventeenLiveAPI();
    ~SeventeenLiveAPI() = default;

    SeventeenLiveAPI(const SeventeenLiveAPI&) = delete;
    SeventeenLiveAPI& operator=(const SeventeenLiveAPI&) = delete;

    QString authToken;
    QString currentStreamId;
    bool authenticated;
};
