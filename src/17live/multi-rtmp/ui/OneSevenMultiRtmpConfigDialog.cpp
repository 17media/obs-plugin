#include "OneSevenMultiRtmpConfigDialog.hpp"

#include <obs-frontend-api.h>

#include <QApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QScrollArea>
#include <QStyle>
#include <QUuid>

#include "../../ui/OneSevenLivePropertiesWidget.hpp"
#include "../../utility/Common.hpp"
#include "moc_OneSevenMultiRtmpConfigDialog.cpp"

OneSevenMultiRtmpConfigDialog::OneSevenMultiRtmpConfigDialog(
    QWidget* parent, std::shared_ptr<OneSevenMultiRtmpConfig> config)
    : QDialog(parent),
      m_config(config),
      m_mainLayout(nullptr),
      m_tabWidget(nullptr),
      m_isEditMode(config != nullptr),
      m_advancedExpanded(false),
      m_baseHeight(0) {
    setWindowTitle(obs_module_text("MultiRTMP.Config.Title"));
    setModal(true);

    // Set dialog size constraints to match reference style
    setMinimumSize(350, 525);  // Increased minimum width to accommodate content
    setMaximumSize(600, 900);  // Increased maximum width for better content display
    resize(400, 450);          // Set initial size to ensure content fits properly

    setupUI();
    setupConnections();

    // Load configuration if provided
    if (m_config) {
        // duplicate config to m_origConfig
        m_originalConfig = std::make_shared<OneSevenMultiRtmpConfig>(*m_config);

        // Load configuration
        loadConfig();
    }

    // Record the base height after UI setup (when advanced settings are collapsed)
    // Use a small delay to ensure layout is fully calculated
    QTimer::singleShot(0, [this]() { m_baseHeight = height(); });
}

OneSevenMultiRtmpConfigDialog::~OneSevenMultiRtmpConfigDialog() {}

void OneSevenMultiRtmpConfigDialog::setupUI() {
    // Create main layout for the dialog
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create scroll area
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);        // Allow content resizing
    scrollArea->setFrameShape(QFrame::NoFrame);  // Remove border
    scrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded);  // Show vertical scrollbar when needed
    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);  // Disable horizontal scrollbar

    // Create container widget for scroll area content
    QWidget* container = new QWidget(this);
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

    QVBoxLayout* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(
        20, 16, 20, 16);  // Increased horizontal margins for better content spacing
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
    containerLayout->addStretch();  // Add stretch to push content to top

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

void OneSevenMultiRtmpConfigDialog::setupBasicInfoSection() {
    m_basicInfoWidget = new QWidget();
    m_basicInfoLayout = new QFormLayout(m_basicInfoWidget);

    // Set form layout properties to match reference style
    m_basicInfoLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_basicInfoLayout->setLabelAlignment(Qt::AlignLeft);
    m_basicInfoLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_basicInfoLayout->setSpacing(12);
    m_basicInfoLayout->setContentsMargins(0, 0, 0, 0);

    // RTMP channel selection (stored in config.streamName)
    QLabel* streamNameLabel = new QLabel();
    streamNameLabel->setText(
        QString("<span style='color:red;'>*</span><span style='color:white;'>%1</span>")
            .arg(obs_module_text("MultiRtmp.Config.StreamName")));
    m_streamNameCombo = new QComboBox();
    m_streamNameCombo->addItem("YouTube");
    m_streamNameCombo->addItem("Twitch");
    m_streamNameCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_basicInfoLayout->addRow(streamNameLabel, m_streamNameCombo);

    // Protocol dropdown - only RTMP, SRT/RIST, WHIP
    QLabel* protocolLabel = new QLabel();
    protocolLabel->setText(QString("<span style='color:white;'>%1</span>")
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

    m_serviceWidget = new OneSevenLivePropertiesWidget(m_basicInfoWidget);
    m_serviceWidget->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Preferred);  // Ensure service widget expands properly
    m_basicInfoLayout->addRow("", m_serviceWidget);

    m_syncStartCheckbox = new QCheckBox();
    m_syncStartCheckbox->setText(obs_module_text("MultiRtmp.Config.SyncStart"));
    m_syncStopCheckbox = new QCheckBox();
    m_syncStopCheckbox->setText(obs_module_text("MultiRtmp.Config.SyncStop"));
}

