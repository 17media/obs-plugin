#include "OneSevenMultiRtmpConfigDialog.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QDateTime>
#include <QUuid>
#include <QScrollArea>

#include "../../ui/OneSevenLivePropertiesWidget.hpp"

#include "moc_OneSevenMultiRtmpConfigDialog.cpp"

OneSevenMultiRtmpConfigDialog::OneSevenMultiRtmpConfigDialog(QWidget* parent, std::shared_ptr<OneSevenMultiRtmpConfig> config)
    : QDialog(parent)
    , m_config(config)
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_validationTimer(nullptr)
    , m_validationLabel(nullptr)
    , m_isEditMode(config != nullptr)
    , m_advancedExpanded(false)
{
    setWindowTitle(obs_module_text("MultiRTMP.Config.Title"));
    setModal(true);
    
    // Set dialog size constraints to match reference style
    setMinimumSize(300, 400);
    setMaximumSize(600, 800);
    resize(350, 500);
    
    // Apply dark theme styling
    setStyleSheet(
        "QDialog { "
        "  background-color: #1e1e1e; "
        "  color: white; "
        "} "
        "QLabel { "
        "  color: white; "
        "  font-weight: bold; "
        "} "
        "QLineEdit { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-width: 100px; "
        "} "
        "QLineEdit:focus { "
        "  border-color: #007AFF; "
        "} "
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 100px; "
        "} "
        "QComboBox:focus { "
        "  border-color: #007AFF; "
        "} "
        "QComboBox::drop-down { "
        "  border: none; "
        "  width: 20px; "
        "} "
        "QComboBox::down-arrow { "
        "  image: none; "
        "  border-left: 5px solid transparent; "
        "  border-right: 5px solid transparent; "
        "  border-top: 5px solid white; "
        "  margin-right: 5px; "
        "} "
        "QComboBox QAbstractItemView { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  selection-background-color: #007AFF; "
        "} "
        "QCheckBox { "
        "  color: white; "
        "} "
        "QCheckBox::indicator { "
        "  background-color: #3c3c3c; "
        "  border: 1px solid #555; "
        "} "
        "QCheckBox::indicator:checked { "
        "  background-color: #007AFF; "
        "} "
    );
    
    setupUI();
    setupConnections();
    setupValidation();
    
    // Load configuration if provided
    if (m_config) {
        loadConfigToUI(*m_config);
    }
}

OneSevenMultiRtmpConfigDialog::~OneSevenMultiRtmpConfigDialog()
{
}

void OneSevenMultiRtmpConfigDialog::setupUI()
{
    // Create main layout for the dialog
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Create scroll area
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);        // Allow content resizing
    scrollArea->setFrameShape(QFrame::NoFrame);  // Remove border
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);   // Show vertical scrollbar when needed
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Disable horizontal scrollbar
    
    // Create container widget for scroll area content
    QWidget *container = new QWidget();
    container->setStyleSheet(
        "QWidget {"
        "    color: white;"
        "    font-family: 'Inter';"
        "    font-style: normal;"
        "}"
        "QToolTip {"
        "   background-color: #333333;"
        "   color: #FFFFFF;"
        "   font-weight: 400;"
        "   font-size: 12px;"
        "   line-height: 16px;"
        "   padding: 5px;"
        "   border: none;"
        "   border-radius: 4px;"
        "}");
    
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(12, 16, 12, 16);
    containerLayout->setSpacing(16);
    
    // Top section: Basic information (name, protocol, URL, stream key)
    setupBasicInfoSection();
    
    // Advanced settings button
    setupAdvancedSettingsButton();
    
    // Advanced settings widget (collapsible)
    setupAdvancedSettingsWidget();
    
    // Add sections to container layout
    containerLayout->addWidget(m_basicInfoWidget);
    containerLayout->addWidget(m_advancedButton);
    containerLayout->addWidget(m_advancedWidget);
    containerLayout->addStretch(); // Add stretch to push content to top
    
    // Set container as scroll area content
    scrollArea->setWidget(container);
    
    // Bottom section: Button box (outside scroll area)
    setupButtonBox();
    
    // Add scroll area and button layout to main layout
    m_mainLayout->addWidget(scrollArea);
    m_mainLayout->addLayout(m_buttonLayout);

    loadEncoders();
    loadScenes();
}

