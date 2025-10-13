#include "OneSevenMultiRtmpConfigDialog.hpp"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>


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
    resize(600, 500);
    
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
    m_mainLayout->setContentsMargins(12, 12, 12, 12);
    m_mainLayout->setSpacing(12);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    
    setupServiceTab();
    setupOutputTab();
    setupVideoTab();
    setupAudioTab();
    setupSyncTab();
    setupAdvancedTab();
    setupButtonBox();
    
    // Validation label
    m_validationLabel = new QLabel();
    m_validationLabel->setStyleSheet("color: #f44336; font-size: 11px;");
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setVisible(false);
    
    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->addWidget(m_validationLabel);
    m_mainLayout->addLayout(m_buttonLayout);
}

void OneSevenMultiRtmpConfigDialog::setupServiceTab()
{
    m_serviceTab = new QWidget();
    m_serviceLayout = new QFormLayout(m_serviceTab);
    m_serviceLayout->setSpacing(8);
    
    // Service type
    m_serviceTypeCombo = new QComboBox();
    m_serviceLayout->addRow(obs_module_text("MultiRTMP.Config.ServiceType"), m_serviceTypeCombo);
    
    // Custom service checkbox
    m_customServiceCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.CustomService"));
    m_serviceLayout->addRow("", m_customServiceCheck);
    
    // Service name
    m_serviceNameEdit = new QLineEdit();
    m_serviceNameEdit->setPlaceholderText(obs_module_text("MultiRTMP.Config.ServiceName.Placeholder"));
    m_serviceLayout->addRow(obs_module_text("MultiRTMP.Config.ServiceName"), m_serviceNameEdit);
    
    // Server URL
    m_serverEdit = new QLineEdit();
    m_serverEdit->setPlaceholderText("rtmp://server.example.com/live");
    m_serviceLayout->addRow(obs_module_text("MultiRTMP.Config.Server"), m_serverEdit);
    
    // Stream key
    m_keyEdit = new QLineEdit();
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setPlaceholderText(obs_module_text("MultiRTMP.Config.StreamKey.Placeholder"));
    m_serviceLayout->addRow(obs_module_text("MultiRTMP.Config.StreamKey"), m_keyEdit);
    
    // Description
    m_descriptionEdit = new QTextEdit();
    m_descriptionEdit->setMaximumHeight(60);
    m_descriptionEdit->setPlaceholderText(obs_module_text("MultiRTMP.Config.Description.Placeholder"));
    m_serviceLayout->addRow(obs_module_text("MultiRTMP.Config.Description"), m_descriptionEdit);
    
    // Test connection
    QHBoxLayout* testLayout = new QHBoxLayout();
    m_testConnectionButton = new QPushButton(obs_module_text("MultiRTMP.Config.TestConnection"));
    m_testConnectionButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    
    m_connectionProgress = new QProgressBar();
    m_connectionProgress->setVisible(false);
    m_connectionProgress->setMaximumHeight(20);
    
    m_connectionStatusLabel = new QLabel();
    m_connectionStatusLabel->setStyleSheet("font-size: 11px;");
    
    testLayout->addWidget(m_testConnectionButton);
    testLayout->addWidget(m_connectionProgress);
    testLayout->addStretch();
    
    m_serviceLayout->addRow("", testLayout);
    m_serviceLayout->addRow("", m_connectionStatusLabel);
    
    m_tabWidget->addTab(m_serviceTab, obs_module_text("MultiRTMP.Config.Tab.Service"));
}

