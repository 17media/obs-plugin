#include "OneSevenMultiRtmpConfigDialog.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QDateTime>
#include <QUuid>
#include <QScrollArea>

#include "../../ui/OneSevenLivePropertiesWidget.hpp"

#include "moc_OneSevenMultiRtmpConfigDialog.cpp"


// Static constants for UI options
const QStringList OneSevenMultiRtmpConfigDialog::SERVICE_TYPES = {
    "Custom RTMP",
    "YouTube",
    "Twitch",
    "Facebook",
    "17Live"
};

const QStringList OneSevenMultiRtmpConfigDialog::ENCODER_TYPES = {
    "x264",
    "NVENC",
    "AMD",
    "QuickSync"
};

const QStringList OneSevenMultiRtmpConfigDialog::VIDEO_RESOLUTIONS = {
    "1920x1080",
    "1280x720",
    "854x480",
    "640x360",
    "Custom"
};

const QStringList OneSevenMultiRtmpConfigDialog::AUDIO_FORMATS = {
    "AAC",
    "MP3",
    "Opus"
};

const QStringList OneSevenMultiRtmpConfigDialog::SCALE_FILTERS = {
    "Bilinear",
    "Bicubic",
    "Lanczos"
};

const QStringList OneSevenMultiRtmpConfigDialog::SYNC_MODES = {
    "None",
    "Start Only",
    "Stop Only",
    "Both"
};

const QStringList OneSevenMultiRtmpConfigDialog::LOG_LEVELS = {
    "Error",
    "Warning",
    "Info",
    "Debug"
};

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
    
    // Populate combo boxes
    populateEncoderOptions();
    populateVideoResolutions();
    populateAudioFormats();
    
    // Load configuration if provided
    if (m_config) {
        loadConfigToUI(*m_config);
    }
}