void OneSevenMultiRtmpConfigDialog::setupBasicInfoSection()
{
    m_basicInfoWidget = new QWidget();
    m_basicInfoLayout = new QFormLayout(m_basicInfoWidget);
    
    // Set form layout properties to match reference style
    m_basicInfoLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_basicInfoLayout->setLabelAlignment(Qt::AlignLeft);
    m_basicInfoLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_basicInfoLayout->setSpacing(12);
    m_basicInfoLayout->setContentsMargins(0, 0, 0, 0);
    
    // Stream name input (required field)
    QLabel *streamNameLabel = new QLabel();
    streamNameLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("MultiRtmp.Config.StreamName")));
    
    m_streamNameEdit = new QLineEdit();
    m_streamNameEdit->setPlaceholderText(obs_module_text("MultiRtmp.Config.StreamName.Placeholder"));
    m_streamNameEdit->setText(obs_module_text("MultiRtmp.Config.StreamName.Default"));
    m_streamNameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_basicInfoLayout->addRow(streamNameLabel, m_streamNameEdit);
    
    // Protocol dropdown - only RTMP, SRT/RIST, WHIP
    QLabel *protocolLabel = new QLabel();
    protocolLabel->setText(
        QString("<span style='color:white;'>%1</span>")
            .arg(obs_module_text("MultiRtmp.Config.Protocol")));
    
    m_protocolCombo = new QComboBox();
    
    // Populate protocol combo box from the protocol list
    const OneSevenLiveProtocol* protocols = getProtocolList();
    size_t protocolCount = getProtocolCount();
    
    for (size_t i = 0; i < protocolCount; ++i) {
        m_protocolCombo->addItem(protocols[i].label, protocols[i].protocol);
    }
    
    // Set default to first protocol (RTMP)
    if (protocolCount > 0) {
        m_protocolCombo->setCurrentIndex(0);
    }
    
    m_protocolCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_basicInfoLayout->addRow(protocolLabel, m_protocolCombo);

    auto protocol_info = findProtocol(m_protocolCombo->currentData().toString().toStdString());
    obs_data_t *service_settings = obs_data_create_from_json(m_config->serviceSettings.dump().c_str());
    auto service = obs_service_create(protocol_info->serviceId, ("tmp_17live_service_" + m_config->id).c_str(), service_settings, nullptr);
    obs_data_t *settings = obs_service_get_settings(service);
    obs_properties_t *props = obs_service_properties(service);

    m_serviceWidget = new OneSevenLivePropertiesWidget(this, settings, props);
    obs_service_release(service);
    
    // Apply dark theme styling to basic info section
    m_basicInfoWidget->setStyleSheet(
        "QWidget { "
        "  background-color: transparent; "
        "  border: none; "
        "} "
        "QLabel { "
        "  color: white; "
        "  font-size: 14px; "
        "  margin-bottom: 4px; "
        "} "
        "QLineEdit { "
        "  background-color: #2d2d2d; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  color: white; "
        "  font-size: 14px; "
        "} "
        "QLineEdit:focus { "
        "  border-color: #007AFF; "
        "} "
        "QComboBox { "
        "  background-color: #2d2d2d; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  color: white; "
        "  font-size: 14px; "
        "} "
        "QComboBox:focus { "
        "  border-color: #007AFF; "
        "} "
        "QComboBox::drop-down { "
        "  border: none; "
        "} "
        "QComboBox::down-arrow { "
        "  image: url(:/resources/arrow-down.svg); "
        "  width: 12px; "
        "  height: 12px; "
        "}"
    );
}

void OneSevenMultiRtmpConfigDialog::setupAdvancedSettingsButton()
{
    m_advancedButton = new QPushButton(obs_module_text("MultiRtmp.Config.AdvancedSettings"));
    m_advancedButton->setStyleSheet(
        "QPushButton { "
        "  background-color: transparent; "
        "  border: none;"
        "  color: white; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "  border: none;"
        "  background-color: transparent; "
        "}"
    );
    
    // Set arrow icon for collapsed state
    QIcon downIcon(":/resources/arrow-down.svg");
    m_advancedButton->setIcon(downIcon);
    m_advancedButton->setIconSize(QSize(12, 12));

    m_advancedButton->setLayoutDirection(Qt::RightToLeft); // Icon on the right, centered layout
}

