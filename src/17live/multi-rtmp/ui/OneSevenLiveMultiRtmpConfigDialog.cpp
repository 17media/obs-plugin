#include "OneSevenLiveMultiRtmpConfigDialog.hpp"

// OBS headers are included in the source to avoid transitive system headers in the dialog header
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QScrollArea>
#include <QStyle>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <set>

#include "OneSevenLiveConfigManager.hpp"
#include "OneSevenLiveCoreManager.hpp"
#include "OneSevenLiveHttpServer.hpp"
#include "moc_OneSevenLiveMultiRtmpConfigDialog.cpp"
#include "multi-rtmp/OneSevenLiveMultiRtmpManager.hpp"
#include "twitch/OneSevenLiveTwitchAuth.hpp"
#include "ui/OneSevenLiveAuthDialog.hpp"
#include "ui/OneSevenLivePropertiesWidget.hpp"
#include "utility/Common.hpp"
#include "youtube/OneSevenLiveYouTubeAuth.hpp"

namespace {
constexpr bool IsYouTubeEnabled()
{
#if ENABLE_YOUTUBE
    return true;
#else
    return false;
#endif
}
}  // namespace

OneSevenLiveMultiRtmpConfigDialog::OneSevenLiveMultiRtmpConfigDialog(
    QWidget* parent, std::shared_ptr<OneSevenLiveMultiRtmpConfig> config, bool isEditMode)
    : QDialog(parent),
      m_config(config),
      m_mainLayout(nullptr),
      m_tabWidget(nullptr),
      m_isEditMode(isEditMode),
      m_advancedExpanded(false),
      m_baseHeight(0),
      m_isAuthorizing(false) {
    setWindowTitle(QString::fromUtf8(obs_module_text("MultiRTMP.Config.Title")));
    setModal(true);

    // Set dialog size constraints to match reference style
    setMinimumSize(350, 525);  // Increased minimum width to accommodate content
    setMaximumSize(600, 900);  // Increased maximum width for better content display
    resize(400, 450);          // Set initial size to ensure content fits properly

    setupUI();

    // Use CoreManager-owned authorization handlers to keep unified state
    OneSevenLiveCoreManager& coreManager = OneSevenLiveCoreManager::getInstance();
    m_twitchAuth = coreManager.getTwitchAuth();
    m_youtubeAuth = coreManager.getYouTubeAuth();

    // Load configuration if provided
    if (m_config) {
        // duplicate config to m_origConfig
        m_originalConfig = std::make_shared<OneSevenLiveMultiRtmpConfig>(*m_config);

        // Load configuration
        loadConfig();
    }

    // Record the base height after UI setup (when advanced settings are collapsed)
    // Use a small delay to ensure layout is fully calculated
    QTimer::singleShot(0, [this]() { m_baseHeight = height(); });

    setupConnections();
}

OneSevenLiveMultiRtmpConfigDialog::~OneSevenLiveMultiRtmpConfigDialog() {
    // Disconnect all QComboBox signals to prevent crashes during destruction
    if (m_streamNameCombo) {
        disconnect(m_streamNameCombo, nullptr, this, nullptr);
    }
    if (m_protocolCombo) {
        disconnect(m_protocolCombo, nullptr, this, nullptr);
    }
    if (m_outputSceneCombo) {
        disconnect(m_outputSceneCombo, nullptr, this, nullptr);
    }
    if (m_videoEncoderCombo) {
        disconnect(m_videoEncoderCombo, nullptr, this, nullptr);
    }
    if (m_audioEncoderCombo) {
        disconnect(m_audioEncoderCombo, nullptr, this, nullptr);
    }

    if (m_tmpServiceProps) {
        obs_service_t* svc = static_cast<obs_service_t*>(m_tmpServiceProps);
        obs_service_release(svc);
        m_tmpServiceProps = nullptr;
    }
}

void OneSevenLiveMultiRtmpConfigDialog::setupUI() {
    // Create main layout for the dialog
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create scroll area
    m_scrollArea = new QScrollArea(this);
    QScrollArea* scrollArea = m_scrollArea;
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollArea->setWidgetResizable(true);        // Allow content resizing
    scrollArea->setFrameShape(QFrame::NoFrame);  // Remove border
    scrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded);  // Show vertical scrollbar when needed
    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);  // Disable horizontal scrollbar

    // Create container widget for scroll area content
    m_container = new QWidget(this);
    QWidget* container = m_container;
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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

void OneSevenLiveMultiRtmpConfigDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    int avail = width();
    if (m_scrollArea && m_scrollArea->viewport()) {
        avail = m_scrollArea->viewport()->width();
    }
    if (m_advancedWidget)
        m_advancedWidget->setMaximumWidth(avail);
    if (m_serviceWidget)
        m_serviceWidget->setMaximumWidth(avail);
    if (m_tabWidget)
        m_tabWidget->setMaximumWidth(avail);
    if (m_outputTab)
        m_outputTab->setMaximumWidth(avail);
    if (m_videoTab)
        m_videoTab->setMaximumWidth(avail);
    if (m_videoWidget)
        m_videoWidget->setMaximumWidth(avail);
    if (m_audioTab)
        m_audioTab->setMaximumWidth(avail);
    if (m_audioWidget)
        m_audioWidget->setMaximumWidth(avail);
}

