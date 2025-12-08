#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "multi-rtmp/OneSevenLiveMultiRtmpModels.hpp"

// Forward declarations
class OneSevenLivePropertiesWidget;
class OneSevenLiveTwitchAuth;
class OneSevenLiveYouTubeAuth;
class OneSevenLiveAuthDialog;

/**
 * Configuration dialog for Multi-RTMP stream settings
 * Provides comprehensive configuration interface with multiple tabs
 */
class OneSevenLiveMultiRtmpConfigDialog : public QDialog {
    Q_OBJECT

   public:
    explicit OneSevenLiveMultiRtmpConfigDialog(
        QWidget* parent = nullptr, std::shared_ptr<OneSevenLiveMultiRtmpConfig> config = nullptr,
        bool isEditMode = false);
    ~OneSevenLiveMultiRtmpConfigDialog();

    // Dialog modes
    void setEditMode(bool isEdit);

    bool isEditMode() const {
        return m_isEditMode;
    }

    // Configuration access
    OneSevenLiveMultiRtmpConfig SaveConfig() const;

   public slots:
    void accept() override;
    void reject() override;

   private slots:
    void onAdvancedSettingsToggled();
    void onAuthorizeClicked();
    void onAuthorizationFailed(const QString& error);

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

    void loadEncoders();
    void refreshVideoEncoderProperties();
    void refreshAudioEncoderProperties();
    void loadScenes();
    void loadConfig();

    // Update authorize button based on selected channel token validity
    void updateAuthorizeButtonState();

    // Helper function to parse and load encoders for both video and audio
    std::vector<std::string> parseAndLoadEncoders(const std::string& supportedEncoders,
                                                  bool isVideoEncoder);

    std::shared_ptr<OneSevenLiveMultiRtmpConfig> m_config;
    std::shared_ptr<OneSevenLiveMultiRtmpConfig> m_originalConfig;

    std::string m_supportedVideoEncoders;
    std::string m_supportedAudioEncoders;

    // Main layout
    QVBoxLayout* m_mainLayout = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QScrollArea* m_scrollArea{nullptr};
    QWidget* m_container{nullptr};

    // Basic info section
    QWidget* m_basicInfoWidget = nullptr;
    QFormLayout* m_basicInfoLayout = nullptr;
    QComboBox* m_streamNameCombo = nullptr;
    QPushButton* m_authorizeButton = nullptr;
    QComboBox* m_protocolCombo = nullptr;
    OneSevenLivePropertiesWidget* m_serviceWidget = nullptr;

    // Advanced settings section
    QPushButton* m_advancedButton = nullptr;
    QWidget* m_advancedWidget = nullptr;
    bool m_advancedExpanded = false;
    int m_baseHeight = 0;

    // Output tab
    QWidget* m_outputTab = nullptr;
    QFormLayout* m_outputLayout = nullptr;
    OneSevenLivePropertiesWidget* m_outputWidget = nullptr;

    // Video tab
    QWidget* m_videoTab = nullptr;
    QFormLayout* m_videoLayout = nullptr;
    QComboBox* m_videoEncoderCombo = nullptr;
    QComboBox* m_outputSceneCombo = nullptr;
    OneSevenLivePropertiesWidget* m_videoWidget = nullptr;

    // Audio tab
    QWidget* m_audioTab = nullptr;
    QFormLayout* m_audioLayout = nullptr;
    QComboBox* m_audioEncoderCombo = nullptr;
    OneSevenLivePropertiesWidget* m_audioWidget = nullptr;

    // Button box
    QHBoxLayout* m_buttonLayout = nullptr;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    // State
    bool m_isEditMode;

    bool m_isAuthorizing;
    // Twitch authorization (non-owning; managed by CoreManager)
    OneSevenLiveTwitchAuth* m_twitchAuth{nullptr};
    // YouTube authorization (non-owning; managed by CoreManager)
    OneSevenLiveYouTubeAuth* m_youtubeAuth{nullptr};

    // Private slots for authorization
    void onAuthUrlChanged(const QString& url);

    OneSevenLiveAuthDialog* m_authDialog{nullptr};
    QString m_lastAuthError;

    void* m_tmpServiceProps{nullptr};
    void resizeEvent(QResizeEvent* event) override;
};