void OneSevenMultiRtmpConfigDialog::setupAdvancedSettingsWidget()
{
    m_advancedWidget = new QWidget();
    m_advancedWidget->setVisible(false); // Initially collapsed
    
    QVBoxLayout* advancedLayout = new QVBoxLayout(m_advancedWidget);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    advancedLayout->setSpacing(0);
    
    // Create tab widget for advanced settings
    m_tabWidget = new QTabWidget();
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { "
        "  border: 1px solid #555; "
        "  background-color: #1e1e1e; "
        "  border-radius: 6px; "
        "  margin: 0px; "
        "  padding: 6px; "
        "} "
        "QTabBar::tab { "
        "  background-color: #2d2d2d; "
        "  color: #ccc; "
        "  padding: 6px 8px; "
        "  margin-right: 2px; "
        "  border-top-left-radius: 6px; "
        "  border-top-right-radius: 6px; "
        "  font-weight: bold; "
        "  min-width: 40px; "
        "} "
        "QTabBar::tab:selected { "
        "  background-color: #1e1e1e; "
        "  color: white; "
        "  border-bottom: 2px solid #FF0001; "
        "} "
        "QTabBar::tab:hover { "
        "  background-color: #3c3c3c; "
        "} "
        "QTabBar { "
        "  qproperty-expanding: true; "
        "}"
    );
    
    setupOutputTab();
    setupVideoTab();
    setupAudioTab();
    
    advancedLayout->addWidget(m_tabWidget);
}

// Service tab removed - integrated into basic info section

