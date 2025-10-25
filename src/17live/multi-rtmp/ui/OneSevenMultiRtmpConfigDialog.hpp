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
    void onValidationTimer();
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
    void setupValidation();
    

    

    
    bool validateConfiguration();
    void showValidationErrors();
    
    void loadConfigToUI(const OneSevenMultiRtmpConfig& config);

    std::shared_ptr<OneSevenMultiRtmpConfig> m_config;

    // Main layout
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;

    OneSevenLivePropertiesWidget *m_serviceWidget;
    OneSevenLivePropertiesWidget *m_outputWidget;
    
    // Basic info section
    QWidget* m_basicInfoWidget;
    QFormLayout* m_basicInfoLayout;
    QLineEdit* m_streamNameEdit;
    QComboBox* m_protocolCombo;

    
    // Advanced settings section
    QPushButton* m_advancedButton;
    QWidget* m_advancedWidget;
    bool m_advancedExpanded;
    
    // Service tab components removed - functionality integrated into basic info section
    
    // Output tab (properties widget used instead of individual controls)
    
    // Video tab
    QWidget* m_videoTab;
    QFormLayout* m_videoLayout;
    
    // Audio tab
    QWidget* m_audioTab;
    QFormLayout* m_audioLayout;
    
    // Additional controls
    
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
    

};