void OneSevenLiveMultiRtmpConfigDialog::setupBasicInfoSection() {
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
    bool hasYouTube = false;
    bool hasTwitch = false;
    if (auto mgr = OneSevenLiveMultiRtmpManager::getInstance()) {
        auto configs = mgr->getAllStreamConfigs();
        obs_log(LOG_INFO, "MultiRtmp: %d stream configs loaded", configs.size());
        for (const auto& cfg : configs) {
            // log stream name
            obs_log(LOG_INFO, "Stream name: %s", cfg.streamName.c_str());
            if (cfg.streamName == "YouTube")
                hasYouTube = true;
            else if (cfg.streamName == "Twitch")
                hasTwitch = true;
        }
    }
    obs_log(LOG_INFO, "isEditMode: %s", m_isEditMode ? "true" : "false");
    obs_log(LOG_INFO, "hasYouTube: %s", hasYouTube ? "true" : "false");
    obs_log(LOG_INFO, "hasTwitch: %s", hasTwitch ? "true" : "false");
    if (m_isEditMode && m_config) {
        m_streamNameCombo->addItem(QString::fromStdString(m_config->streamName));
        m_streamNameCombo->setEnabled(false);
    } else {
        if (IsYouTubeEnabled() && !hasYouTube)
            m_streamNameCombo->addItem("YouTube");
        if (!hasTwitch)
            m_streamNameCombo->addItem("Twitch");
    }
    m_streamNameCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_basicInfoLayout->addRow(streamNameLabel, m_streamNameCombo);

    // Authorize login button (YouTube/Twitch)
    m_authorizeButton = new QPushButton(obs_module_text("MultiRtmp.Config.Authorize"));
    m_basicInfoLayout->addRow(m_authorizeButton);

    // Initialize authorize button state based on current selection and token validity
    // The state will be updated again after config load and when selection changes
    updateAuthorizeButtonState();

    connect(m_authorizeButton, &QPushButton::clicked, this, [this]() {
        const QString channel = m_streamNameCombo ? m_streamNameCombo->currentText() : QString();
        bool isAuthorized = false;
        if (channel == OneSevenLiveYouTubeAuth::PLATFORM) {
            isAuthorized = (m_youtubeAuth && m_youtubeAuth->hasValidToken());
            if (isAuthorized && m_youtubeAuth) {
                m_youtubeAuth->clearToken();
                if (auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager()) {
                    if (!cm->clearYouTubeAccessToken()) {
                        const auto err = cm->getLastError();
                        obs_log(LOG_WARNING, "Failed to clear YouTube access token: %s %s",
                                err.code.c_str(), err.message.c_str());
                    }
                    if (!cm->clearYouTubeRefreshToken()) {
                        const auto err = cm->getLastError();
                        obs_log(LOG_WARNING, "Failed to clear YouTube refresh token: %s %s",
                                err.code.c_str(), err.message.c_str());
                    }
                }
            } else if (!isAuthorized) {
                onAuthorizeClicked();
            }
        } else if (channel == OneSevenLiveTwitchAuth::PLATFORM) {
            isAuthorized = (m_twitchAuth && m_twitchAuth->hasValidToken());
            if (isAuthorized && m_twitchAuth) {
                m_twitchAuth->clearTokens();
                if (auto* cm = OneSevenLiveCoreManager::getInstance().getConfigManager()) {
                    if (!cm->clearTwitchTokens()) {
                        const auto err = cm->getLastError();
                        obs_log(LOG_WARNING, "Failed to clear Twitch tokens: %s %s",
                                err.code.c_str(), err.message.c_str());
                    }
                    if (!cm->clearTwitchUserInfo()) {
                        const auto err = cm->getLastError();
                        obs_log(LOG_WARNING, "Failed to clear Twitch user info: %s %s",
                                err.code.c_str(), err.message.c_str());
                    }
                }
            } else if (!isAuthorized) {
                onAuthorizeClicked();
            }
        }
        updateAuthorizeButtonState();
    });

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
}