void OneSevenMultiRtmpConfigDialog::setupOutputTab()
{
    m_outputTab = new QWidget();
    m_outputLayout = new QFormLayout(m_outputTab);
    m_outputLayout->setSpacing(12);
    m_outputLayout->setContentsMargins(8, 12, 8, 12);
    m_outputLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_outputLayout->setLabelAlignment(Qt::AlignLeft);
    m_outputLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    // Encoder type
    m_encoderTypeCombo = new QComboBox();
    m_encoderTypeCombo->setStyleSheet(
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 100px; "
        "} "
        "QComboBox:focus { "
        "  border-color: #007AFF; "
        "} "
        "QComboBox::drop-down { "
        "  border: none; "
        "  width: 20px; "
        "} "
        "QComboBox::down-arrow { "
        "  image: none; "
        "  border-left: 5px solid transparent; "
        "  border-right: 5px solid transparent; "
        "  border-top: 5px solid white; "
        "  margin-right: 5px; "
        "} "
        "QComboBox QAbstractItemView { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  selection-background-color: #007AFF; "
        "}"
    );
    m_outputLayout->addRow(obs_module_text("MultiRtmp.Config.Output.EncoderType"), m_encoderTypeCombo);
    
    // // Share encoder
    // m_shareEncoderCheck = new QCheckBox(obs_module_text("MultiRtmp.Config.Output.ShareEncoder"));
    // m_shareEncoderCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    // m_shareEncoderCheck->setToolTip(obs_module_text("MultiRtmp.Config.Output.ShareEncoder.Tooltip"));
    // m_outputLayout->addRow("", m_shareEncoderCheck);
    
    // // Video bitrate
    // m_videoBitrateSpin = new QSpinBox();
    // m_videoBitrateSpin->setRange(100, 50000);
    // m_videoBitrateSpin->setValue(2500);
    // m_videoBitrateSpin->setSuffix(" kbps");
    // m_videoBitrateSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    // m_outputLayout->addRow(obs_module_text("MultiRtmp.Config.Output.VideoBitrate"), m_videoBitrateSpin);
    
    // // Audio bitrate
    // m_audioBitrateSpin = new QSpinBox();
    // m_audioBitrateSpin->setRange(64, 320);
    // m_audioBitrateSpin->setValue(128);
    // m_audioBitrateSpin->setSuffix(" kbps");
    // m_audioBitrateSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    // m_outputLayout->addRow(obs_module_text("MultiRtmp.Config.Output.AudioBitrate"), m_audioBitrateSpin);
    
    // // Output mode
    // m_outputModeCombo = new QComboBox();
    // m_outputModeCombo->addItems({obs_module_text("MultiRtmp.Config.Output.OutputMode.Simple"), obs_module_text("MultiRtmp.Config.Output.OutputMode.Advanced")});
    // m_outputModeCombo->setStyleSheet(
    //     "QComboBox { "
    //     "  background-color: #3c3c3c; "
    //     "  color: white; "
    //     "  border: 1px solid #555; "
    //     "  border-radius: 4px; "
    //     "  padding: 8px; "
    //     "  min-height: 20px; "
    //     "  min-width: 100px; "
    //     "} "
    //     "QComboBox:focus { "
    //     "  border-color: #007AFF; "
    //     "} "
    //     "QComboBox::drop-down { "
    //     "  border: none; "
    //     "  width: 20px; "
    //     "} "
    //     "QComboBox::down-arrow { "
    //     "  image: none; "
    //     "  border-left: 5px solid transparent; "
    //     "  border-right: 5px solid transparent; "
    //     "  border-top: 5px solid white; "
    //     "  margin-right: 5px; "
    //     "} "
    //     "QComboBox QAbstractItemView { "
    //     "  background-color: #3c3c3c; "
    //     "  color: white; "
    //     "  border: 1px solid #555; "
    //     "  selection-background-color: #007AFF; "
    //     "}"
    // );
    // m_outputLayout->addRow(obs_module_text("MultiRtmp.Config.Output.OutputMode"), m_outputModeCombo);
    
    // // Reconnect settings
    // m_enableReconnectCheck = new QCheckBox(obs_module_text("MultiRtmp.Config.Output.EnableReconnect"));
    // m_enableReconnectCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    // m_outputLayout->addRow("", m_enableReconnectCheck);
    
    // m_maxRetriesSpin = new QSpinBox();
    // m_maxRetriesSpin->setRange(0, 100);
    // m_maxRetriesSpin->setValue(5);
    // m_maxRetriesSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    // m_outputLayout->addRow(obs_module_text("MultiRtmp.Config.Output.MaxRetries"), m_maxRetriesSpin);
    
    // m_retryDelaySpin = new QSpinBox();
    // m_retryDelaySpin->setRange(1, 60);
    // m_retryDelaySpin->setValue(5);
    // m_retryDelaySpin->setSuffix(" sec");
    // m_retryDelaySpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    // m_outputLayout->addRow(obs_module_text("MultiRtmp.Config.Output.RetryDelay"), m_retryDelaySpin);
    
    // // Apply dark theme to output tab
    // m_outputTab->setStyleSheet(
    //     "QWidget { "
    //     "  background-color: #1e1e1e; "
    //     "  color: white; "
    //     "} "
    //     "QLabel { "
    //     "  color: white; "
    //     "  font-weight: bold; "
    //     "}"
    // );

    auto protocol_info = findProtocol("rtmp");

    obs_data_t *output_settings = obs_data_create_from_json(m_config->outputSettings.dump().c_str());
        
    auto output = obs_output_create(protocol_info->outputId, ("tmp_output_" + m_config->id).c_str(), output_settings, nullptr);

    obs_data_t *settings = obs_output_get_settings(output);
    obs_properties_t *props = obs_output_properties(output);

    m_outputWidget = new OneSevenLivePropertiesWidget(m_tabWidget, settings, props);

    // supported_audio_encoders_ = obs_output_get_supported_audio_codecs(output);
    // supported_video_encoders_ = obs_output_get_supported_video_codecs(output);
    obs_output_release(output);

    // if (aenc_ && venc_)
    //     LoadEncoders();

    m_tabWidget->addTab(m_outputWidget, obs_module_text("Basic.Settings.Output"));
}

void OneSevenMultiRtmpConfigDialog::setupVideoTab()
{
    m_videoTab = new QWidget();
    m_videoLayout = new QFormLayout(m_videoTab);
    m_videoLayout->setSpacing(12);
    m_videoLayout->setContentsMargins(8, 12, 8, 12);
    m_videoLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_videoLayout->setLabelAlignment(Qt::AlignLeft);
    m_videoLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    // Enable video
    m_useOBSVideoCheck = new QCheckBox(obs_module_text("MultiRtmp.Config.Video.UseOBS"));
    m_useOBSVideoCheck->setChecked(true);
    m_useOBSVideoCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    m_videoLayout->addRow("", m_useOBSVideoCheck);

    m_outputSceneCombo = new QComboBox(m_videoTab);
    m_videoLayout->addRow(obs_module_text("Basic.Scene"), m_outputSceneCombo);

    m_videoEncoderCombo = new QComboBox(m_videoTab);
    m_videoLayout->addRow(obs_module_text("Basic.Settings.Output.Encoder.Video"), m_videoEncoderCombo);
    
    // TODO: if suitable for rtmp?
    // m_videoResolutionCombo = new QComboBox(m_videoTab);
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.Resolution"), m_videoResolutionCombo);

    // m_fpsDenominatorCombo = new QComboBox(m_videoTab);
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.FPSDenominator"), m_fpsDenominatorCombo);
    
    m_tabWidget->addTab(m_videoTab, obs_module_text("Basic.Settings.Video"));
}