void OneSevenMultiRtmpConfigDialog::setupAdvancedSettingsButton() {
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
        "}");

    // Set arrow icon for collapsed state
    QIcon downIcon(":/resources/arrow-down.svg");
    m_advancedButton->setIcon(downIcon);
    m_advancedButton->setIconSize(QSize(12, 12));

    m_advancedButton->setLayoutDirection(Qt::RightToLeft);  // Icon on the right, centered layout
}

void OneSevenMultiRtmpConfigDialog::setupAdvancedSettingsWidget() {
    m_advancedWidget = new QWidget(this);
    m_advancedWidget->setVisible(false);  // Initially collapsed

    QVBoxLayout* advancedLayout = new QVBoxLayout(m_advancedWidget);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    advancedLayout->setSpacing(0);

    // Create tab widget for advanced settings
    m_tabWidget = new QTabWidget(m_advancedWidget);
    m_tabWidget->setSizePolicy(QSizePolicy::Expanding,
                               QSizePolicy::Expanding);  // Ensure TabWidget expands properly

    setupOutputTab();
    setupVideoTab();
    setupAudioTab();

    advancedLayout->addWidget(m_tabWidget);
}

// Service tab removed - integrated into basic info section

void OneSevenMultiRtmpConfigDialog::setupOutputTab() {
    m_outputTab = new QWidget(m_tabWidget);
    m_outputLayout = new QFormLayout(m_outputTab);
    m_outputLayout->setSpacing(12);
    m_outputLayout->setContentsMargins(8, 12, 8, 12);
    m_outputLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_outputLayout->setLabelAlignment(Qt::AlignLeft);
    m_outputLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_outputWidget = new OneSevenLivePropertiesWidget(m_outputTab);

    m_tabWidget->addTab(m_outputWidget, obs_module_text("MultiRTMP.Config.Tab.Output"));
}

void OneSevenMultiRtmpConfigDialog::setupVideoTab() {
    m_videoTab = new QWidget(m_tabWidget);
    m_videoLayout = new QFormLayout(m_videoTab);
    m_videoLayout->setSpacing(12);
    m_videoLayout->setContentsMargins(8, 12, 8, 12);
    m_videoLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_videoLayout->setLabelAlignment(Qt::AlignLeft);
    m_videoLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // Enable video
    QHBoxLayout* useOBSVideoCheckLayout = new QHBoxLayout();
    QLabel* useOBSVideoCheckLabel = new QLabel(obs_module_text("MultiRtmp.Config.Video.UseOBS"));
    m_useOBSVideoCheck = new QCheckBox();
    m_useOBSVideoCheck->setChecked(true);
    useOBSVideoCheckLayout->addWidget(useOBSVideoCheckLabel);
    useOBSVideoCheckLayout->addStretch();
    useOBSVideoCheckLayout->addWidget(m_useOBSVideoCheck);
    m_videoLayout->addRow("", useOBSVideoCheckLayout);

    m_outputSceneCombo = new QComboBox(m_videoTab);
    m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.OutputScene"),
                          m_outputSceneCombo);

    m_videoEncoderCombo = new QComboBox(m_videoTab);
    m_videoLayout->addRow(obs_module_text("MultiRTMP.Config.Encoder.Video"), m_videoEncoderCombo);

    m_videoWidget = new OneSevenLivePropertiesWidget(m_videoTab);
    m_videoLayout->addRow("", m_videoWidget);

    // TODO: if suitable for rtmp?
    // m_videoResolutionCombo = new QComboBox(m_videoTab);
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.Resolution"),
    // m_videoResolutionCombo);

    // m_fpsDenominatorCombo = new QComboBox(m_videoTab);
    // m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.FPSDenominator"),
    // m_fpsDenominatorCombo);

    m_tabWidget->addTab(m_videoTab, obs_module_text("MultiRTMP.Config.Tab.Video"));
}

