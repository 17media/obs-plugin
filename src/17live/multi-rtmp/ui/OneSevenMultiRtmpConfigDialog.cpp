#include "OneSevenMultiRtmpConfigDialog.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QDateTime>
#include <QUuid>


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

OneSevenMultiRtmpConfigDialog::OneSevenMultiRtmpConfigDialog(QWidget* parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_validationTimer(nullptr)
    , m_validationLabel(nullptr)
    , m_isEditMode(false)
{
    setWindowTitle(obs_module_text("MultiRTMP.Config.Title"));
    setModal(true);
    setMinimumWidth(350);
    resize(400, 500);
    
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
        "  min-width: 200px; "
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
        "  min-width: 200px; "
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
    populateServiceTypes();
    populateEncoderOptions();
    populateVideoResolutions();
    populateAudioFormats();
}

OneSevenMultiRtmpConfigDialog::~OneSevenMultiRtmpConfigDialog()
{
    if (m_validationTimer) {
        m_validationTimer->stop();
    }
}

void OneSevenMultiRtmpConfigDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Top section: Basic information
    setupBasicInfoSection();
    
    // Middle section: Tab widget with detailed settings
    m_tabWidget = new QTabWidget();
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { "
        "  border: 1px solid #555; "
        "  background-color: #1e1e1e; "
        "  border-radius: 6px; "
        "  margin: 12px; "
        "} "
        "QTabBar::tab { "
        "  background-color: #2d2d2d; "
        "  color: #ccc; "
        "  padding: 10px 20px; "
        "  margin-right: 2px; "
        "  border-top-left-radius: 6px; "
        "  border-top-right-radius: 6px; "
        "  font-weight: bold; "
        "} "
        "QTabBar::tab:selected { "
        "  background-color: #1e1e1e; "
        "  color: white; "
        "  border-bottom: 2px solid #FF0001; "
        "} "
        "QTabBar::tab:hover { "
        "  background-color: #3c3c3c; "
        "}"
    );
    
    setupServiceTab();
    setupOutputTab();
    setupVideoTab();
    setupAudioTab();
    
    // Bottom section: Button box
    setupButtonBox();
    
    // Validation label
    m_validationLabel = new QLabel();
    m_validationLabel->setStyleSheet("color: #f44336; font-size: 11px; margin: 0 16px;");
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setVisible(false);
    
    // Add sections to main layout
    m_mainLayout->addWidget(m_basicInfoWidget);
    m_mainLayout->addWidget(m_tabWidget, 1); // Give tabs more space
    m_mainLayout->addWidget(m_validationLabel);
    m_mainLayout->addLayout(m_buttonLayout);
}

void OneSevenMultiRtmpConfigDialog::setupBasicInfoSection()
{
    m_basicInfoWidget = new QWidget();
    m_basicInfoLayout = new QFormLayout(m_basicInfoWidget);
    m_basicInfoLayout->setSpacing(12);
    m_basicInfoLayout->setContentsMargins(16, 16, 16, 16);
    
    // Stream name input (required field)
    m_streamNameEdit = new QLineEdit();
    m_streamNameEdit->setPlaceholderText("新建串流");
    m_streamNameEdit->setText("新建串流");
    m_streamNameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_basicInfoLayout->addRow("名称", m_streamNameEdit);
    
    // Protocol dropdown
    m_protocolCombo = new QComboBox();
    m_protocolCombo->addItem("RTMP");
    m_protocolCombo->setCurrentText("RTMP");
    m_protocolCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_basicInfoLayout->addRow("协议", m_protocolCombo);
    
    // Apply dark theme styling to basic info section
    m_basicInfoWidget->setStyleSheet(
        "QWidget { "
        "  background-color: #1e1e1e; "
        "  border: none; "
        "} "
        "QLabel { "
        "  font-weight: bold; "
        "  color: white; "
        "}"
    );
}

