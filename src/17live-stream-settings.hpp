#pragma once
#include <obs-module.h>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>
#include "17live-chat-window.hpp"

class SeventeenLiveStreamSettings : public QDialog {
    Q_OBJECT

public:
    SeventeenLiveStreamSettings(QWidget *parent = nullptr);
    ~SeventeenLiveStreamSettings() = default;

    static void Register();

private slots:
    void openChatWindow();
    void onLoginClicked();
    void onSaveSettings();
    void onStreamKeyGenerated(const QString &key);
    void onCreateBrowserSourceClicked();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    void updateStreamKey();

    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLineEdit *titleEdit;
    QLineEdit *hashtagsEdit;
    QLineEdit *streamKeyEdit;
    QPushButton *loginButton;
    QPushButton *saveButton;
    QPushButton *generateKeyButton;

    bool isAuthenticated;
    QString authToken;
};