void OneSevenMultiRtmpConfigDialog::setupAudioTab() {
    m_audioTab = new QWidget(m_tabWidget);
    m_audioLayout = new QFormLayout(m_audioTab);
    m_audioLayout->setSpacing(12);
    m_audioLayout->setContentsMargins(8, 12, 8, 12);
    m_audioLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_audioLayout->setLabelAlignment(Qt::AlignLeft);
    m_audioLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // Enable audio
    QHBoxLayout* useOBSAudioCheckLayout = new QHBoxLayout();
    QLabel* useOBSAudioCheckLabel = new QLabel(obs_module_text("MultiRtmp.Config.Audio.UseOBS"));
    m_useOBSAudioCheck = new QCheckBox();
    m_useOBSAudioCheck->setChecked(true);
    useOBSAudioCheckLayout->addWidget(useOBSAudioCheckLabel);
    useOBSAudioCheckLayout->addStretch();
    useOBSAudioCheckLayout->addWidget(m_useOBSAudioCheck);
    m_audioLayout->addRow(useOBSAudioCheckLayout);

    m_audioEncoderCombo = new QComboBox(m_audioTab);
    m_audioLayout->addRow(obs_module_text("MultiRTMP.Config.Encoder.Audio"), m_audioEncoderCombo);

    m_audioWidget = new OneSevenLivePropertiesWidget(m_audioTab);
    m_audioLayout->addRow("", m_audioWidget);

    m_tabWidget->addTab(m_audioTab, obs_module_text("MultiRTMP.Config.Tab.Audio"));
}

void OneSevenMultiRtmpConfigDialog::setupButtonBox() {
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
        "}");

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
        "}");
    m_okButton->setDefault(true);

    // Center the buttons
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_okButton);
    m_buttonLayout->addStretch();
}

void OneSevenMultiRtmpConfigDialog::setupConnections() {
    // Advanced settings toggle
    connect(m_advancedButton, &QPushButton::clicked, this,
            &OneSevenMultiRtmpConfigDialog::onAdvancedSettingsToggled);

    // Buttons
    connect(m_okButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &OneSevenMultiRtmpConfigDialog::reject);
}

void OneSevenMultiRtmpConfigDialog::setEditMode(bool isEdit) {
    m_isEditMode = isEdit;
}

void OneSevenMultiRtmpConfigDialog::accept() {
    // Note: SaveConfig() is called by the parent dialog (OneSevenMultiRtmpDock)
    // to avoid double calls and potential memory issues
    obs_log(LOG_INFO,
            "[MultiRTMP-ConfigDialog] Dialog accepted, SaveConfig will be called by parent");
    QDialog::accept();
}

void OneSevenMultiRtmpConfigDialog::reject() {
    QDialog::reject();
}

// Service-related slot functions removed - functionality integrated into basic info section

void OneSevenMultiRtmpConfigDialog::onAdvancedSettingsToggled() {
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

    // Adjust dialog size properly
    if (m_advancedExpanded) {
        // When expanding, calculate the needed height for advanced settings
        // Use a small delay to ensure the widget visibility change is processed
        QTimer::singleShot(0, [this]() {
            int currentWidth = width();
            int neededHeight = sizeHint().height();

            // Ensure we don't shrink below the base height
            if (neededHeight < m_baseHeight) {
                neededHeight = m_baseHeight + 200;  // Add some space for advanced settings
            }

            resize(currentWidth, neededHeight);
        });
    } else {
        // When collapsing, return to base height
        if (m_baseHeight > 0) {
            resize(width(), m_baseHeight);
        } else {
            // Fallback if base height wasn't recorded properly
            resize(width(), 400);
        }
    }
}