OneSevenMultiRtmpConfigDialog::~OneSevenMultiRtmpConfigDialog()
{
    if (m_validationTimer) {
        m_validationTimer->stop();
    }
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
    
    // Validation label
    m_validationLabel = new QLabel();
    m_validationLabel->setStyleSheet("color: #f44336; font-size: 11px; margin: 0;");
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setVisible(false);
    
    // Add sections to container layout
    containerLayout->addWidget(m_basicInfoWidget);
    containerLayout->addWidget(m_advancedButton);
    containerLayout->addWidget(m_advancedWidget);
    containerLayout->addWidget(m_validationLabel);
    containerLayout->addStretch(); // Add stretch to push content to top
    
    // Set container as scroll area content
    scrollArea->setWidget(container);
    
    // Bottom section: Button box (outside scroll area)
    setupButtonBox();
    
    // Add scroll area and button layout to main layout
    m_mainLayout->addWidget(scrollArea);
    m_mainLayout->addLayout(m_buttonLayout);
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
    // m_outputTab = new QWidget();
    // m_outputLayout = new QFormLayout(m_outputTab);
    // m_outputLayout->setSpacing(12);
    // m_outputLayout->setContentsMargins(8, 12, 8, 12);
    // m_outputLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // m_outputLayout->setLabelAlignment(Qt::AlignLeft);
    // m_outputLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    // // Encoder type
    // m_encoderTypeCombo = new QComboBox();
    // m_encoderTypeCombo->setStyleSheet(
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
    // m_outputLayout->addRow(obs_module_text("MultiRtmp.Config.Output.EncoderType"), m_encoderTypeCombo);
    
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

    m_tabWidget->addTab(m_outputWidget, obs_module_text("MultiRtmp.Config.Tab.Output"));
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
    // m_enableVideoCheck = new QCheckBox(obs_module_text("MultiRtmp.Config.Video.EnableVideo"));
    // m_enableVideoCheck->setChecked(true);
    // m_enableVideoCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    // m_videoLayout->addRow("", m_enableVideoCheck);
    
    // // Video resolution
    // m_videoResolutionCombo = new QComboBox();
    // m_videoResolutionCombo->setStyleSheet(
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
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.Resolution"), m_videoResolutionCombo);
    
    // // Custom resolution
    // QHBoxLayout* customResLayout = new QHBoxLayout();
    // m_customWidthEdit = new QLineEdit();
    // m_customWidthEdit->setPlaceholderText("1920");
    // m_customWidthEdit->setMaximumWidth(80);
    // m_customWidthEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QLineEdit:focus { border-color: #007AFF; }");
    // m_customHeightEdit = new QLineEdit();
    // m_customHeightEdit->setPlaceholderText("1080");
    // m_customHeightEdit->setMaximumWidth(80);
    // m_customHeightEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QLineEdit:focus { border-color: #007AFF; }");
    
    // QLabel* timesLabel = new QLabel("×");
    // timesLabel->setStyleSheet("QLabel { font-weight: bold; color: #666; }");
    
    // customResLayout->addWidget(m_customWidthEdit);
    // customResLayout->addWidget(timesLabel);
    // customResLayout->addWidget(m_customHeightEdit);
    // customResLayout->addStretch();
    
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.CustomResolution"), customResLayout);
    
    // // Frame rate
    // m_fpsSpinBox = new QDoubleSpinBox();
    // m_fpsSpinBox->setRange(1.0, 120.0);
    // m_fpsSpinBox->setValue(30.0);
    // m_fpsSpinBox->setSuffix(" fps");
    // m_fpsSpinBox->setStyleSheet("QDoubleSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QDoubleSpinBox:focus { border-color: #007AFF; }");
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.FrameRate"), m_fpsSpinBox);
    
    // // Scale filter
    // m_scaleFilterCombo = new QComboBox();
    // m_scaleFilterCombo->setStyleSheet(
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
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.ScaleFilter"), m_scaleFilterCombo);
    
    // // Quality slider
    // QHBoxLayout* qualityLayout = new QHBoxLayout();
    // m_qualitySlider = new QSlider(Qt::Horizontal);
    // m_qualitySlider->setRange(1, 10);
    // m_qualitySlider->setValue(7);
    // m_qualitySlider->setStyleSheet(
    //     "QSlider::groove:horizontal { "
    //     "  border: 1px solid #ddd; "
    //     "  height: 8px; "
    //     "  background: #f0f0f0; "
    //     "  border-radius: 4px; "
    //     "} "
    //     "QSlider::handle:horizontal { "
    //     "  background: #007AFF; "
    //     "  border: 1px solid #007AFF; "
    //     "  width: 18px; "
    //     "  margin: -5px 0; "
    //     "  border-radius: 9px; "
    //     "}"
    // );
    // m_qualityLabel = new QLabel("7");
    // m_qualityLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; min-width: 20px; }");
    
    // qualityLayout->addWidget(m_qualitySlider);
    // qualityLayout->addWidget(m_qualityLabel);
    
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.Quality"), qualityLayout);
    
    // // Apply dark theme to video tab
    // m_videoTab->setStyleSheet(
    //     "QWidget { "
    //     "  background-color: #1e1e1e; "
    //     "  color: white; "
    //     "} "
    //     "QLabel { "
    //     "  color: white; "
    //     "  font-weight: bold; "
    //     "}"
    // );
    
    m_tabWidget->addTab(m_videoTab, obs_module_text("MultiRtmp.Config.Tab.Video"));
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
    
    // // Enable audio
    // m_enableAudioCheck = new QCheckBox(obs_module_text("MultiRtmp.Config.Audio.EnableAudio"));
    // m_enableAudioCheck->setChecked(true);
    // m_enableAudioCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    // m_audioLayout->addRow("", m_enableAudioCheck);
    
    // // Audio format
    // m_audioFormatCombo = new QComboBox();
    // m_audioFormatCombo->setStyleSheet(
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
    // m_audioLayout->addRow(obs_module_text("MultiRtmp.Config.Audio.Format"), m_audioFormatCombo);
    
    // // Sample rate
    // m_sampleRateSpin = new QSpinBox();
    // m_sampleRateSpin->setRange(8000, 48000);
    // m_sampleRateSpin->setValue(44100);
    // m_sampleRateSpin->setSuffix(" Hz");
    // m_sampleRateSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    // m_audioLayout->addRow(obs_module_text("MultiRtmp.Config.Audio.SampleRate"), m_sampleRateSpin);
    
    // // Channel layout
    // m_channelLayoutCombo = new QComboBox();
    // m_channelLayoutCombo->addItems({"Mono", "Stereo", "5.1"});
    // m_channelLayoutCombo->setCurrentText("Stereo");
    // m_channelLayoutCombo->setStyleSheet(
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
    // m_audioLayout->addRow(obs_module_text("MultiRtmp.Config.Audio.Channels"), m_channelLayoutCombo);
    
    // // Audio volume
    // QHBoxLayout* volumeLayout = new QHBoxLayout();
    // m_audioVolumeSlider = new QSlider(Qt::Horizontal);
    // m_audioVolumeSlider->setRange(0, 100);
    // m_audioVolumeSlider->setValue(100);
    // m_audioVolumeSlider->setStyleSheet(
    //     "QSlider::groove:horizontal { "
    //     "  border: 1px solid #ddd; "
    //     "  height: 8px; "
    //     "  background: #f0f0f0; "
    //     "  border-radius: 4px; "
    //     "} "
    //     "QSlider::handle:horizontal { "
    //     "  background: #007AFF; "
    //     "  border: 1px solid #007AFF; "
    //     "  width: 18px; "
    //     "  margin: -5px 0; "
    //     "  border-radius: 9px; "
    //     "}"
    // );
    // m_audioVolumeLabel = new QLabel("100%");
    // m_audioVolumeLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; min-width: 40px; }");
    
    // volumeLayout->addWidget(m_audioVolumeSlider);
    // volumeLayout->addWidget(m_audioVolumeLabel);
    
    // m_audioLayout->addRow(obs_module_text("MultiRtmp.Config.Audio.Volume"), volumeLayout);
    
    // // Apply dark theme to audio tab
    // m_audioTab->setStyleSheet(
    //     "QWidget { "
    //     "  background-color: #1e1e1e; "
    //     "  color: white; "
    //     "} "
    //     "QLabel { "
    //     "  color: white; "
    //     "  font-weight: bold; "
    //     "}"
    // );
    
    m_tabWidget->addTab(m_audioTab, obs_module_text("MultiRtmp.Config.Tab.Audio"));
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
    // Basic info connections
    connect(m_streamNameEdit, &QLineEdit::textChanged, 
            this, [this]() { m_validationTimer->start(); });
    connect(m_serverEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });
    connect(m_keyEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });
    
    // Stream key visibility toggle
    connect(m_showKeyCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                m_keyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
            });
    
    // Advanced settings toggle
    connect(m_advancedButton, &QPushButton::clicked,
            this, &OneSevenMultiRtmpConfigDialog::onAdvancedSettingsToggled);
    
    // Output tab connections
    connect(m_shareEncoderCheck, &QCheckBox::toggled,
            this, &OneSevenMultiRtmpConfigDialog::onEncoderSharingChanged);
    
    // Video tab connections
    connect(m_videoResolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OneSevenMultiRtmpConfigDialog::onVideoResolutionChanged);
    connect(m_qualitySlider, &QSlider::valueChanged,
            this, [this](int value) { m_qualityLabel->setText(QString::number(value)); });
    
    // Audio tab connections
    connect(m_audioVolumeSlider, &QSlider::valueChanged,
            this, [this](int value) { m_audioVolumeLabel->setText(QString("%1%").arg(value)); });
    
    // Button connections
    connect(m_okButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::reject);
}

