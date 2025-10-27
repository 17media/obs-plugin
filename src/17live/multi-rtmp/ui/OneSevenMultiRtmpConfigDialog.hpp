#pragma once

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "../OneSevenMultiRtmpModels.hpp"
#include "plugin-support.h"

class OneSevenLivePropertiesWidget;

/**
 * Configuration dialog for Multi-RTMP stream settings
 * Provides comprehensive configuration interface with multiple tabs
 */
class OneSevenMultiRtmpConfigDialog : public QDialog {
    Q_OBJECT

   public:
    explicit OneSevenMultiRtmpConfigDialog(
        QWidget* parent = nullptr, std::shared_ptr<OneSevenMultiRtmpConfig> config = nullptr);
    ~OneSevenMultiRtmpConfigDialog();

    // Dialog modes
    void setEditMode(bool isEdit);

    bool isEditMode() const {
        return m_isEditMode;
    }

    // Configuration access
    OneSevenMultiRtmpConfig SaveConfig() const;

   public slots:
    void accept() override;
    void reject() override;

   private slots:
    void onAdvancedSettingsToggled();

   private:
    void setupUI();
    void setupBasicInfoSection();
    void setupAdvancedSettingsButton();
    void setupAdvancedSettingsWidget();
    void setupOutputTab();
    void setupVideoTab();
    void setupAudioTab();
    void setupButtonBox();

    void setupConnections();

    void loadEncoders();
    void loadScenes();
    void loadConfig();

    // Helper function to parse and load encoders for both video and audio
    std::vector<std::string> parseAndLoadEncoders(const std::string& supportedEncoders,
                                                  bool isVideoEncoder);

    std::shared_ptr<OneSevenMultiRtmpConfig> m_config;
    std::shared_ptr<OneSevenMultiRtmpConfig> m_originalConfig;

    std::string m_supportedVideoEncoders;
    std::string m_supportedAudioEncoders;

    // Main layout
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;

    // Basic info section
    QWidget* m_basicInfoWidget;
    QFormLayout* m_basicInfoLayout;
    QLineEdit* m_streamNameEdit;
    QComboBox* m_protocolCombo;
    OneSevenLivePropertiesWidget* m_serviceWidget;
    QCheckBox* m_syncStartCheckbox;
    QCheckBox* m_syncStopCheckbox;

    // Advanced settings section
    QPushButton* m_advancedButton;
    QWidget* m_advancedWidget;
    bool m_advancedExpanded;
    int m_baseHeight;

    // Output tab
    QWidget* m_outputTab;
    QFormLayout* m_outputLayout;
    OneSevenLivePropertiesWidget* m_outputWidget;

    // Video tab
    QWidget* m_videoTab;
    QFormLayout* m_videoLayout;
    QCheckBox* m_useOBSVideoCheck;
    QComboBox* m_videoEncoderCombo;
    QComboBox* m_videoResolutionCombo;
    QComboBox* m_fpsDenominatorCombo;
    QComboBox* m_outputSceneCombo;
    OneSevenLivePropertiesWidget* m_videoWidget;

    // Audio tab
    QWidget* m_audioTab;
    QFormLayout* m_audioLayout;
    QCheckBox* m_useOBSAudioCheck;
    QComboBox* m_audioEncoderCombo;
    OneSevenLivePropertiesWidget* m_audioWidget;

    // Button box
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    // State
    bool m_isEditMode;
};