void OneSevenMultiRtmpConfigDialog::loadConfig() {
    if (!m_config) {
        return;
    }

    // Load basic information
    // Map existing streamName to combo selection if matches supported channels
    if (!m_config->streamName.empty()) {
        const QString name = QString::fromStdString(m_config->streamName);
        int idx = m_streamNameCombo->findText(name, Qt::MatchFixedString);
        if (idx >= 0) {
            m_streamNameCombo->setCurrentIndex(idx);
        }
    }

    // Load protocol and URL
    auto protocol_info = findProtocol(m_config->protocol);
    if (!protocol_info) {
        obs_log(LOG_ERROR, "[loadConfig] Failed to find protocol info for: %s",
                m_config->protocol.c_str());
        return;
    }

    // Set protocol in combo box
    for (int i = 0; i < m_protocolCombo->count(); ++i) {
        if (m_protocolCombo->itemData(i).toString().toStdString() == m_config->protocol) {
            m_protocolCombo->setCurrentIndex(i);
            break;
        }
    }

    // Load service settings
    {
        obs_data_t* service_settings = nullptr;
        if (!m_config->serviceSettings.empty()) {
            service_settings = obs_data_create_from_json(m_config->serviceSettings.dump().c_str());
        }

        obs_service_t* service =
            obs_service_create(protocol_info->serviceId, "temp_service", service_settings, nullptr);
        if (!service) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to create OBS service with ID: %s",
                    protocol_info->serviceId);
            obs_data_release(service_settings);
            return;
        }

        obs_data_t* settings = obs_service_get_settings(service);
        obs_properties_t* props = obs_service_properties(service);

        if (!settings || !props) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to get service settings or properties");
            obs_service_release(service);
            obs_data_release(service_settings);
            return;
        }

        if (m_serviceWidget) {
            m_serviceWidget->UpdateProperties(settings, props);
        }

        obs_properties_destroy(props);
        obs_data_release(settings);
        obs_service_release(service);
        obs_data_release(service_settings);
    }

    // Load output settings
    {
        obs_data_t* output_settings = nullptr;
        if (!m_config->outputSettings.empty()) {
            output_settings = obs_data_create_from_json(m_config->outputSettings.dump().c_str());
        }

        obs_output_t* output =
            obs_output_create(protocol_info->outputId, "temp_output", output_settings, nullptr);
        if (!output) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to create OBS output with ID: %s",
                    protocol_info->outputId);
            obs_data_release(output_settings);
            return;
        }

        obs_data_t* settings = obs_output_get_settings(output);
        obs_properties_t* props = obs_output_properties(output);

        if (!settings || !props) {
            obs_log(LOG_ERROR,
                    "[loadConfig] Failed to get output settings or properties (settings: %p, "
                    "props: %p)",
                    (void*) settings, (void*) props);
            obs_output_release(output);
            obs_data_release(output_settings);
            return;
        }

        if (!m_outputWidget) {
            obs_log(LOG_ERROR, "[loadConfig] m_outputWidget is null, cannot update properties");
            obs_properties_destroy(props);
            obs_data_release(settings);
            obs_output_release(output);
            obs_data_release(output_settings);
            return;
        }

        try {
            m_outputWidget->UpdateProperties(settings, props);
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "[loadConfig] Exception in UpdateProperties: %s", e.what());
        } catch (...) {
            obs_log(LOG_ERROR, "[loadConfig] Unknown exception in UpdateProperties");
        }

        obs_properties_destroy(props);
        obs_data_release(settings);
        obs_output_release(output);
        obs_data_release(output_settings);
    }

    // Load video encoder settings
    if (m_config->videoConfig.has_value() && !m_config->videoConfig->encoderSettings.empty()) {
        obs_data_t* encoder_settings =
            obs_data_create_from_json(m_config->videoConfig->encoderSettings.dump().c_str());

        obs_encoder_t* encoder =
            obs_video_encoder_create(m_config->videoConfig->encoderId.c_str(), "temp_video_encoder",
                                     encoder_settings, nullptr);
        if (!encoder) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to create video encoder with ID: %s",
                    m_config->videoConfig->encoderId.c_str());
            obs_data_release(encoder_settings);
            return;
        }

        obs_data_t* settings = obs_encoder_get_settings(encoder);
        obs_properties_t* props = obs_encoder_properties(encoder);

        if (!settings || !props) {
            obs_log(LOG_ERROR,
                    "[loadConfig] Failed to get video encoder settings or properties (settings: "
                    "%p, props: %p)",
                    (void*) settings, (void*) props);
            obs_encoder_release(encoder);
            obs_data_release(encoder_settings);
            return;
        }

        if (!m_videoWidget) {
            obs_log(LOG_ERROR,
                    "[loadConfig] m_videoWidget is null, cannot update video properties");
            obs_properties_destroy(props);
            obs_data_release(settings);
            obs_encoder_release(encoder);
            obs_data_release(encoder_settings);
            return;
        }

        try {
            m_videoWidget->UpdateProperties(settings, props);
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "[loadConfig] Exception in video UpdateProperties: %s", e.what());
        } catch (...) {
            obs_log(LOG_ERROR, "[loadConfig] Unknown exception in video UpdateProperties");
        }

        obs_properties_destroy(props);
        obs_data_release(settings);
        obs_encoder_release(encoder);
        obs_data_release(encoder_settings);
    }

    // Load audio encoder settings
    if (m_config->audioConfig.has_value() && !m_config->audioConfig->encoderSettings.empty()) {
        obs_data_t* encoder_settings =
            obs_data_create_from_json(m_config->audioConfig->encoderSettings.dump().c_str());

        obs_encoder_t* encoder =
            obs_audio_encoder_create(m_config->audioConfig->encoderId.c_str(), "temp_audio_encoder",
                                     encoder_settings, 0, nullptr);
        if (!encoder) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to create audio encoder with ID: %s",
                    m_config->audioConfig->encoderId.c_str());
            obs_data_release(encoder_settings);
            return;
        }

        obs_data_t* settings = obs_encoder_get_settings(encoder);
        obs_properties_t* props = obs_encoder_properties(encoder);

        if (!settings || !props) {
            obs_log(LOG_ERROR,
                    "[loadConfig] Failed to get audio encoder settings or properties (settings: "
                    "%p, props: %p)",
                    (void*) settings, (void*) props);
            obs_encoder_release(encoder);
            obs_data_release(encoder_settings);
            return;
        }

        if (!m_audioWidget) {
            obs_log(LOG_ERROR,
                    "[loadConfig] m_audioWidget is null, cannot update audio properties");
            obs_properties_destroy(props);
            obs_data_release(settings);
            obs_encoder_release(encoder);
            obs_data_release(encoder_settings);
            return;
        }

        try {
            m_audioWidget->UpdateProperties(settings, props);
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "[loadConfig] Exception in audio UpdateProperties: %s", e.what());
        } catch (...) {
            obs_log(LOG_ERROR, "[loadConfig] Unknown exception in audio UpdateProperties");
        }

        obs_properties_destroy(props);
        obs_data_release(settings);
        obs_encoder_release(encoder);
        obs_data_release(encoder_settings);
    }
}