void OneSevenLiveMultiRtmpConfigDialog::setupAdvancedSettingsButton() {
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

void OneSevenLiveMultiRtmpConfigDialog::setupAdvancedSettingsWidget() {
    m_advancedWidget = new QWidget(this);
    m_advancedWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_advancedWidget->setMinimumWidth(0);
    m_advancedWidget->setVisible(false);  // Initially collapsed

    QVBoxLayout* advancedLayout = new QVBoxLayout(m_advancedWidget);
    advancedLayout->setContentsMargins(0, 10, 0, 0);
    advancedLayout->setSpacing(10);

    m_serviceWidget = new OneSevenLivePropertiesWidget(m_advancedWidget);
    m_serviceWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_serviceWidget->setMinimumWidth(0);

    advancedLayout->addWidget(m_serviceWidget);

    // Set initial visibility based on current platform authorization state
    if (m_streamNameCombo) {
        const QString channelSel = m_streamNameCombo->currentText();
        bool isAuthorized = false;
        if (channelSel == "YouTube") {
            isAuthorized = (m_youtubeAuth && m_youtubeAuth->hasValidToken());
        } else if (channelSel == "Twitch") {
            isAuthorized = (m_twitchAuth && m_twitchAuth->hasValidToken());
        }
        m_serviceWidget->setVisible(!isAuthorized);
    }

    // Create tab widget for advanced settings
    m_tabWidget = new QTabWidget(m_advancedWidget);
    m_tabWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_tabWidget->setMinimumWidth(0);

    setupOutputTab();
    setupVideoTab();
    setupAudioTab();

    advancedLayout->addWidget(m_tabWidget);
}

// Service tab removed - integrated into basic info section

void OneSevenLiveMultiRtmpConfigDialog::setupOutputTab() {
    m_outputTab = new QWidget(m_tabWidget);
    m_outputTab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_outputTab->setMinimumWidth(0);
    m_outputLayout = new QFormLayout(m_outputTab);
    m_outputLayout->setSpacing(12);
    m_outputLayout->setContentsMargins(8, 12, 8, 12);
    m_outputLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_outputLayout->setLabelAlignment(Qt::AlignLeft);
    m_outputLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_outputWidget = new OneSevenLivePropertiesWidget(m_outputTab);

    m_tabWidget->addTab(m_outputWidget, obs_module_text("MultiRTMP.Config.Tab.Output"));
}

void OneSevenLiveMultiRtmpConfigDialog::setupVideoTab() {
    m_videoTab = new QWidget(m_tabWidget);
    m_videoTab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_videoTab->setMinimumWidth(0);
    m_videoLayout = new QFormLayout(m_videoTab);
    m_videoLayout->setSpacing(12);
    m_videoLayout->setContentsMargins(8, 12, 8, 12);
    m_videoLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_videoLayout->setLabelAlignment(Qt::AlignLeft);
    m_videoLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_outputSceneCombo = new QComboBox(m_videoTab);
    m_videoLayout->addRow(obs_module_text("MultiRtmp.Config.Video.OutputScene"),
                          m_outputSceneCombo);

    m_videoEncoderCombo = new QComboBox(m_videoTab);
    m_videoLayout->addRow(obs_module_text("MultiRTMP.Config.Encoder.Video"), m_videoEncoderCombo);

    m_videoWidget = new OneSevenLivePropertiesWidget(m_videoTab);
    m_videoLayout->addWidget(m_videoWidget);

    m_tabWidget->addTab(m_videoTab, obs_module_text("MultiRTMP.Config.Tab.Video"));
}

void OneSevenLiveMultiRtmpConfigDialog::setupAudioTab() {
    m_audioTab = new QWidget(m_tabWidget);
    m_audioLayout = new QFormLayout(m_audioTab);
    m_audioLayout->setSpacing(12);
    m_audioLayout->setContentsMargins(8, 12, 8, 12);
    m_audioLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_audioLayout->setLabelAlignment(Qt::AlignLeft);
    m_audioLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_audioEncoderCombo = new QComboBox(m_audioTab);
    m_audioLayout->addRow(obs_module_text("MultiRTMP.Config.Encoder.Audio"), m_audioEncoderCombo);

    m_audioWidget = new OneSevenLivePropertiesWidget(m_audioTab);
    m_audioLayout->addWidget(m_audioWidget);

    m_tabWidget->addTab(m_audioTab, obs_module_text("MultiRTMP.Config.Tab.Audio"));
}

void OneSevenLiveMultiRtmpConfigDialog::setupButtonBox() {
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(12);
    m_buttonLayout->setContentsMargins(16, 16, 16, 16);

    m_cancelButton = new QPushButton(obs_module_text("MultiRtmp.Config.Cancel"));
    m_cancelButton->setFixedHeight(32);
    m_cancelButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_cancelButton->setStyleSheet("background-color: #666666; color: white;");

    m_okButton = new QPushButton(obs_module_text("MultiRtmp.Config.Confirm"));
    m_okButton->setFixedHeight(32);
    m_okButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_okButton->setStyleSheet("background-color: #FF0001; color: white;");
    m_okButton->setDefault(true);

    m_buttonLayout->addWidget(m_cancelButton, 1);
    m_buttonLayout->addWidget(m_okButton, 2);
}

void OneSevenLiveMultiRtmpConfigDialog::setupConnections() {
    // Advanced settings toggle
    connect(m_advancedButton, &QPushButton::clicked, this,
            &OneSevenLiveMultiRtmpConfigDialog::onAdvancedSettingsToggled);

    // Buttons
    connect(m_okButton, &QPushButton::clicked, this, &OneSevenLiveMultiRtmpConfigDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this,
            &OneSevenLiveMultiRtmpConfigDialog::reject);

    // Update authorize button whenever channel selection changes
    connect(m_streamNameCombo, &QComboBox::currentTextChanged, this,
            [this](const QString&) { updateAuthorizeButtonState(); });

    connect(m_streamNameCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        const char* svc = (text == "YouTube") ? "YouTube - RTMPS" : "Twitch";
        ObsDataPtr s{obs_data_create()};
        obs_data_set_string(s.get(), "service", svc);
        obs_service_t* tmp =
            obs_service_create("rtmp_common", "temp_service_refresh", s.get(), nullptr);
        s.reset();
        if (tmp) {
            obs_data_t* st = obs_service_get_settings(tmp);
            obs_properties_t* pr = obs_service_properties(tmp);
            if (st && pr && m_serviceWidget) {
                m_serviceWidget->UpdateProperties(st, pr);
            } else {
                if (pr)
                    obs_properties_destroy(pr);
                if (st)
                    obs_data_release(st);
            }
            obs_service_release(tmp);
        }
        if (text == "Twitch") {
            proc_handler_t* ph = obs_get_proc_handler();
            calldata_t cd;
            calldata_init(&cd);
            calldata_set_int(&cd, "seconds", 10);
            proc_handler_call(ph, "twitch_ingests_refresh", &cd);
            calldata_free(&cd);
        }
    });

    // Authorization failure handling
    if (m_twitchAuth) {
        connect(m_twitchAuth, &OneSevenLiveTwitchAuth::authorizationFailed, this,
                &OneSevenLiveMultiRtmpConfigDialog::onAuthorizationFailed);
    }
    if (m_youtubeAuth) {
        connect(m_youtubeAuth, &OneSevenLiveYouTubeAuth::authorizationFailed, this,
                &OneSevenLiveMultiRtmpConfigDialog::onAuthorizationFailed);
    }

    if (m_videoEncoderCombo) {
        connect(m_videoEncoderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { refreshVideoEncoderProperties(); });
    }
    if (m_audioEncoderCombo) {
        connect(m_audioEncoderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { refreshAudioEncoderProperties(); });
    }
}

