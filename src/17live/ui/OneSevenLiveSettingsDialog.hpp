#pragma once

#include <QDialog>

class QCheckBox;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QWidget;
class OneSevenLiveConfigManager;

class OneSevenLiveSettingsDialog : public QDialog {
    Q_OBJECT

   public:
    explicit OneSevenLiveSettingsDialog(QWidget* parent, OneSevenLiveConfigManager* configManager);
    ~OneSevenLiveSettingsDialog() override;

   private:
    void setupUi();
    void loadValues();
    void applyValues();
    void updateApplyState();

    OneSevenLiveConfigManager* configManager_ = nullptr;

    QListWidget* categoryList_ = nullptr;
    QStackedWidget* stackedWidget_ = nullptr;

    QCheckBox* autoAdjustDontRemindCheck_ = nullptr;

    QPushButton* okButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* applyButton_ = nullptr;

    bool initialAutoAdjustDontRemind_ = false;
};