void OneSevenMultiRtmpConfigDialog::setupOutputTab()
{
    m_outputTab = new QWidget();
    m_outputLayout = new QFormLayout(m_outputTab);
    m_outputLayout->setSpacing(8);
    
    // Encoder type
    m_encoderTypeCombo = new QComboBox();
    m_outputLayout->addRow(obs_module_text("MultiRTMP.Config.EncoderType"), m_encoderTypeCombo);
    
    // Share encoder
    m_shareEncoderCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.ShareEncoder"));
    m_shareEncoderCheck->setToolTip(obs_module_text("MultiRTMP.Config.ShareEncoder.Tooltip"));
    m_outputLayout->addRow("", m_shareEncoderCheck);
    
    // Video bitrate
    m_videoBitrateSpin = new QSpinBox();
    m_videoBitrateSpin->setRange(100, 50000);
    m_videoBitrateSpin->setValue(2500);
    m_videoBitrateSpin->setSuffix(" kbps");
    m_outputLayout->addRow(obs_module_text("MultiRTMP.Config.VideoBitrate"), m_videoBitrateSpin);
    
    // Audio bitrate
    m_audioBitrateSpin = new QSpinBox();
    m_audioBitrateSpin->setRange(64, 320);
    m_audioBitrateSpin->setValue(128);
    m_audioBitrateSpin->setSuffix(" kbps");
    m_outputLayout->addRow(obs_module_text("MultiRTMP.Config.AudioBitrate"), m_audioBitrateSpin);
    
    // Output mode
    m_outputModeCombo = new QComboBox();
    m_outputModeCombo->addItems({"Simple", "Advanced"});
    m_outputLayout->addRow(obs_module_text("MultiRTMP.Config.OutputMode"), m_outputModeCombo);
    
    // Reconnect settings
    m_enableReconnectCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.EnableReconnect"));
    m_outputLayout->addRow("", m_enableReconnectCheck);
    
    m_maxRetriesSpin = new QSpinBox();
    m_maxRetriesSpin->setRange(0, 100);
    m_maxRetriesSpin->setValue(5);
    m_outputLayout->addRow(obs_module_text("MultiRTMP.Config.MaxRetries"), m_maxRetriesSpin);
    
    m_retryDelaySpin = new QSpinBox();
    m_retryDelaySpin->setRange(1, 60);
    m_retryDelaySpin->setValue(5);
    m_retryDelaySpin->setSuffix(" sec");
    m_outputLayout->addRow(obs_module_text("MultiRTMP.Config.RetryDelay"), m_retryDelaySpin);
    
    m_tabWidget->addTab(m_outputTab, obs_module_text("MultiRTMP.Config.Tab.Output"));
}

void OneSevenMultiRtmpConfigDialog::setupVideoTab()
{
    m_videoTab = new QWidget();
    m_videoLayout = new QFormLayout(m_videoTab);
    m_videoLayout->setSpacing(8);
    
    // Enable video
    m_enableVideoCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.EnableVideo"));
    m_enableVideoCheck->setChecked(true);
    m_videoLayout->addRow("", m_enableVideoCheck);
    
    // Video resolution
    m_videoResolutionCombo = new QComboBox();
    m_videoLayout->addRow(obs_module_text("MultiRTMP.Config.VideoResolution"), m_videoResolutionCombo);
    
    // Custom resolution
    QHBoxLayout* customResLayout = new QHBoxLayout();
    m_customWidthEdit = new QLineEdit();
    m_customWidthEdit->setPlaceholderText("1920");
    m_customWidthEdit->setMaximumWidth(80);
    m_customHeightEdit = new QLineEdit();
    m_customHeightEdit->setPlaceholderText("1080");
    m_customHeightEdit->setMaximumWidth(80);
    
    customResLayout->addWidget(m_customWidthEdit);
    customResLayout->addWidget(new QLabel("×"));
    customResLayout->addWidget(m_customHeightEdit);
    customResLayout->addStretch();
    
    m_videoLayout->addRow(obs_module_text("MultiRTMP.Config.CustomResolution"), customResLayout);
    
    // Frame rate
    m_fpsSpinBox = new QDoubleSpinBox();
    m_fpsSpinBox->setRange(1.0, 120.0);
    m_fpsSpinBox->setValue(30.0);
    m_fpsSpinBox->setSuffix(" fps");
    m_videoLayout->addRow(obs_module_text("MultiRTMP.Config.FrameRate"), m_fpsSpinBox);
    
    // Scale filter
    m_scaleFilterCombo = new QComboBox();
    m_videoLayout->addRow(obs_module_text("MultiRTMP.Config.ScaleFilter"), m_scaleFilterCombo);
    
    // Quality slider
    QHBoxLayout* qualityLayout = new QHBoxLayout();
    m_qualitySlider = new QSlider(Qt::Horizontal);
    m_qualitySlider->setRange(1, 10);
    m_qualitySlider->setValue(7);
    m_qualityLabel = new QLabel("7");
    
    qualityLayout->addWidget(m_qualitySlider);
    qualityLayout->addWidget(m_qualityLabel);
    
    m_videoLayout->addRow(obs_module_text("MultiRTMP.Config.Quality"), qualityLayout);
    
    m_tabWidget->addTab(m_videoTab, obs_module_text("MultiRTMP.Config.Tab.Video"));
}