void OneSevenLiveMultiRtmpConfigDialog::setEditMode(bool isEdit) {
    m_isEditMode = isEdit;
}

void OneSevenLiveMultiRtmpConfigDialog::accept() {
    // Note: SaveConfig() is called by the parent dialog (OneSevenLiveMultiRtmpDock)
    // to avoid double calls and potential memory issues
    obs_log(LOG_INFO,
            "[MultiRTMP-ConfigDialog] Dialog accepted, SaveConfig will be called by parent");
    QDialog::accept();
}

void OneSevenLiveMultiRtmpConfigDialog::reject() {
    QDialog::reject();
}

// Service-related slot functions removed - functionality integrated into basic info section

void OneSevenLiveMultiRtmpConfigDialog::onAdvancedSettingsToggled() {
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

void OneSevenLiveMultiRtmpConfigDialog::onAuthorizeClicked() {
    // Determine selected RTMP channel
    QString channel = m_streamNameCombo->currentText();

    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Authorize clicked for channel: %s",
            channel.isEmpty() ? "(none)" : channel.toUtf8().constData());
    if (channel != OneSevenLiveTwitchAuth::PLATFORM &&
        channel != OneSevenLiveYouTubeAuth::PLATFORM) {
        obs_log(LOG_WARNING, "Unknown type authorization cancelled");
        return;
    }

    if (m_isAuthorizing) {
        obs_log(LOG_WARNING, "Authorization already in progress");
        return;
    }

    m_isAuthorizing = true;
    m_lastAuthError.clear();

    QString authUrl;

    if (channel == OneSevenLiveTwitchAuth::PLATFORM) {
        // Handle Twitch authorization using device code flow
        QString redirectUri = OneSevenLiveTwitchAuth::TWITCH_CALLBACK_URI;
        authUrl = m_twitchAuth->getAuthUrl(redirectUri);
        obs_log(LOG_INFO, "Opening Twitch authorization URL: %s", authUrl.toStdString().c_str());
    } else if (channel == OneSevenLiveYouTubeAuth::PLATFORM) {
        // Build YouTube authorization URL (authorization code flow)
        OneSevenLiveCoreManager& coreManager = OneSevenLiveCoreManager::getInstance();
        QString redirectUri =
            QString("http://localhost:%1").arg(coreManager.getHttpServer()->getPort());
        authUrl = m_youtubeAuth->getAuthUrl(redirectUri);
        obs_log(LOG_INFO, "Opening YouTube authorization URL: %s", authUrl.toStdString().c_str());
    }

    // Show authorization dialog with embedded browser
    m_authDialog = new OneSevenLiveAuthDialog(authUrl, this);
    // Don't use DeleteOnClose, we will delete it manually after exec() returns
    // m_authDialog->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(m_authDialog, &OneSevenLiveAuthDialog::urlChanged, this,
            &OneSevenLiveMultiRtmpConfigDialog::onAuthUrlChanged);

    m_authDialog->exec();

    // Explicitly delete the dialog
    delete m_authDialog;
    m_authDialog = nullptr;
    m_isAuthorizing = false;

    // Check if there was an error during authorization (deferred display)
    if (!m_lastAuthError.isEmpty()) {
        // Use a 0-timer to allow the parent dialog to regain focus/activation properly
        // before showing the error message. This prevents the message box from being
        // obscured by the parent dialog on some platforms (macOS).
        QString error = m_lastAuthError;
        QTimer::singleShot(0, this, [this, error]() {
            QMessageBox::warning(
                this, QString::fromUtf8(obs_module_text("MultiRTMP.AuthorizationFailed.Title")),
                QString::fromUtf8(obs_module_text("MultiRTMP.AuthorizationFailed.Text")).arg(error),
                QMessageBox::Ok);
            updateAuthorizeButtonState();
        });
        m_lastAuthError.clear();
    }
}

void OneSevenLiveMultiRtmpConfigDialog::onAuthorizationFailed(const QString& error) {
    obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] Authorization failed: %s",
            error.toUtf8().constData());

    m_lastAuthError = error;

    // Close auth dialog if it's open
    if (m_authDialog) {
        // Just reject/close the dialog. Deletion is handled in onAuthorizeClicked after exec()
        // returns.
        m_authDialog->reject();
    } else {
        // If dialog is not open (unlikely in this flow), show error immediately
        // Use singleShot to ensure proper z-ordering
        QTimer::singleShot(0, this, [this, error]() {
            QMessageBox::warning(
                this, QString::fromUtf8(obs_module_text("MultiRTMP.AuthorizationFailed.Title")),
                QString::fromUtf8(obs_module_text("MultiRTMP.AuthorizationFailed.Text")).arg(error),
                QMessageBox::Ok);
            updateAuthorizeButtonState();
        });
        m_lastAuthError.clear();
    }

    m_isAuthorizing = false;
}

