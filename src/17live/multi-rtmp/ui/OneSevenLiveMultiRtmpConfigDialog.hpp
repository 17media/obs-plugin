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
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QResizeEvent>
#include <QScrollArea>

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
        QWidget* parent = nullptr, std::shared_ptr<OneSevenLiveMultiRtmpConfig> config = nullptr);
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
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    QScrollArea* m_scrollArea{nullptr};
    QWidget* m_container{nullptr};

    // Basic info section
    QWidget* m_basicInfoWidget;
    QFormLayout* m_basicInfoLayout;
    QComboBox* m_streamNameCombo;
    QPushButton* m_authorizeButton;
    QComboBox* m_protocolCombo;
    OneSevenLivePropertiesWidget* m_serviceWidget;

    // Advanced settings section
    QPushButton* m_advancedButton;
    QWidget* m_advancedWidget;
    bool m_advancedExpanded;
    int m_baseHeight;

    // Output tab
    QWidget* m_outputTab;
    QFormLayout* m_outputLayout;
    OneSevenLivePropertiesWidget* m_outputWidget;

    // Video tab
    QWidget* m_videoTab;
    QFormLayout* m_videoLayout;
    QComboBox* m_videoEncoderCombo;
    QComboBox* m_videoResolutionCombo;
    QComboBox* m_fpsDenominatorCombo;
    QComboBox* m_outputSceneCombo;
    OneSevenLivePropertiesWidget* m_videoWidget;

    // Audio tab
    QWidget* m_audioTab;
    QFormLayout* m_audioLayout;
    QComboBox* m_audioEncoderCombo;
    OneSevenLivePropertiesWidget* m_audioWidget;

    // Button box
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

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

    void* m_tmpServiceProps{nullptr};
    void resizeEvent(QResizeEvent* event) override;
};