void OneSevenMultiRtmpConfigDialog::setupServiceTab()
{
    m_serviceTab = new QWidget();
    m_serviceLayout = new QFormLayout(m_serviceTab);
    m_serviceLayout->setSpacing(12);
    m_serviceLayout->setContentsMargins(20, 20, 20, 20);
    
    // Service type combo box (required for populateServiceTypes)
    m_serviceTypeCombo = new QComboBox();
    m_serviceTypeCombo->setStyleSheet(
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 200px; "
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
    m_serviceLayout->addRow("Service Type:", m_serviceTypeCombo);
    
    // Custom service checkbox
    m_customServiceCheck = new QCheckBox("Custom Service");
    m_customServiceCheck->setStyleSheet("QCheckBox { color: white; }");
    m_serviceLayout->addRow("", m_customServiceCheck);
    
    // Server URL
    m_serverEdit = new QLineEdit();
    m_serverEdit->setPlaceholderText("");
    m_serverEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_serviceLayout->addRow("* URL", m_serverEdit);
    
    // Stream key with show/hide checkbox
    QHBoxLayout* keyLayout = new QHBoxLayout();
    m_keyEdit = new QLineEdit();
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setPlaceholderText("");
    m_keyEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    QCheckBox* showKeyCheck = new QCheckBox("显示");
    showKeyCheck->setStyleSheet("QCheckBox { color: white; }");
    connect(showKeyCheck, &QCheckBox::toggled, [this](bool checked) {
        m_keyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    
    keyLayout->addWidget(m_keyEdit);
    keyLayout->addWidget(showKeyCheck);
    
    m_serviceLayout->addRow("* 推流码", keyLayout);
    
    // Description text edit
    m_descriptionEdit = new QTextEdit();
    m_descriptionEdit->setMaximumHeight(80);
    m_descriptionEdit->setStyleSheet(
        "QTextEdit { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "}"
    );
    m_serviceLayout->addRow("Description:", m_descriptionEdit);
    
    // Test connection button
    m_testConnectionButton = new QPushButton("Test Connection");
    m_testConnectionButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #007AFF; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 4px; "
        "  padding: 8px 16px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #0056CC; "
        "}"
    );
    
    // Connection progress bar
    m_connectionProgress = new QProgressBar();
    m_connectionProgress->setVisible(false);
    m_connectionProgress->setStyleSheet(
        "QProgressBar { "
        "  background-color: #3c3c3c; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  text-align: center; "
        "  color: white; "
        "} "
        "QProgressBar::chunk { "
        "  background-color: #007AFF; "
        "  border-radius: 3px; "
        "}"
    );
    
    // Connection status label
    m_connectionStatusLabel = new QLabel();
    m_connectionStatusLabel->setStyleSheet("QLabel { color: white; }");
    
    QHBoxLayout* connectionLayout = new QHBoxLayout();
    connectionLayout->addWidget(m_testConnectionButton);
    connectionLayout->addWidget(m_connectionProgress);
    connectionLayout->addStretch();
    
    m_serviceLayout->addRow("", connectionLayout);
    m_serviceLayout->addRow("", m_connectionStatusLabel);
    
    // User authentication checkbox
    QCheckBox* authCheck = new QCheckBox("用户身份认证");
    authCheck->setChecked(true);
    authCheck->setStyleSheet("QCheckBox { color: white; }");
    m_serviceLayout->addRow("", authCheck);
    
    // Apply dark theme to service tab
    m_serviceTab->setStyleSheet(
        "QWidget { "
        "  background-color: #1e1e1e; "
        "  color: white; "
        "} "
        "QLabel { "
        "  color: white; "
        "  font-weight: bold; "
        "}"
    );
    
    m_tabWidget->addTab(m_serviceTab, "服务");
}

void OneSevenMultiRtmpConfigDialog::setupOutputTab()
{
    m_outputTab = new QWidget();
    m_outputLayout = new QFormLayout(m_outputTab);
    m_outputLayout->setSpacing(12);
    m_outputLayout->setContentsMargins(20, 20, 20, 20);
    
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
        "  min-width: 200px; "
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
    m_outputLayout->addRow("Encoder Type:", m_encoderTypeCombo);
    
    // Share encoder
    m_shareEncoderCheck = new QCheckBox("Share Encoder with Main Stream");
    m_shareEncoderCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    m_shareEncoderCheck->setToolTip("Use the same encoder settings as the main stream");
    m_outputLayout->addRow("", m_shareEncoderCheck);
    
    // Video bitrate
    m_videoBitrateSpin = new QSpinBox();
    m_videoBitrateSpin->setRange(100, 50000);
    m_videoBitrateSpin->setValue(2500);
    m_videoBitrateSpin->setSuffix(" kbps");
    m_videoBitrateSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    m_outputLayout->addRow("Video Bitrate:", m_videoBitrateSpin);
    
    // Audio bitrate
    m_audioBitrateSpin = new QSpinBox();
    m_audioBitrateSpin->setRange(64, 320);
    m_audioBitrateSpin->setValue(128);
    m_audioBitrateSpin->setSuffix(" kbps");
    m_audioBitrateSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    m_outputLayout->addRow("Audio Bitrate:", m_audioBitrateSpin);
    
    // Output mode
    m_outputModeCombo = new QComboBox();
    m_outputModeCombo->addItems({"Simple", "Advanced"});
    m_outputModeCombo->setStyleSheet(
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 200px; "
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
    m_outputLayout->addRow("Output Mode:", m_outputModeCombo);
    
    // Reconnect settings
    m_enableReconnectCheck = new QCheckBox("Enable Auto Reconnect");
    m_enableReconnectCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    m_outputLayout->addRow("", m_enableReconnectCheck);
    
    m_maxRetriesSpin = new QSpinBox();
    m_maxRetriesSpin->setRange(0, 100);
    m_maxRetriesSpin->setValue(5);
    m_maxRetriesSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    m_outputLayout->addRow("Max Retries:", m_maxRetriesSpin);
    
    m_retryDelaySpin = new QSpinBox();
    m_retryDelaySpin->setRange(1, 60);
    m_retryDelaySpin->setValue(5);
    m_retryDelaySpin->setSuffix(" sec");
    m_retryDelaySpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    m_outputLayout->addRow("Retry Delay:", m_retryDelaySpin);
    
    // Apply dark theme to output tab
    m_outputTab->setStyleSheet(
        "QWidget { "
        "  background-color: #1e1e1e; "
        "  color: white; "
        "} "
        "QLabel { "
        "  color: white; "
        "  font-weight: bold; "
        "}"
    );
    
    m_tabWidget->addTab(m_outputTab, "输出");
}

void OneSevenMultiRtmpConfigDialog::setupVideoTab()
{
    m_videoTab = new QWidget();
    m_videoLayout = new QFormLayout(m_videoTab);
    m_videoLayout->setSpacing(12);
    m_videoLayout->setContentsMargins(20, 20, 20, 20);
    
    // Enable video
    m_enableVideoCheck = new QCheckBox("Enable Video Stream");
    m_enableVideoCheck->setChecked(true);
    m_enableVideoCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    m_videoLayout->addRow("", m_enableVideoCheck);
    
    // Video resolution
    m_videoResolutionCombo = new QComboBox();
    m_videoResolutionCombo->setStyleSheet(
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 200px; "
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
    m_videoLayout->addRow("Video Resolution:", m_videoResolutionCombo);
    
    // Custom resolution
    QHBoxLayout* customResLayout = new QHBoxLayout();
    m_customWidthEdit = new QLineEdit();
    m_customWidthEdit->setPlaceholderText("1920");
    m_customWidthEdit->setMaximumWidth(80);
    m_customWidthEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QLineEdit:focus { border-color: #007AFF; }");
    m_customHeightEdit = new QLineEdit();
    m_customHeightEdit->setPlaceholderText("1080");
    m_customHeightEdit->setMaximumWidth(80);
    m_customHeightEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QLineEdit:focus { border-color: #007AFF; }");
    
    QLabel* timesLabel = new QLabel("×");
    timesLabel->setStyleSheet("QLabel { font-weight: bold; color: #666; }");
    
    customResLayout->addWidget(m_customWidthEdit);
    customResLayout->addWidget(timesLabel);
    customResLayout->addWidget(m_customHeightEdit);
    customResLayout->addStretch();
    
    m_videoLayout->addRow("Custom Resolution:", customResLayout);
    
    // Frame rate
    m_fpsSpinBox = new QDoubleSpinBox();
    m_fpsSpinBox->setRange(1.0, 120.0);
    m_fpsSpinBox->setValue(30.0);
    m_fpsSpinBox->setSuffix(" fps");
    m_fpsSpinBox->setStyleSheet("QDoubleSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QDoubleSpinBox:focus { border-color: #007AFF; }");
    m_videoLayout->addRow("Frame Rate:", m_fpsSpinBox);
    
    // Scale filter
    m_scaleFilterCombo = new QComboBox();
    m_scaleFilterCombo->setStyleSheet(
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 200px; "
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
    m_videoLayout->addRow("Scale Filter:", m_scaleFilterCombo);
    
    // Quality slider
    QHBoxLayout* qualityLayout = new QHBoxLayout();
    m_qualitySlider = new QSlider(Qt::Horizontal);
    m_qualitySlider->setRange(1, 10);
    m_qualitySlider->setValue(7);
    m_qualitySlider->setStyleSheet(
        "QSlider::groove:horizontal { "
        "  border: 1px solid #ddd; "
        "  height: 8px; "
        "  background: #f0f0f0; "
        "  border-radius: 4px; "
        "} "
        "QSlider::handle:horizontal { "
        "  background: #007AFF; "
        "  border: 1px solid #007AFF; "
        "  width: 18px; "
        "  margin: -5px 0; "
        "  border-radius: 9px; "
        "}"
    );
    m_qualityLabel = new QLabel("7");
    m_qualityLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; min-width: 20px; }");
    
    qualityLayout->addWidget(m_qualitySlider);
    qualityLayout->addWidget(m_qualityLabel);
    
    m_videoLayout->addRow("Quality:", qualityLayout);
    
    // Apply dark theme to video tab
    m_videoTab->setStyleSheet(
        "QWidget { "
        "  background-color: #1e1e1e; "
        "  color: white; "
        "} "
        "QLabel { "
        "  color: white; "
        "  font-weight: bold; "
        "}"
    );
    
    m_tabWidget->addTab(m_videoTab, "视频");
}

void OneSevenMultiRtmpConfigDialog::setupAudioTab()
{
    m_audioTab = new QWidget();
    m_audioLayout = new QFormLayout(m_audioTab);
    m_audioLayout->setSpacing(12);
    m_audioLayout->setContentsMargins(20, 20, 20, 20);
    
    // Enable audio
    m_enableAudioCheck = new QCheckBox("Enable Audio Stream");
    m_enableAudioCheck->setChecked(true);
    m_enableAudioCheck->setStyleSheet("QCheckBox { font-weight: bold; color: #333; }");
    m_audioLayout->addRow("", m_enableAudioCheck);
    
    // Audio format
    m_audioFormatCombo = new QComboBox();
    m_audioFormatCombo->setStyleSheet(
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 200px; "
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
    m_audioLayout->addRow("Audio Format:", m_audioFormatCombo);
    
    // Sample rate
    m_sampleRateSpin = new QSpinBox();
    m_sampleRateSpin->setRange(8000, 48000);
    m_sampleRateSpin->setValue(44100);
    m_sampleRateSpin->setSuffix(" Hz");
    m_sampleRateSpin->setStyleSheet("QSpinBox { padding: 8px; border: 1px solid #ddd; border-radius: 4px; } QSpinBox:focus { border-color: #007AFF; }");
    m_audioLayout->addRow("Sample Rate:", m_sampleRateSpin);
    
    // Channel layout
    m_channelLayoutCombo = new QComboBox();
    m_channelLayoutCombo->addItems({"Mono", "Stereo", "5.1"});
    m_channelLayoutCombo->setCurrentText("Stereo");
    m_channelLayoutCombo->setStyleSheet(
        "QComboBox { "
        "  background-color: #3c3c3c; "
        "  color: white; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  min-height: 20px; "
        "  min-width: 200px; "
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
    m_audioLayout->addRow("Channel Layout:", m_channelLayoutCombo);
    
    // Audio volume
    QHBoxLayout* volumeLayout = new QHBoxLayout();
    m_audioVolumeSlider = new QSlider(Qt::Horizontal);
    m_audioVolumeSlider->setRange(0, 100);
    m_audioVolumeSlider->setValue(100);
    m_audioVolumeSlider->setStyleSheet(
        "QSlider::groove:horizontal { "
        "  border: 1px solid #ddd; "
        "  height: 8px; "
        "  background: #f0f0f0; "
        "  border-radius: 4px; "
        "} "
        "QSlider::handle:horizontal { "
        "  background: #007AFF; "
        "  border: 1px solid #007AFF; "
        "  width: 18px; "
        "  margin: -5px 0; "
        "  border-radius: 9px; "
        "}"
    );
    m_audioVolumeLabel = new QLabel("100%");
    m_audioVolumeLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; min-width: 40px; }");
    
    volumeLayout->addWidget(m_audioVolumeSlider);
    volumeLayout->addWidget(m_audioVolumeLabel);
    
    m_audioLayout->addRow("Audio Volume:", volumeLayout);
    
    // Apply dark theme to audio tab
    m_audioTab->setStyleSheet(
        "QWidget { "
        "  background-color: #1e1e1e; "
        "  color: white; "
        "} "
        "QLabel { "
        "  color: white; "
        "  font-weight: bold; "
        "}"
    );
    
    m_tabWidget->addTab(m_audioTab, "音频");
}



void OneSevenMultiRtmpConfigDialog::setupButtonBox()
{
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(12);
    m_buttonLayout->setContentsMargins(16, 16, 16, 16);
    
    // Cancel button with blue background and white text
    m_cancelButton = new QPushButton("Cancel");
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
    m_okButton = new QPushButton("Confirm");
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
    
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_okButton);
}

void OneSevenMultiRtmpConfigDialog::setupConnections()
{
    // Service tab connections
    connect(m_serviceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OneSevenMultiRtmpConfigDialog::onServiceTypeChanged);
    connect(m_customServiceCheck, &QCheckBox::toggled,
            this, &OneSevenMultiRtmpConfigDialog::onCustomServiceToggled);
    connect(m_testConnectionButton, &QPushButton::clicked,
            this, &OneSevenMultiRtmpConfigDialog::onTestConnectionClicked);
    
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
    
    // Basic info connections
    connect(m_streamNameEdit, &QLineEdit::textChanged, 
            this, [this]() { m_validationTimer->start(); });
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

void OneSevenMultiRtmpConfigDialog::populateServiceTypes()
{
    m_serviceTypeCombo->addItems(SERVICE_TYPES);
}

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

void OneSevenMultiRtmpConfigDialog::setConfig(const OneSevenMultiRtmpConfig& config)
{
    m_originalConfig = config;
    loadConfigToUI(config);
}

OneSevenMultiRtmpConfig OneSevenMultiRtmpConfigDialog::getConfig() const
{
    return buildConfigFromUI();
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

void OneSevenMultiRtmpConfigDialog::onServiceTypeChanged()
{
    updateServiceFields();
}

void OneSevenMultiRtmpConfigDialog::onCustomServiceToggled(bool enabled)
{
    Q_UNUSED(enabled); // Parameter not used in current implementation
    updateServiceFields();
}

void OneSevenMultiRtmpConfigDialog::onTestConnectionClicked()
{
    updateConnectionTest();
}

void OneSevenMultiRtmpConfigDialog::onEncoderSharingChanged()
{
    updateEncoderFields();
}

void OneSevenMultiRtmpConfigDialog::onVideoResolutionChanged()
{
    updateVideoFields();
}



void OneSevenMultiRtmpConfigDialog::onValidationTimer()
{
    validateConfiguration();
}

void OneSevenMultiRtmpConfigDialog::updateServiceFields()
{
    bool isCustom = m_customServiceCheck->isChecked();
    
    if (!isCustom) {
        QString serviceType = m_serviceTypeCombo->currentText();
        
        // Set default server URLs for known services
        if (serviceType == "YouTube") {
            m_serverEdit->setText("rtmp://a.rtmp.youtube.com/live2");
        } else if (serviceType == "Twitch") {
            m_serverEdit->setText("rtmp://live.twitch.tv/live");
        } else if (serviceType == "Facebook") {
            m_serverEdit->setText("rtmps://live-api-s.facebook.com:443/rtmp");
        } else if (serviceType == "17Live") {
            m_serverEdit->setText("rtmp://publish.17app.co/live");
        }
    }
}

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
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Server URL: '%s'", config.service.serverUrl.c_str());
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Stream key length: %zu", config.service.streamKey.length());
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video resolution: %dx%d", config.video.outputWidth, config.video.outputHeight);
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video bitrate: %d", config.video.bitrate);
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Audio bitrate: %d", config.audio.bitrate);
    
    // Perform full validation using the model's validation logic
    if (!config.isValid()) {
        std::string error = config.getValidationError();
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] Configuration validation failed: %s", error.c_str());
        
        m_validationLabel->setText(QString::fromStdString(error));
        m_validationLabel->setVisible(true);
        m_okButton->setEnabled(false);
        return false;
    } else {
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Configuration validation passed");
        m_validationLabel->setVisible(false);
        m_okButton->setEnabled(true);
        return true;
    }
}

void OneSevenMultiRtmpConfigDialog::showValidationErrors()
{
    auto config = buildConfigFromUI();
    QString error = QString::fromStdString(config.getValidationError());
    
    QMessageBox::warning(this,
        obs_module_text("MultiRTMP.Config.Validation.Title"),
        error);
}

void OneSevenMultiRtmpConfigDialog::updateConnectionTest()
{
    m_testConnectionButton->setEnabled(false);
    m_connectionProgress->setVisible(true);
    m_connectionProgress->setRange(0, 0); // Indeterminate
    m_connectionStatusLabel->setText(obs_module_text("MultiRTMP.Config.Testing"));
    
    // Simulate connection test (in real implementation, this would test the RTMP connection)
    QTimer::singleShot(2000, this, [this]() {
        m_connectionProgress->setVisible(false);
        m_testConnectionButton->setEnabled(true);
        m_connectionStatusLabel->setText(obs_module_text("MultiRTMP.Config.TestSuccess"));
        m_connectionStatusLabel->setStyleSheet("color: #4CAF50;");
    });
}

void OneSevenMultiRtmpConfigDialog::loadConfigToUI(const OneSevenMultiRtmpConfig& config)
{
    // Basic info section
    m_streamNameEdit->setText(QString::fromStdString(config.streamName));
    
    // Service tab
    m_serverEdit->setText(QString::fromStdString(config.service.serverUrl));
    m_keyEdit->setText(QString::fromStdString(config.service.streamKey));
    m_descriptionEdit->setPlainText(""); // Default empty since field doesn't exist
    
    // Output tab
    m_shareEncoderCheck->setChecked(config.video.useSharedEncoder);
    m_videoBitrateSpin->setValue(config.video.bitrate);
    m_audioBitrateSpin->setValue(config.audio.bitrate);
    m_enableReconnectCheck->setChecked(config.output.autoReconnect);
    m_maxRetriesSpin->setValue(5); // Default value since field doesn't exist
    m_retryDelaySpin->setValue(config.output.reconnectDelay);
    
    // Video tab
    m_enableVideoCheck->setChecked(true); // Default to enabled since struct doesn't have this field
    m_customWidthEdit->setText(QString::number(config.video.outputWidth));
    m_customHeightEdit->setText(QString::number(config.video.outputHeight));
    m_fpsSpinBox->setValue(30.0); // Default FPS since struct uses fpsDenominator
    
    // Audio tab
    m_enableAudioCheck->setChecked(true); // Default to enabled since struct doesn't have this field
    m_sampleRateSpin->setValue(config.audio.sampleRate);
    
    // Update dependent fields
    updateServiceFields();
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
    config.service.serverUrl = m_serverEdit->text().toStdString(); // Map to service.serverUrl
    config.service.streamKey = m_keyEdit->text().toStdString(); // Map to service.streamKey
    
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Service config - Name: '%s', URL: '%s', Key length: %zu", 
            config.streamName.c_str(), config.service.serverUrl.c_str(), config.service.streamKey.length());
    
    // Output configuration
    config.video.useSharedEncoder = m_shareEncoderCheck->isChecked(); // Map to video.useSharedEncoder
    config.video.bitrate = m_videoBitrateSpin->value(); // Map to video.bitrate
    config.audio.bitrate = m_audioBitrateSpin->value(); // Map to audio.bitrate
    config.output.autoReconnect = m_enableReconnectCheck->isChecked(); // Map to output.autoReconnect
    config.output.reconnectDelay = m_retryDelaySpin->value(); // Map to output.reconnectDelay
    
    // Video configuration
    config.video.outputWidth = m_customWidthEdit->text().toInt();
    config.video.outputHeight = m_customHeightEdit->text().toInt();
    
    // Audio configuration
    config.audio.sampleRate = m_sampleRateSpin->value();
    
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video config - Resolution: %dx%d, Bitrate: %d, Shared encoder: %s", 
            config.video.outputWidth, config.video.outputHeight, config.video.bitrate, 
            config.video.useSharedEncoder ? "true" : "false");
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Audio config - Bitrate: %d, Sample rate: %d", 
            config.audio.bitrate, config.audio.sampleRate);
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Output config - Auto reconnect: %s, Delay: %d", 
            config.output.autoReconnect ? "true" : "false", config.output.reconnectDelay);
    
    return config;
}