void OneSevenLiveMultiRtmpConfigDialog::onAuthUrlChanged(const QString& url) {
    QString channel = m_streamNameCombo->currentText();

    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] %s auth URL changed: %s",
            channel.toUtf8().constData(), url.toUtf8().constData());

    QString redirectUrl;
    if (channel == OneSevenLiveTwitchAuth::PLATFORM) {
        redirectUrl = OneSevenLiveTwitchAuth::TWITCH_CALLBACK_URI;
    } else if (channel == OneSevenLiveYouTubeAuth::PLATFORM) {
        redirectUrl = m_youtubeAuth->getRedirectUri();
    }

    // ignore the url not same with redirect url
    if (!url.startsWith(redirectUrl)) {
        return;
    }

    if (channel == OneSevenLiveTwitchAuth::PLATFORM) {
        m_twitchAuth->handleAuthorizationCallbackUrl(url);
    } else if (channel == OneSevenLiveYouTubeAuth::PLATFORM) {
        m_youtubeAuth->handleAuthorizationCallbackUrl(url);
    }

    if (m_authDialog) {
        QMetaObject::invokeMethod(m_authDialog, "accept", Qt::QueuedConnection);
    }

    // Update button state after potential token change
    updateAuthorizeButtonState();
}

void OneSevenLiveMultiRtmpConfigDialog::loadConfig() {
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
        // Ensure authorize button reflects token state for the selected channel
        updateAuthorizeButtonState();
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
        ObsDataPtr service_settings{nullptr};
        if (!m_config->serviceSettings.empty()) {
            service_settings.reset(
                obs_data_create_from_json(m_config->serviceSettings.dump().c_str()));
        }

        obs_service_t* service = obs_service_create(protocol_info->serviceId, "temp_service",
                                                    service_settings.get(), nullptr);
        if (!service) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to create OBS service with ID: %s",
                    protocol_info->serviceId);
            service_settings.reset();
            return;
        }

        obs_data_t* settings = obs_service_get_settings(service);
        // Pre-select service based on current channel to ensure proper server list
        if (settings && m_streamNameCombo) {
            const QString channel = m_streamNameCombo->currentText();
            const char* svcName = (channel == "YouTube") ? "YouTube - RTMPS" : "Twitch";
            obs_data_set_string(settings, "service", svcName);
            obs_service_update(service, settings);
        }
        obs_properties_t* props = obs_service_properties(service);

        if (!settings || !props) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to get service settings or properties");
            obs_service_release(service);
            service_settings.reset();
            return;
        }

        if (m_serviceWidget) {
            m_serviceWidget->UpdateProperties(settings, props);
        }

        // Ownership of 'settings' and 'props' is transferred to m_serviceWidget
        obs_service_release(service);
        service_settings.reset();
    }

    // Load output settings
    {
        ObsDataPtr output_settings{nullptr};
        if (!m_config->outputSettings.empty()) {
            output_settings.reset(
                obs_data_create_from_json(m_config->outputSettings.dump().c_str()));
        }

        obs_output_t* output = obs_output_create(protocol_info->outputId, "temp_output",
                                                 output_settings.get(), nullptr);
        if (!output) {
            obs_log(LOG_ERROR, "[loadConfig] Failed to create OBS output with ID: %s",
                    protocol_info->outputId);
            output_settings.reset();
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
            output_settings.reset();
            return;
        }

        if (!m_outputWidget) {
            obs_log(LOG_ERROR, "[loadConfig] m_outputWidget is null, cannot update properties");
            // Ownership of 'settings' and 'props' is transferred to m_outputWidget
            obs_output_release(output);
            output_settings.reset();
            return;
        }

        try {
            m_outputWidget->UpdateProperties(settings, props);
        } catch (const std::exception& e) {
            obs_log(LOG_ERROR, "[loadConfig] Exception in UpdateProperties: %s", e.what());
        } catch (...) {
            obs_log(LOG_ERROR, "[loadConfig] Unknown exception in UpdateProperties");
        }

        // Ownership of 'settings' and 'props' is transferred to m_outputWidget
        obs_output_release(output);
        output_settings.reset();
    }

    // Load video encoder selection and settings
    if (m_config->videoConfig.has_value()) {
        // Select encoder in combo (non-empty means custom encoder; empty means Use OBS)
        if (m_videoEncoderCombo) {
            const QString encId = QString::fromStdString(m_config->videoConfig->encoderId);
            if (!encId.isEmpty()) {
                int idx = m_videoEncoderCombo->findData(encId);
                if (idx >= 0)
                    m_videoEncoderCombo->setCurrentIndex(idx);
            } else {
                m_videoEncoderCombo->setCurrentIndex(0);
            }
        }

        if (!m_config->videoConfig->encoderSettings.empty()) {
            ObsDataPtr encoder_settings{
                obs_data_create_from_json(m_config->videoConfig->encoderSettings.dump().c_str())};

            obs_encoder_t* encoder =
                obs_video_encoder_create(m_config->videoConfig->encoderId.c_str(),
                                         "temp_video_encoder", encoder_settings.get(), nullptr);
            if (!encoder) {
                obs_log(LOG_ERROR, "[loadConfig] Failed to create video encoder with ID: %s",
                        m_config->videoConfig->encoderId.c_str());
                encoder_settings.reset();
                // Fall back to refreshing default properties if encoder cannot be created
                refreshVideoEncoderProperties();
            } else {
                obs_data_t* settings = obs_encoder_get_settings(encoder);
                obs_properties_t* props = obs_encoder_properties(encoder);

                if (!settings || !props) {
                    obs_log(LOG_ERROR,
                            "[loadConfig] Failed to get video encoder settings or properties "
                            "(settings: "
                            "%p, props: %p)",
                            (void*) settings, (void*) props);
                    obs_encoder_release(encoder);
                    encoder_settings.reset();
                    refreshVideoEncoderProperties();
                } else {
                    if (!m_videoWidget) {
                        obs_log(
                            LOG_ERROR,
                            "[loadConfig] m_videoWidget is null, cannot update video properties");
                        // Ownership of 'settings' and 'props' is transferred to m_videoWidget
                        obs_encoder_release(encoder);
                        encoder_settings.reset();
                        refreshVideoEncoderProperties();
                    } else {
                        try {
                            m_videoWidget->UpdateProperties(settings, props);
                            m_videoWidget->setVisible(true);
                        } catch (const std::exception& e) {
                            obs_log(LOG_ERROR,
                                    "[loadConfig] Exception in video UpdateProperties: %s",
                                    e.what());
                        } catch (...) {
                            obs_log(LOG_ERROR,
                                    "[loadConfig] Unknown exception in video UpdateProperties");
                        }

                        // Ownership of 'settings' and 'props' is transferred to m_videoWidget
                        obs_encoder_release(encoder);
                        encoder_settings.reset();
                    }
                }
            }
        } else {
            // No saved settings; just refresh properties for selected encoder
            refreshVideoEncoderProperties();
        }
        // Ensure properties reflect the selected encoder state
        if (m_videoEncoderCombo && m_videoEncoderCombo->currentData().toString().isEmpty()) {
            if (m_videoWidget)
                m_videoWidget->setVisible(false);
        } else {
            if (m_videoWidget)
                m_videoWidget->setVisible(true);
        }
    } else {
        // No videoConfig present → Use OBS
        if (m_videoEncoderCombo)
            m_videoEncoderCombo->setCurrentIndex(0);
        if (m_videoWidget)
            m_videoWidget->setVisible(false);
    }

    // Load audio encoder selection and settings
    if (m_config->audioConfig.has_value()) {
        if (m_audioEncoderCombo) {
            const QString encId = QString::fromStdString(m_config->audioConfig->encoderId);
            if (!encId.isEmpty()) {
                int idx = m_audioEncoderCombo->findData(encId);
                if (idx >= 0)
                    m_audioEncoderCombo->setCurrentIndex(idx);
            } else {
                m_audioEncoderCombo->setCurrentIndex(0);
            }
        }

        if (!m_config->audioConfig->encoderSettings.empty()) {
            ObsDataPtr encoder_settings{
                obs_data_create_from_json(m_config->audioConfig->encoderSettings.dump().c_str())};

            obs_encoder_t* encoder =
                obs_audio_encoder_create(m_config->audioConfig->encoderId.c_str(),
                                         "temp_audio_encoder", encoder_settings.get(), 0, nullptr);
            if (!encoder) {
                obs_log(LOG_ERROR, "[loadConfig] Failed to create audio encoder with ID: %s",
                        m_config->audioConfig->encoderId.c_str());
                encoder_settings.reset();
                refreshAudioEncoderProperties();
            } else {
                obs_data_t* settings = obs_encoder_get_settings(encoder);
                obs_properties_t* props = obs_encoder_properties(encoder);

                if (!settings || !props) {
                    obs_log(LOG_ERROR,
                            "[loadConfig] Failed to get audio encoder settings or properties "
                            "(settings: "
                            "%p, props: %p)",
                            (void*) settings, (void*) props);
                    obs_encoder_release(encoder);
                    encoder_settings.reset();
                    refreshAudioEncoderProperties();
                } else {
                    if (!m_audioWidget) {
                        obs_log(
                            LOG_ERROR,
                            "[loadConfig] m_audioWidget is null, cannot update audio properties");
                        // Ownership of 'settings' and 'props' is transferred to m_audioWidget
                        obs_encoder_release(encoder);
                        encoder_settings.reset();
                        refreshAudioEncoderProperties();
                    } else {
                        try {
                            m_audioWidget->UpdateProperties(settings, props);
                            m_audioWidget->setVisible(true);
                        } catch (const std::exception& e) {
                            obs_log(LOG_ERROR,
                                    "[loadConfig] Exception in audio UpdateProperties: %s",
                                    e.what());
                        } catch (...) {
                            obs_log(LOG_ERROR,
                                    "[loadConfig] Unknown exception in audio UpdateProperties");
                        }

                        // Ownership of 'settings' and 'props' is transferred to m_audioWidget
                        obs_encoder_release(encoder);
                        encoder_settings.reset();
                    }
                }
            }
        } else {
            refreshAudioEncoderProperties();
        }
        if (m_audioEncoderCombo && m_audioEncoderCombo->currentData().toString().isEmpty()) {
            if (m_audioWidget)
                m_audioWidget->setVisible(false);
        } else {
            if (m_audioWidget)
                m_audioWidget->setVisible(true);
        }
    } else {
        if (m_audioEncoderCombo)
            m_audioEncoderCombo->setCurrentIndex(0);
        if (m_audioWidget)
            m_audioWidget->setVisible(false);
    }
}

