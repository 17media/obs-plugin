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

/**
 * Configuration dialog for Multi-RTMP stream settings
 * Provides comprehensive configuration interface with multiple tabs
 */
class OneSevenMultiRtmpConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit OneSevenMultiRtmpConfigDialog(QWidget* parent = nullptr);
    ~OneSevenMultiRtmpConfigDialog();

    // Configuration management
    void setConfig(const OneSevenMultiRtmpConfig& config);
    OneSevenMultiRtmpConfig getConfig() const;
    void resetToDefaults();
    
    // Dialog modes
    void setEditMode(bool isEdit);
    bool isEditMode() const { return m_isEditMode; }

public slots:
    void accept() override;
    void reject() override;

private slots:
    void onServiceTypeChanged();
    void onCustomServiceToggled(bool enabled);
    void onTestConnectionClicked();
    void onEncoderSharingChanged();
    void onVideoResolutionChanged();
    void onValidationTimer();

private:
    void setupUI();
    void setupBasicInfoSection();
    void setupServiceTab();
    void setupOutputTab();
    void setupVideoTab();
    void setupAudioTab();
    void setupButtonBox();
    
    void setupConnections();
    void setupValidation();
    
    void populateServiceTypes();
    void populateEncoderOptions();
    void populateVideoResolutions();
    void populateAudioFormats();
    
    void updateServiceFields();
    void updateEncoderFields();
    void updateVideoFields();
    void updateAudioFields();
    
    bool validateConfiguration();
    void showValidationErrors();
    void updateConnectionTest();
    
    void loadConfigToUI(const OneSevenMultiRtmpConfig& config);
    OneSevenMultiRtmpConfig buildConfigFromUI() const;

    // Main layout
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    
    // Basic info section
    QWidget* m_basicInfoWidget;
    QFormLayout* m_basicInfoLayout;
    QLineEdit* m_streamNameEdit;
    QComboBox* m_protocolCombo;
    
    // Service tab
    QWidget* m_serviceTab;
    QFormLayout* m_serviceLayout;
    QComboBox* m_serviceTypeCombo;
    QCheckBox* m_customServiceCheck;
    QLineEdit* m_serverEdit;
    QLineEdit* m_keyEdit;
    QTextEdit* m_descriptionEdit;
    QPushButton* m_testConnectionButton;
    QProgressBar* m_connectionProgress;
    QLabel* m_connectionStatusLabel;
    
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