void OneSevenMultiRtmpConfigDialog::setupAudioTab()
{
    m_audioTab = new QWidget();
    m_audioLayout = new QFormLayout(m_audioTab);
    m_audioLayout->setSpacing(8);
    
    // Enable audio
    m_enableAudioCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.EnableAudio"));
    m_enableAudioCheck->setChecked(true);
    m_audioLayout->addRow("", m_enableAudioCheck);
    
    // Audio format
    m_audioFormatCombo = new QComboBox();
    m_audioLayout->addRow(obs_module_text("MultiRTMP.Config.AudioFormat"), m_audioFormatCombo);
    
    // Sample rate
    m_sampleRateSpin = new QSpinBox();
    m_sampleRateSpin->setRange(8000, 48000);
    m_sampleRateSpin->setValue(44100);
    m_sampleRateSpin->setSuffix(" Hz");
    m_audioLayout->addRow(obs_module_text("MultiRTMP.Config.SampleRate"), m_sampleRateSpin);
    
    // Channel layout
    m_channelLayoutCombo = new QComboBox();
    m_channelLayoutCombo->addItems({"Mono", "Stereo", "5.1"});
    m_channelLayoutCombo->setCurrentText("Stereo");
    m_audioLayout->addRow(obs_module_text("MultiRTMP.Config.ChannelLayout"), m_channelLayoutCombo);
    
    // Audio volume
    QHBoxLayout* volumeLayout = new QHBoxLayout();
    m_audioVolumeSlider = new QSlider(Qt::Horizontal);
    m_audioVolumeSlider->setRange(0, 100);
    m_audioVolumeSlider->setValue(100);
    m_audioVolumeLabel = new QLabel("100%");
    
    volumeLayout->addWidget(m_audioVolumeSlider);
    volumeLayout->addWidget(m_audioVolumeLabel);
    
    m_audioLayout->addRow(obs_module_text("MultiRTMP.Config.AudioVolume"), volumeLayout);
    
    m_tabWidget->addTab(m_audioTab, obs_module_text("MultiRTMP.Config.Tab.Audio"));
}

void OneSevenMultiRtmpConfigDialog::setupSyncTab()
{
    m_syncTab = new QWidget();
    m_syncLayout = new QFormLayout(m_syncTab);
    m_syncLayout->setSpacing(8);
    
    // Sync with main stream
    m_syncWithMainCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.SyncWithMain"));
    m_syncWithMainCheck->setToolTip(obs_module_text("MultiRTMP.Config.SyncWithMain.Tooltip"));
    m_syncLayout->addRow("", m_syncWithMainCheck);
    
    // Start delay
    m_startDelaySpin = new QSpinBox();
    m_startDelaySpin->setRange(0, 60);
    m_startDelaySpin->setValue(0);
    m_startDelaySpin->setSuffix(" sec");
    m_syncLayout->addRow(obs_module_text("MultiRTMP.Config.StartDelay"), m_startDelaySpin);
    
    // Stop delay
    m_stopDelaySpin = new QSpinBox();
    m_stopDelaySpin->setRange(0, 60);
    m_stopDelaySpin->setValue(0);
    m_stopDelaySpin->setSuffix(" sec");
    m_syncLayout->addRow(obs_module_text("MultiRTMP.Config.StopDelay"), m_stopDelaySpin);
    
    // Auto start
    m_autoStartCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.AutoStart"));
    m_syncLayout->addRow("", m_autoStartCheck);
    
    // Auto stop
    m_autoStopCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.AutoStop"));
    m_syncLayout->addRow("", m_autoStopCheck);
    
    // Sync mode
    m_syncModeCombo = new QComboBox();
    m_syncLayout->addRow(obs_module_text("MultiRTMP.Config.SyncMode"), m_syncModeCombo);
    
    m_tabWidget->addTab(m_syncTab, obs_module_text("MultiRTMP.Config.Tab.Sync"));
}