// Helper to set authorize button text/enabled based on token validity of selected channel
void OneSevenLiveMultiRtmpConfigDialog::updateAuthorizeButtonState() {
    if (!m_authorizeButton) {
        return;
    }

    const QString channel = m_streamNameCombo ? m_streamNameCombo->currentText() : QString();

    bool isAuthorized = false;
    if (channel == "YouTube") {
        isAuthorized = (m_youtubeAuth && m_youtubeAuth->hasValidToken());
    } else if (channel == "Twitch") {
        isAuthorized = (m_twitchAuth && m_twitchAuth->hasValidToken());
    }

    if (isAuthorized) {
        m_authorizeButton->setText(obs_module_text("MultiRtmp.Config.Deauthorize"));
        m_authorizeButton->setEnabled(true);
        if (m_serviceWidget)
            m_serviceWidget->setVisible(false);
    } else {
        m_authorizeButton->setText(obs_module_text("MultiRtmp.Config.Authorize"));
        m_authorizeButton->setEnabled(true);
        if (m_serviceWidget)
            m_serviceWidget->setVisible(true);
    }
}

OneSevenLiveMultiRtmpConfig OneSevenLiveMultiRtmpConfigDialog::SaveConfig() const {
    obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] SaveConfig called - starting configuration save");

    try {
        OneSevenLiveMultiRtmpConfig config;

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
        if (m_isEditMode && m_config) {
            config.streamName = m_config->streamName;
        } else {
            if (!m_streamNameCombo) {
                obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_streamNameCombo is null");
                throw std::runtime_error("Stream name combo widget is null");
            }
            config.streamName = m_streamNameCombo->currentText().toStdString();
        }
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] RTMP channel (stream name): '%s'",
                config.streamName.c_str());

        if (!m_protocolCombo) {
            obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_protocolCombo is null");
            throw std::runtime_error("Protocol combo widget is null");
        }
        config.protocol = m_protocolCombo->currentData().toString().toStdString();
        obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Protocol: '%s'", config.protocol.c_str());

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
        QString vidId =
            m_videoEncoderCombo ? m_videoEncoderCombo->currentData().toString() : QString();
        bool useObsVideo = vidId.isEmpty();
        if (!useObsVideo) {
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving custom video configuration...");
            OneSevenLiveMultiRtmpVideoConfig vcfg;

            vcfg.encoderId = vidId.toStdString();
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video encoder ID: '%s'",
                    vcfg.encoderId.c_str());

            if (m_outputSceneCombo) {
                const QVariant data = m_outputSceneCombo->currentData();
                vcfg.outputScene = data.isValid() ? data.toString().toStdString()
                                                  : m_outputSceneCombo->currentText().toStdString();
                obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Output scene: '%s'",
                        vcfg.outputScene.c_str());
            }

            if (!m_videoWidget) {
                obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_videoWidget is null");
                vcfg.encoderSettings = nlohmann::json::object();
            } else {
                vcfg.encoderSettings = m_videoWidget->SaveData();
            }
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving video encoder settings...");
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Video encoder settings saved successfully");

            config.videoConfig = vcfg;
        } else {
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Using OBS video settings");
            config.videoConfig.reset();
        }

        // Audio configuration
        QString audId =
            m_audioEncoderCombo ? m_audioEncoderCombo->currentData().toString() : QString();
        bool useObsAudio = audId.isEmpty();
        if (!useObsAudio) {
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving custom audio configuration...");
            OneSevenLiveMultiRtmpAudioConfig acfg;

            acfg.encoderId = audId.toStdString();
            obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Audio encoder ID: '%s'",
                    acfg.encoderId.c_str());

            if (!m_audioWidget) {
                obs_log(LOG_ERROR, "[MultiRTMP-ConfigDialog] m_audioWidget is null");
                acfg.encoderSettings = nlohmann::json::object();
            } else {
                obs_log(LOG_INFO, "[MultiRTMP-ConfigDialog] Saving audio encoder settings...");
                acfg.encoderSettings = m_audioWidget->SaveData();
                obs_log(LOG_INFO,
                        "[MultiRTMP-ConfigDialog] Audio encoder settings saved successfully");
            }

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