void OneSevenMultiRtmpConfigDialog::setupValidation()
{
    m_validationTimer = new QTimer(this);
    m_validationTimer->setSingleShot(true);
    m_validationTimer->setInterval(500); // 500ms delay
    connect(m_validationTimer, &QTimer::timeout, this, &OneSevenMultiRtmpConfigDialog::onValidationTimer);
    
    // Connect validation triggers to input fields
    connect(m_streamNameEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });
    connect(m_serverEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });
    connect(m_keyEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });
    
    // Initially hide validation label and enable OK button for new streams
    if (!m_isEditMode) {
        m_validationLabel->setVisible(false);
        m_okButton->setEnabled(true);
    }
}

// Service types population removed - service tab functionality integrated into basic info

void OneSevenMultiRtmpConfigDialog::populateEncoderOptions()
{
    m_encoderTypeCombo->addItems(ENCODER_TYPES);
}

void OneSevenMultiRtmpConfigDialog::populateVideoResolutions()
{
    m_videoResolutionCombo->addItems(VIDEO_RESOLUTIONS);
    m_scaleFilterCombo->addItems(SCALE_FILTERS);
}

void OneSevenMultiRtmpConfigDialog::populateAudioFormats()
{
    m_audioFormatCombo->addItems(AUDIO_FORMATS);
    // Note: m_syncModeCombo and m_logLevelCombo are declared but not initialized
    // Commenting out to prevent null pointer access crash
    // TODO: Initialize these ComboBoxes if they are needed in the UI
    // m_syncModeCombo->addItems(SYNC_MODES);
    // m_logLevelCombo->addItems(LOG_LEVELS);
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

void OneSevenMultiRtmpConfigDialog::onEncoderSharingChanged()
{
    updateEncoderFields();
}

void OneSevenMultiRtmpConfigDialog::onVideoResolutionChanged()
{
    updateVideoFields();
}

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

void OneSevenMultiRtmpConfigDialog::updateEncoderFields()
{
    bool shareEncoder = m_shareEncoderCheck->isChecked();
    m_encoderTypeCombo->setEnabled(!shareEncoder);
    
    if (shareEncoder) {
        m_encoderTypeCombo->setCurrentText("x264"); // Default shared encoder
    }
}

void OneSevenMultiRtmpConfigDialog::updateVideoFields()
{
    bool isCustom = (m_videoResolutionCombo->currentText() == "Custom");
    m_customWidthEdit->setEnabled(isCustom);
    m_customHeightEdit->setEnabled(isCustom);
    
    if (!isCustom) {
        QString resolution = m_videoResolutionCombo->currentText();
        QStringList parts = resolution.split('x');
        if (parts.size() == 2) {
            m_customWidthEdit->setText(parts[0]);
            m_customHeightEdit->setText(parts[1]);
        }
    }
}

void OneSevenMultiRtmpConfigDialog::updateAudioFields()
{
    // Update audio-related fields based on current settings
}



bool OneSevenMultiRtmpConfigDialog::validateConfiguration()
{
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] validateConfiguration called");
    
    // Build configuration from UI to perform full validation
    auto config = buildConfigFromUI();
    
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
//    auto config = buildConfigFromUI();
//    QString error = QString::fromStdString(config.getValidationError());
//    
//    QMessageBox::warning(this,
//        obs_module_text("MultiRTMP.Config.Validation.Title"),
//        error);
}

// Connection test functionality removed - service tab functionality integrated into basic info section

void OneSevenMultiRtmpConfigDialog::loadConfigToUI(const OneSevenMultiRtmpConfig& config)
{
    // Basic info section
    m_streamNameEdit->setText(QString::fromStdString(config.streamName));
    
    // Set protocol combo box
    QString protocolValue = QString::fromStdString(config.protocol);
    for (int i = 0; i < m_protocolCombo->count(); ++i) {
        if (m_protocolCombo->itemData(i).toString() == protocolValue) {
            m_protocolCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // Output tab
    
    // Video tab
    
    // Audio tab
    
    // Update dependent fields
    updateEncoderFields();
    updateVideoFields();
}

OneSevenMultiRtmpConfig OneSevenMultiRtmpConfigDialog::buildConfigFromUI() const
{
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] buildConfigFromUI called");
    
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
    
    
    // Output configuration
    
    // Video configuration
    
    // Audio configuration
    
    
    return config;
}