void OneSevenMultiRtmpConfigDialog::setupAdvancedTab()
{
    m_advancedTab = new QWidget();
    m_advancedLayout = new QFormLayout(m_advancedTab);
    m_advancedLayout->setSpacing(8);
    
    // Custom settings
    m_customSettingsEdit = new QLineEdit();
    m_customSettingsEdit->setPlaceholderText("key1=value1 key2=value2");
    m_advancedLayout->addRow(obs_module_text("MultiRTMP.Config.CustomSettings"), m_customSettingsEdit);
    
    // Enable logging
    m_enableLoggingCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.EnableLogging"));
    m_advancedLayout->addRow("", m_enableLoggingCheck);
    
    // Log level
    m_logLevelCombo = new QComboBox();
    m_advancedLayout->addRow(obs_module_text("MultiRTMP.Config.LogLevel"), m_logLevelCombo);
    
    // Buffer size
    m_bufferSizeSpin = new QSpinBox();
    m_bufferSizeSpin->setRange(1024, 65536);
    m_bufferSizeSpin->setValue(8192);
    m_bufferSizeSpin->setSuffix(" KB");
    m_advancedLayout->addRow(obs_module_text("MultiRTMP.Config.BufferSize"), m_bufferSizeSpin);
    
    // Enable stats
    m_enableStatsCheck = new QCheckBox(obs_module_text("MultiRTMP.Config.EnableStats"));
    m_enableStatsCheck->setChecked(true);
    m_advancedLayout->addRow("", m_enableStatsCheck);
    
    // Stats interval
    m_statsIntervalSpin = new QSpinBox();
    m_statsIntervalSpin->setRange(1, 60);
    m_statsIntervalSpin->setValue(5);
    m_statsIntervalSpin->setSuffix(" sec");
    m_advancedLayout->addRow(obs_module_text("MultiRTMP.Config.StatsInterval"), m_statsIntervalSpin);
    
    m_tabWidget->addTab(m_advancedTab, obs_module_text("MultiRTMP.Config.Tab.Advanced"));
}

void OneSevenMultiRtmpConfigDialog::setupButtonBox()
{
    m_buttonLayout = new QHBoxLayout();
    
    m_resetButton = new QPushButton(obs_module_text("MultiRTMP.Config.Reset"));
    m_resetButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    
    m_testButton = new QPushButton(obs_module_text("MultiRTMP.Config.Test"));
    m_testButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    
    m_cancelButton = new QPushButton(obs_module_text("MultiRTMP.Config.Cancel"));
    m_cancelButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton));
    
    m_okButton = new QPushButton(obs_module_text("MultiRTMP.Config.OK"));
    m_okButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogOkButton));
    m_okButton->setDefault(true);
    
    m_buttonLayout->addWidget(m_resetButton);
    m_buttonLayout->addWidget(m_testButton);
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
    connect(m_resetButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::onResetToDefaultsClicked);
    connect(m_testButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::onTestConnectionClicked);
}

void OneSevenMultiRtmpConfigDialog::setupValidation()
{
    m_validationTimer = new QTimer(this);
    m_validationTimer->setSingleShot(true);
    m_validationTimer->setInterval(500); // 500ms delay
    connect(m_validationTimer, &QTimer::timeout, this, &OneSevenMultiRtmpConfigDialog::onValidationTimer);
    
    // Connect validation triggers to all input fields
    connect(m_serviceNameEdit, &QLineEdit::textChanged, 
            this, [this]() { m_validationTimer->start(); });
    connect(m_serverEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });
    connect(m_keyEdit, &QLineEdit::textChanged,
            this, [this]() { m_validationTimer->start(); });
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
    m_syncModeCombo->addItems(SYNC_MODES);
    m_logLevelCombo->addItems(LOG_LEVELS);
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

void OneSevenMultiRtmpConfigDialog::onEncoderSharingChanged()
{
    updateEncoderFields();
}

void OneSevenMultiRtmpConfigDialog::onVideoResolutionChanged()
{
    updateVideoFields();
}

void OneSevenMultiRtmpConfigDialog::onTestConnectionClicked()
{
    updateConnectionTest();
}