OneSevenMultiRtmpConfig OneSevenMultiRtmpConfigDialog::SaveConfig() const {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] SaveConfig called - starting configuration save");

    try {
        OneSevenMultiRtmpConfig config;

        // Set ID based on edit mode - only preserve existing ID for edit mode
        if (m_isEditMode && m_config) {
            config.id = m_config->id;
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Edit mode: using existing ID: %s",
                    config.id.c_str());
        } else {
            // For new configurations, leave ID empty - it will be generated by the manager
            config.id = "";
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] New mode: ID will be generated by manager");
        }

        // Basic configuration with null checks
        if (!m_streamNameCombo) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_streamNameCombo is null");
            throw std::runtime_error("Stream name combo widget is null");
        }
        config.streamName = m_streamNameCombo->currentText().toStdString();
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] RTMP channel (stream name): '%s'", config.streamName.c_str());

        if (!m_protocolCombo) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_protocolCombo is null");
            throw std::runtime_error("Protocol combo widget is null");
        }
        config.protocol = m_protocolCombo->currentData().toString().toStdString();
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Protocol: '%s'", config.protocol.c_str());

        if (!m_syncStartCheckbox || !m_syncStopCheckbox) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] Sync checkboxes are null");
            throw std::runtime_error("Sync checkbox widgets are null");
        }
        config.syncStart = m_syncStartCheckbox->isChecked();
        config.syncStop = m_syncStopCheckbox->isChecked();
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Sync settings - Start: %s, Stop: %s",
                config.syncStart ? "true" : "false", config.syncStop ? "true" : "false");

        // Service configuration
        if (!m_serviceWidget) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_serviceWidget is null");
            throw std::runtime_error("Service widget is null");
        }
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving service settings...");
        config.serviceSettings = m_serviceWidget->SaveData();
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Service settings saved successfully");

        // Output configuration
        if (!m_outputWidget) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_outputWidget is null");
            throw std::runtime_error("Output widget is null");
        }
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving output settings...");
        config.outputSettings = m_outputWidget->SaveData();
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Output settings saved successfully");

        // Video configuration
        if (!m_useOBSVideoCheck) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_useOBSVideoCheck is null");
            throw std::runtime_error("Video checkbox widget is null");
        }

        if (!m_useOBSVideoCheck->isChecked()) {
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving custom video configuration...");
            OneSevenMultiRtmpVideoConfig vcfg;

            if (m_videoEncoderCombo) {
                vcfg.encoderId = m_videoEncoderCombo->currentData().toString().toStdString();
                obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video encoder ID: '%s'",
                        vcfg.encoderId.c_str());
            }
            if (m_videoResolutionCombo) {
                vcfg.resolution = m_videoResolutionCombo->currentText().toStdString();
                obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video resolution: '%s'",
                        vcfg.resolution.c_str());
            }
            if (m_fpsDenominatorCombo) {
                vcfg.fpsDenominator = m_fpsDenominatorCombo->currentText().toInt();
                obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video FPS denominator: %d",
                        vcfg.fpsDenominator);
            }
            if (m_outputSceneCombo) {
                const QVariant data = m_outputSceneCombo->currentData();
                vcfg.outputScene = data.isValid() ? data.toString().toStdString()
                                                  : m_outputSceneCombo->currentText().toStdString();
                obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Output scene: '%s'",
                        vcfg.outputScene.c_str());
            }

            if (!m_videoWidget) {
                obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_videoWidget is null");
                throw std::runtime_error("Video widget is null");
            }
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving video encoder settings...");
            vcfg.encoderSettings = m_videoWidget->SaveData();
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video encoder settings saved successfully");

            config.videoConfig = vcfg;
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Using OBS video settings");
            config.videoConfig.reset();
        }

        // Audio configuration
        if (!m_useOBSAudioCheck) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_useOBSAudioCheck is null");
            throw std::runtime_error("Audio checkbox widget is null");
        }

        if (!m_useOBSAudioCheck->isChecked()) {
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving custom audio configuration...");
            OneSevenMultiRtmpAudioConfig acfg;

            if (m_audioEncoderCombo) {
                acfg.encoderId = m_audioEncoderCombo->currentData().toString().toStdString();
                obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Audio encoder ID: '%s'",
                        acfg.encoderId.c_str());
            }

            if (!m_audioWidget) {
                obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_audioWidget is null");
                throw std::runtime_error("Audio widget is null");
            }
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving audio encoder settings...");
            acfg.encoderSettings = m_audioWidget->SaveData();
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Audio encoder settings saved successfully");

            config.audioConfig = acfg;
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Using OBS audio settings");
            config.audioConfig.reset();
        }

        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] SaveConfig completed successfully");
        return config;

    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] SaveConfig failed with exception: %s",
                e.what());
        throw;
    } catch (...) {
        obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] SaveConfig failed with unknown exception");
        throw;
    }
}