void OneSevenLiveMultiRtmpConfigDialog::loadScenes() {
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

std::vector<std::string> OneSevenLiveMultiRtmpConfigDialog::parseAndLoadEncoders(
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

void OneSevenLiveMultiRtmpConfigDialog::loadEncoders() {
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

        QString channelSel = m_streamNameCombo ? m_streamNameCombo->currentText() : QString();
        const char* svcName = (channelSel == "YouTube") ? "YouTube - RTMPS" : "Twitch";
        ObsDataPtr s{obs_data_create()};
        obs_data_set_string(s.get(), "service", svcName);
        obs_service_t* tmp =
            obs_service_create("rtmp_common", "temp_codec_service_v", s.get(), nullptr);
        s.reset();
        std::set<std::string> vset;
        const char** vcodecs = nullptr;
        if (tmp)
            vcodecs = obs_service_get_supported_video_codecs(tmp);
        if (vcodecs) {
            for (size_t i = 0; vcodecs[i]; ++i)
                vset.insert(vcodecs[i]);
        } else {
            const char* list = obs_get_output_supported_video_codecs("rtmp_output");
            if (list && *list) {
                std::string l(list);
                size_t pos;
                while ((pos = l.find(';')) != std::string::npos) {
                    std::string tok = l.substr(0, pos);
                    if (!tok.empty())
                        vset.insert(tok);
                    l.erase(0, pos + 1);
                }
                if (!l.empty())
                    vset.insert(l);
            }
        }

        size_t i = 0;
        const char* encId = nullptr;
        while (obs_enum_encoder_types(i++, &encId)) {
            if (!encId)
                continue;
            if (obs_get_encoder_type(encId) != OBS_ENCODER_VIDEO)
                continue;
            uint32_t caps = obs_get_encoder_caps(encId);
            if (caps & OBS_ENCODER_CAP_DEPRECATED)
                continue;
            const char* codec = obs_get_encoder_codec(encId);
            if (!codec || vset.find(codec) == vset.end())
                continue;
            m_videoEncoderCombo->addItem(ui_text(encId).c_str(), QString::fromUtf8(encId));
        }
        if (tmp)
            obs_service_release(tmp);
        int idx = m_videoEncoderCombo->findData(old);
        if (idx >= 0)
            m_videoEncoderCombo->setCurrentIndex(idx);
        refreshVideoEncoderProperties();
    }

    // Audio encoders
    if (m_audioEncoderCombo) {
        QVariant old = m_audioEncoderCombo->currentData();
        m_audioEncoderCombo->clear();
        m_audioEncoderCombo->addItem(obs_module_text("MultiRtmp.Config.Audio.UseOBS"),
                                     streamingAudioId ? streamingAudioId : "");

        QString channelSelA = m_streamNameCombo ? m_streamNameCombo->currentText() : QString();
        const char* svcNameA = (channelSelA == "YouTube") ? "YouTube - RTMPS" : "Twitch";
        ObsDataPtr sa{obs_data_create()};
        obs_data_set_string(sa.get(), "service", svcNameA);
        obs_service_t* tmpa =
            obs_service_create("rtmp_common", "temp_codec_service_a", sa.get(), nullptr);
        sa.reset();
        std::set<std::string> aset;
        const char** acodecs = nullptr;
        if (tmpa)
            acodecs = obs_service_get_supported_audio_codecs(tmpa);
        if (acodecs) {
            for (size_t i2 = 0; acodecs[i2]; ++i2)
                aset.insert(acodecs[i2]);
        } else {
            const char* list = obs_get_output_supported_audio_codecs("rtmp_output");
            if (list && *list) {
                std::string l(list);
                size_t pos;
                while ((pos = l.find(';')) != std::string::npos) {
                    std::string tok = l.substr(0, pos);
                    if (!tok.empty())
                        aset.insert(tok);
                    l.erase(0, pos + 1);
                }
                if (!l.empty())
                    aset.insert(l);
            }
        }

        size_t j = 0;
        const char* aeId = nullptr;
        while (obs_enum_encoder_types(j++, &aeId)) {
            if (!aeId)
                continue;
            if (obs_get_encoder_type(aeId) != OBS_ENCODER_AUDIO)
                continue;
            uint32_t caps = obs_get_encoder_caps(aeId);
            if (caps & OBS_ENCODER_CAP_DEPRECATED)
                continue;
            const char* codec = obs_get_encoder_codec(aeId);
            if (!codec || aset.find(codec) == aset.end())
                continue;
            m_audioEncoderCombo->addItem(ui_text(aeId).c_str(), QString::fromUtf8(aeId));
        }
        if (tmpa)
            obs_service_release(tmpa);
        int idx = m_audioEncoderCombo->findData(old);
        if (idx >= 0)
            m_audioEncoderCombo->setCurrentIndex(idx);
        refreshAudioEncoderProperties();
    }
}

void OneSevenLiveMultiRtmpConfigDialog::refreshVideoEncoderProperties() {
    if (!m_videoWidget || !m_videoEncoderCombo)
        return;
    QString id = m_videoEncoderCombo->currentData().toString();
    if (id.isEmpty()) {
        m_videoWidget->setVisible(false);
        return;
    }
    m_videoWidget->setVisible(true);
    ObsDataPtr initSettings{obs_data_create()};
    obs_encoder_t* enc = obs_video_encoder_create(
        id.toUtf8().constData(), "temp_video_encoder_props", initSettings.get(), nullptr);
    initSettings.reset();
    if (!enc)
        return;
    obs_data_t* settings = obs_encoder_get_settings(enc);
    obs_properties_t* props = obs_encoder_properties(enc);
    if (settings && props) {
        m_videoWidget->UpdateProperties(settings, props);
    }
    obs_encoder_release(enc);
}

void OneSevenLiveMultiRtmpConfigDialog::refreshAudioEncoderProperties() {
    if (!m_audioWidget || !m_audioEncoderCombo)
        return;
    QString id = m_audioEncoderCombo->currentData().toString();
    if (id.isEmpty()) {
        m_audioWidget->setVisible(false);
        return;
    }
    m_audioWidget->setVisible(true);
    ObsDataPtr initSettings{obs_data_create()};
    obs_encoder_t* enc = obs_audio_encoder_create(
        id.toUtf8().constData(), "temp_audio_encoder_props", initSettings.get(), 0, nullptr);
    initSettings.reset();
    if (!enc)
        return;
    obs_data_t* settings = obs_encoder_get_settings(enc);
    obs_properties_t* props = obs_encoder_properties(enc);
    if (settings && props) {
        m_audioWidget->UpdateProperties(settings, props);
    }
    obs_encoder_release(enc);
}