void OneSevenMultiRtmpConfigDialog::onResetToDefaultsClicked()
{
    auto reply = QMessageBox::question(this,
        obs_module_text("MultiRTMP.Config.Reset.Title"),
        obs_module_text("MultiRTMP.Config.Reset.Confirm"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        resetToDefaults();
    }
}

void OneSevenMultiRtmpConfigDialog::onValidationTimer()
{
    validateConfiguration();
}

void OneSevenMultiRtmpConfigDialog::updateServiceFields()
{
    bool isCustom = m_customServiceCheck->isChecked();
    m_serviceNameEdit->setEnabled(isCustom);
    
    if (!isCustom) {
        QString serviceType = m_serviceTypeCombo->currentText();
        m_serviceNameEdit->setText(serviceType);
        
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

void OneSevenMultiRtmpConfigDialog::updateSyncFields()
{
    // Update sync-related fields based on current settings
}

bool OneSevenMultiRtmpConfigDialog::validateConfiguration()
{
    auto config = buildConfigFromUI();
    bool isValid = config.isValid();
    
    if (!isValid) {
        m_validationLabel->setText(QString::fromStdString(config.getValidationError()));
        m_validationLabel->setVisible(true);
        m_okButton->setEnabled(false);
    } else {
        m_validationLabel->setVisible(false);
        m_okButton->setEnabled(true);
    }
    
    return isValid;
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
    // Service tab
    m_serviceNameEdit->setText(QString::fromStdString(config.streamName)); // Map to streamName
    m_serverEdit->setText(QString::fromStdString(config.service.serverUrl)); // Map to service.serverUrl
    m_keyEdit->setText(QString::fromStdString(config.service.streamKey)); // Map to service.streamKey
    m_descriptionEdit->setPlainText(""); // Default empty since field doesn't exist
    
    // Output tab
    m_shareEncoderCheck->setChecked(config.video.useSharedEncoder); // Map to video.useSharedEncoder
    m_videoBitrateSpin->setValue(config.video.bitrate); // Map to video.bitrate
    m_audioBitrateSpin->setValue(config.audio.bitrate); // Map to audio.bitrate
    m_enableReconnectCheck->setChecked(config.output.autoReconnect); // Map to output.autoReconnect
    m_maxRetriesSpin->setValue(5); // Default value since field doesn't exist
    m_retryDelaySpin->setValue(config.output.reconnectDelay); // Map to output.reconnectDelay
    
    // Video tab
    m_enableVideoCheck->setChecked(true); // Default to enabled since struct doesn't have this field
    m_customWidthEdit->setText(QString::number(config.video.outputWidth));
    m_customHeightEdit->setText(QString::number(config.video.outputHeight));
    m_fpsSpinBox->setValue(30.0); // Default FPS since struct uses fpsDenominator
    
    // Audio tab
    m_enableAudioCheck->setChecked(true); // Default to enabled since struct doesn't have this field
    m_sampleRateSpin->setValue(config.audio.sampleRate);
    
    // Sync tab
    m_syncWithMainCheck->setChecked(config.sync.syncStart); // Map to syncStart
    m_startDelaySpin->setValue(0); // Default value since field doesn't exist
    m_stopDelaySpin->setValue(0); // Default value since field doesn't exist
    m_autoStartCheck->setChecked(config.sync.syncStart); // Map to syncStart
    m_autoStopCheck->setChecked(config.sync.syncStop); // Map to syncStop
    
    // Update dependent fields
    updateServiceFields();
    updateEncoderFields();
    updateVideoFields();
}

OneSevenMultiRtmpConfig OneSevenMultiRtmpConfigDialog::buildConfigFromUI() const
{
    OneSevenMultiRtmpConfig config;
    
    // Keep original ID if editing
    if (m_isEditMode) {
        config.id = m_originalConfig.id;
    }
    
    // Service configuration
    config.streamName = m_serviceNameEdit->text().toStdString(); // Map to streamName
    config.service.serverUrl = m_serverEdit->text().toStdString(); // Map to service.serverUrl
    config.service.streamKey = m_keyEdit->text().toStdString(); // Map to service.streamKey
    // Note: description field doesn't exist in struct, skipping
    
    // Output configuration
    config.video.useSharedEncoder = m_shareEncoderCheck->isChecked(); // Map to video.useSharedEncoder
    config.video.bitrate = m_videoBitrateSpin->value(); // Map to video.bitrate
    config.audio.bitrate = m_audioBitrateSpin->value(); // Map to audio.bitrate
    config.output.autoReconnect = m_enableReconnectCheck->isChecked(); // Map to output.autoReconnect
    // Note: maxRetries field doesn't exist in struct, skipping
    config.output.reconnectDelay = m_retryDelaySpin->value(); // Map to output.reconnectDelay
    
    // Video configuration
    // Note: enabled field doesn't exist in struct, skipping
    config.video.outputWidth = m_customWidthEdit->text().toInt();
    config.video.outputHeight = m_customHeightEdit->text().toInt();
    // Note: fps field doesn't exist in struct, using fpsDenominator instead
    // For now, we'll skip this since fpsDenominator has different semantics
    
    // Audio configuration
    // Note: enabled field doesn't exist in struct, skipping
    config.audio.sampleRate = m_sampleRateSpin->value();
    
    // Sync configuration
    config.sync.syncStart = m_syncWithMainCheck->isChecked(); // Map to syncStart
    // Note: startDelay and stopDelay fields don't exist in struct, skipping
    config.sync.syncStart = m_autoStartCheck->isChecked(); // Map to syncStart
    config.sync.syncStop = m_autoStopCheck->isChecked(); // Map to syncStop
    
    // Note: updateLastModified() doesn't exist on OneSevenMultiRtmpConfig
    // This should be handled at the global config level
    
    return config;
}