void OneSevenMultiRtmpConfigDialog::loadScenes() {
    if (!m_outputSceneCombo)
        return;

    m_outputSceneCombo->clear();
    m_outputSceneCombo->addItem(obs_module_text("MultiRtmp.Config.Video.UseOBS"), "");

    using EnumParam = std::vector<std::string>;
    EnumParam scenes;

    obs_enum_scenes(
        [](void* p, obs_source_t* src) {
            auto* list = static_cast<EnumParam*>(p);
            const char* name = obs_source_get_name(src);
            if (name && *name)
                list->emplace_back(name);
            return true;
        },
        &scenes);

    for (const auto& name : scenes) {
        m_outputSceneCombo->addItem(name.c_str(), name.c_str());
    }
}

std::vector<std::string> OneSevenMultiRtmpConfigDialog::parseAndLoadEncoders(
    const std::string& supportedEncoders, bool isVideoEncoder) {
    std::vector<std::string> encoderIds;

    if (!supportedEncoders.empty()) {
        // Split the semicolon-separated string
        std::string encoders = supportedEncoders;
        size_t pos = 0;
        std::string token;

        while ((pos = encoders.find(';')) != std::string::npos) {
            token = encoders.substr(0, pos);
            if (!token.empty()) {
                // Query OBS API for encoders supporting this codec
                size_t i = 0;
                for (;;) {
                    const char* encid;
                    if (!obs_enum_encoder_types(i++, &encid))
                        break;
                    auto caps = obs_get_encoder_caps(encid);
                    if (caps & OBS_ENCODER_CAP_DEPRECATED)
                        continue;

                    // Check if this is the correct encoder type (video or audio)
                    auto enc_type = obs_get_encoder_type(encid);
                    bool isCorrectType = isVideoEncoder ? (enc_type == OBS_ENCODER_VIDEO)
                                                        : (enc_type == OBS_ENCODER_AUDIO);
                    if (!isCorrectType)
                        continue;

                    auto enc_codec = obs_get_encoder_codec(encid);
                    if (strcmp(enc_codec, token.c_str()) == 0) {
                        encoderIds.emplace_back(encid);
                    }
                }
            }
            encoders.erase(0, pos + 1);
        }

        // Handle the last token (after the last semicolon or if no semicolon exists)
        if (!encoders.empty()) {
            size_t i = 0;
            for (;;) {
                const char* encid;
                if (!obs_enum_encoder_types(i++, &encid))
                    break;
                auto caps = obs_get_encoder_caps(encid);
                if (caps & OBS_ENCODER_CAP_DEPRECATED)
                    continue;

                // Check if this is the correct encoder type (video or audio)
                auto enc_type = obs_get_encoder_type(encid);
                bool isCorrectType = isVideoEncoder ? (enc_type == OBS_ENCODER_VIDEO)
                                                    : (enc_type == OBS_ENCODER_AUDIO);
                if (!isCorrectType)
                    continue;

                auto enc_codec = obs_get_encoder_codec(encid);
                if (strcmp(enc_codec, encoders.c_str()) == 0) {
                    encoderIds.emplace_back(encid);
                }
            }
        }
    }

    return encoderIds;
}

