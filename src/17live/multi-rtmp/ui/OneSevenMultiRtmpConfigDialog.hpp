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
    OneSevenMultiRtmpConfig buildConfigFromUI() const;

public slots:
    void accept() override;
    void reject() override;

private slots:
    void onEncoderSharingChanged();
    void onVideoResolutionChanged();
    void onValidationTimer();
    void onAdvancedSettingsToggled();

private:
    void setupUI();
    void setupBasicInfoSection();
    void setupAdvancedSettingsButton();
    void setupAdvancedSettingsWidget();
    void setupServiceTab();
    void setupOutputTab();
    void setupVideoTab();
    void setupAudioTab();
    void setupButtonBox();
    
    void setupConnections();
    void setupValidation();
    
    void populateEncoderOptions();
    void populateVideoResolutions();
    void populateAudioFormats();
    
    void updateEncoderFields();
    void updateVideoFields();
    void updateAudioFields();
    
    bool validateConfiguration();
    void showValidationErrors();
    
    void loadConfigToUI(const OneSevenMultiRtmpConfig& config);

    std::shared_ptr<OneSevenMultiRtmpConfig> m_config;

    // Main layout
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;

    OneSevenLivePropertiesWidget *m_serviceWidget;
    OneSevenLivePropertiesWidget *m_outputWidget;
    OneSevenLivePropertiesWidget *m_videoWidget;
    OneSevenLivePropertiesWidget *m_audioWidget;
    
    // Basic info section
    QWidget* m_basicInfoWidget;
    QFormLayout* m_basicInfoLayout;
    QLineEdit* m_streamNameEdit;
    QComboBox* m_protocolCombo;
    QLineEdit* m_serverEdit;
    QLineEdit* m_keyEdit;
    QCheckBox* m_showKeyCheck;
    QCheckBox* m_authCheck;
    
    // Advanced settings section
    QPushButton* m_advancedButton;
    QWidget* m_advancedWidget;
    bool m_advancedExpanded;
    
    // Service tab components removed - functionality integrated into basic info section
    
    // Output tab
    QWidget* m_outputTab;
    QFormLayout* m_outputLayout;
    QComboBox* m_encoderTypeCombo;
    QCheckBox* m_shareEncoderCheck;
    QSpinBox* m_videoBitrateSpin;
    QSpinBox* m_audioBitrateSpin;
    QComboBox* m_outputModeCombo;
    QCheckBox* m_enableReconnectCheck;
    QSpinBox* m_maxRetriesSpin;
    QSpinBox* m_retryDelaySpin;
    
    // Video tab
    QWidget* m_videoTab;
    QFormLayout* m_videoLayout;
    QComboBox* m_videoResolutionCombo;
    QLineEdit* m_customWidthEdit;
    QLineEdit* m_customHeightEdit;
    QDoubleSpinBox* m_fpsSpinBox;
    QComboBox* m_scaleFilterCombo;
    QCheckBox* m_enableVideoCheck;
    QSlider* m_qualitySlider;
    QLabel* m_qualityLabel;
    
    // Audio tab
    QWidget* m_audioTab;
    QFormLayout* m_audioLayout;
    QComboBox* m_audioFormatCombo;
    QSpinBox* m_sampleRateSpin;
    QComboBox* m_channelLayoutCombo;
    QCheckBox* m_enableAudioCheck;
    QSlider* m_audioVolumeSlider;
    QLabel* m_audioVolumeLabel;
    
    // Additional controls
    QComboBox* m_syncModeCombo;
    QComboBox* m_logLevelCombo;
    
    // Button box
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    
    // Validation
    QTimer* m_validationTimer;
    QLabel* m_validationLabel;
    
    // State
    bool m_isEditMode;
    OneSevenMultiRtmpConfig m_originalConfig;
    
    // Constants for UI
    static const QStringList SERVICE_TYPES;
    static const QStringList ENCODER_TYPES;
    static const QStringList VIDEO_RESOLUTIONS;
    static const QStringList AUDIO_FORMATS;
    static const QStringList SCALE_FILTERS;
    static const QStringList SYNC_MODES;
    static const QStringList LOG_LEVELS;
};
