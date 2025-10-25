#pragma once

#include "../OneSevenMultiRtmpModels.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QSlider>
#include <QProgressBar>
#include <QTimer>
#include <QWidget>
#include <QIcon>

class OneSevenLivePropertiesWidget;

/**
 * Configuration dialog for Multi-RTMP stream settings
 * Provides comprehensive configuration interface with multiple tabs
 */
class OneSevenMultiRtmpConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit OneSevenMultiRtmpConfigDialog(QWidget* parent = nullptr, std::shared_ptr<OneSevenMultiRtmpConfig> config = nullptr);
    ~OneSevenMultiRtmpConfigDialog();

    void resetToDefaults();
    
    // Dialog modes
    void setEditMode(bool isEdit);
    bool isEditMode() const { return m_isEditMode; }
    
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
    
    void loadConfigToUI(const OneSevenMultiRtmpConfig& config);

    void loadEncoders();
    void loadScenes();

    std::shared_ptr<OneSevenMultiRtmpConfig> m_config;
    std::shared_ptr<OneSevenMultiRtmpConfig> m_originalConfig;

    // Main layout
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    
    
    // Basic info section
    QWidget* m_basicInfoWidget;
    QFormLayout* m_basicInfoLayout;
    QLineEdit* m_streamNameEdit;
    QComboBox* m_protocolCombo;
    OneSevenLivePropertiesWidget *m_serviceWidget;
    QCheckBox* m_syncStartCheckbox;
    QCheckBox* m_syncStopCheckbox;

    // Advanced settings section
    QPushButton* m_advancedButton;
    QWidget* m_advancedWidget;
    bool m_advancedExpanded;
    
    // Output tab
    QWidget* m_outputTab;
    QFormLayout* m_outputLayout;
    OneSevenLivePropertiesWidget *m_outputWidget;
    
    // Video tab
    QWidget* m_videoTab;
    QFormLayout* m_videoLayout;
    QCheckBox* m_useOBSVideoCheck;
    QComboBox* m_videoEncoderCombo;
    QComboBox* m_videoResolutionCombo;
    QComboBox* m_fpsDenominatorCombo;
    QComboBox* m_outputSceneCombo;
    OneSevenLivePropertiesWidget *m_videoWidget;
    
    // Audio tab
    QWidget* m_audioTab;
    QFormLayout* m_audioLayout;
    QCheckBox* m_useOBSAudioCheck;
    QComboBox* m_audioEncoderCombo;
    OneSevenLivePropertiesWidget *m_audioWidget;
    
    
    // Button box
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    
    // State
    bool m_isEditMode;
};