void OneSevenMultiRtmpConfigDialog::loadEncoders() {
    auto ui_text = [](const std::string& id) {
        const char* dn = obs_encoder_get_display_name(id.c_str());
        if (!dn)
            dn = id.c_str();
        return std::string(dn) + " [" + id + "]";
    };

    // Query current OBS outputs to provide "SameAsOBS" placeholders
    const char* streamingVideoId = nullptr;
    const char* streamingAudioId = nullptr;

    if (obs_output_t* streaming = obs_frontend_get_streaming_output()) {
        if (obs_encoder_t* ve = obs_output_get_video_encoder(streaming))
            streamingVideoId = obs_encoder_get_id(ve);
        if (obs_encoder_t* ae = obs_output_get_audio_encoder(streaming, 0))
            streamingAudioId = obs_encoder_get_id(ae);
        obs_output_release(streaming);
    }

    // Video encoders
    if (m_videoEncoderCombo) {
        QVariant old = m_videoEncoderCombo->currentData();
        m_videoEncoderCombo->clear();
        m_videoEncoderCombo->addItem(obs_module_text("MultiRtmp.Config.Video.UseOBS"),
                                     streamingVideoId ? streamingVideoId : "");

        // Parse supported video encoders using the generic function
        std::vector<std::string> videoIds = parseAndLoadEncoders(m_supportedVideoEncoders, true);

        // Add the found video encoders to the combo box
        for (const std::string& id : videoIds) {
            m_videoEncoderCombo->addItem(ui_text(id).c_str(), QString::fromStdString(id));
        }
        int idx = m_videoEncoderCombo->findData(old);
        if (idx >= 0)
            m_videoEncoderCombo->setCurrentIndex(idx);
    }

    // Audio encoders
    if (m_audioEncoderCombo) {
        QVariant old = m_audioEncoderCombo->currentData();
        m_audioEncoderCombo->clear();
        m_audioEncoderCombo->addItem(obs_module_text("MultiRtmp.Config.Video.UseOBS"),
                                     streamingAudioId ? streamingAudioId : "");

        // Parse supported audio encoders using the generic function
        std::vector<std::string> audioIds = parseAndLoadEncoders(m_supportedAudioEncoders, false);

        // Add the found audio encoders to the combo box
        for (const std::string& id : audioIds) {
            m_audioEncoderCombo->addItem(ui_text(id).c_str(), QString::fromStdString(id));
        }
        int idx = m_audioEncoderCombo->findData(old);
        if (idx >= 0)
            m_audioEncoderCombo->setCurrentIndex(idx);
    }
}