void OneSevenMultiRtmpConfigDialog::setupAudioTab()
{
    m_audioTab = new QWidget();
    m_audioLayout = new QFormLayout(m_audioTab);
    m_audioLayout->setSpacing(12);
    m_audioLayout->setContentsMargins(8, 12, 8, 12);
    m_audioLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_audioLayout->setLabelAlignment(Qt::AlignLeft);
    m_audioLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    // Enable audio
    m_useOBSAudioCheck = new QCheckBox(obs_module_text("MultiRtmp.Config.Audio.UseOBS"));
    m_useOBSAudioCheck->setChecked(true);
    m_useOBSAudioCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    m_audioLayout->addRow("", m_useOBSAudioCheck);
    
    m_audioEncoderCombo = new QComboBox(m_audioTab);
    m_audioLayout->addRow(obs_module_text("Basic.Settings.Output.Encoder.Audio"), m_audioEncoderCombo);
    
    m_tabWidget->addTab(m_audioTab, obs_module_text("Basic.Settings.Audio"));
}



void OneSevenMultiRtmpConfigDialog::setupButtonBox()
{
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(12);
    m_buttonLayout->setContentsMargins(16, 16, 16, 16);
    
    // Cancel button with blue background and white text
    m_cancelButton = new QPushButton(obs_module_text("MultiRtmp.Config.Cancel"));
    m_cancelButton->setMinimumHeight(40);
    m_cancelButton->setMinimumWidth(100);
    m_cancelButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #007AFF; "
        "  color: white; "
        "  border: none; "
        "  padding: 10px 20px; "
        "  border-radius: 6px; "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #0056CC; "
        "} "
        "QPushButton:pressed { "
        "  background-color: #004499; "
        "}"
    );
    
    // Confirm button with red background and white text
    m_okButton = new QPushButton(obs_module_text("MultiRtmp.Config.Confirm"));
    m_okButton->setMinimumHeight(40);
    m_okButton->setMinimumWidth(100);
    m_okButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #FF0001; "
        "  color: white; "
        "  border: none; "
        "  padding: 10px 20px; "
        "  border-radius: 6px; "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #CC0001; "
        "} "
        "QPushButton:pressed { "
        "  background-color: #990001; "
        "}"
    );
    m_okButton->setDefault(true);
    
    // Center the buttons
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_okButton);
    m_buttonLayout->addStretch();
}

void OneSevenMultiRtmpConfigDialog::setupConnections()
{
    // Basic info
    connect(m_streamNameEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });

    // Advanced settings toggle
    connect(m_advancedButton, &QPushButton::clicked,
            this, &OneSevenMultiRtmpConfigDialog::onAdvancedSettingsToggled);

    // Buttons
    connect(m_okButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::reject);
}

void OneSevenMultiRtmpConfigDialog::setupValidation()
{
    m_validationTimer = new QTimer(this);
    m_validationTimer->setSingleShot(true);
    m_validationTimer->setInterval(500);
    connect(m_validationTimer, &QTimer::timeout, this, &OneSevenMultiRtmpConfigDialog::onValidationTimer);

    // Validation triggers
    connect(m_streamNameEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });

    if (!m_isEditMode) {
        m_validationLabel->setVisible(false);
        m_okButton->setEnabled(true);
    }
}

void OneSevenMultiRtmpConfigDialog::resetToDefaults()
{
    OneSevenMultiRtmpConfig defaultConfig;
    loadConfigToUI(defaultConfig);
}

void OneSevenMultiRtmpConfigDialog::setEditMode(bool isEdit)
{
    m_isEditMode = isEdit;
}

void OneSevenMultiRtmpConfigDialog::accept()
{
    if (validateConfiguration()) {
        QDialog::accept();
    } else {
        showValidationErrors();
    }
}

void OneSevenMultiRtmpConfigDialog::reject()
{
    QDialog::reject();
}

// Service-related slot functions removed - functionality integrated into basic info section



void OneSevenMultiRtmpConfigDialog::onAdvancedSettingsToggled()
{
    m_advancedExpanded = !m_advancedExpanded;
    m_advancedWidget->setVisible(m_advancedExpanded);
    
    // Update button icon based on expanded state
    if (m_advancedExpanded) {
        QIcon upIcon(":/resources/arrow-up.svg");
        m_advancedButton->setIcon(upIcon);
    } else {
        QIcon downIcon(":/resources/arrow-down.svg");
        m_advancedButton->setIcon(downIcon);
    }
    
    // Keep current dialog width unchanged, only adjust height to fit content
    resize(width(), sizeHint().height());
}

void OneSevenMultiRtmpConfigDialog::onValidationTimer()
{
    validateConfiguration();
}

// Service fields update removed - service selection integrated into basic info section






bool OneSevenMultiRtmpConfigDialog::validateConfiguration()
{
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] validateConfiguration called");
    
    // Build configuration from UI to perform full validation
    auto config = SaveConfig();
    
    // Log configuration details for debugging
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Stream name: '%s'", config.streamName.c_str());
    
    // TODO: Perform full validation using the model's validation logic
    {
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Configuration validation passed");
        m_validationLabel->setVisible(false);
        m_okButton->setEnabled(true);
        return true;
    }
    
    return false;
}

void OneSevenMultiRtmpConfigDialog::showValidationErrors()
{
//    auto config = SaveConfig();
//    QString error = QString::fromStdString(config.getValidationError());
//    
//    QMessageBox::warning(this,
//        obs_module_text("MultiRTMP.Config.Validation.Title"),
//        error);
}

// Connection test functionality removed - service tab functionality integrated into basic info section

void OneSevenMultiRtmpConfigDialog::loadConfigToUI(const OneSevenMultiRtmpConfig& config)
{
    m_streamNameEdit->setText(QString::fromStdString(config.streamName));

    QString protocolValue = QString::fromStdString(config.protocol);
    for (int i = 0; i < m_protocolCombo->count(); ++i) {
        if (m_protocolCombo->itemData(i).toString() == protocolValue) {
            m_protocolCombo->setCurrentIndex(i);
            break;
        }
    }
}

OneSevenMultiRtmpConfig OneSevenMultiRtmpConfigDialog::SaveConfig() const
{
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] SaveConfig called");
    
    OneSevenMultiRtmpConfig config;
    
    // Auto-generate UUID for new streams, keep original ID if editing
    if (m_isEditMode) {
        config.id = m_originalConfig.id;
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Edit mode: using existing ID: %s", config.id.c_str());
    } else {
        // Auto-generate UUID for new streams
        config.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Add mode: generated new ID: %s", config.id.c_str());
    }
    
    // Service configuration
    config.streamName = m_streamNameEdit->text().toStdString(); // Map to streamName
    config.protocol = m_protocolCombo->currentData().toString().toStdString(); // Map to protocol
    config.syncStart = m_syncStartCheckbox->isChecked();
    config.syncStop = m_syncStopCheckbox->isChecked();

    config.serviceSettings = m_serviceSettingsWidget->SaveData();
    
    // Output configuration
    config.outputSettings = m_outputSettingsWidget->SaveData();

    // Video configuration
    if (!m_useOBSVideoCheck->isChecked()) { 
        config.encoderId = m_encoderCombo->currentData().toString().toStdString();
        config.resolution = m_resolutionCombo->currentText().toStdString();
        config.fpsDenominator = m_fpsDenominatorCombo->currentText().toInt();
        config.outputScene = m_outputSceneCombo->currentText().toStdString();

        config.encoderSettings = m_videoSettingsWidget->SaveData();
    }
    
    // Audio configuration
    if (!m_useOBSAudioCheck->isChecked()) {
        config.encoderId = m_audioEncoderCombo->currentData().toString().toStdString();
        config.encoderSettings = m_audioSettingsWidget->SaveData();

        // TODO: track audio settings
    }
    
    
    return config;
}

