#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QStackedWidget>
#include <QDateTime>
#include <memory>

#include "../OneSevenLiveYouTubeClient.hpp"

class OneSevenLiveYouTubeBroadcastDialog : public QDialog {
    Q_OBJECT

public:
    explicit OneSevenLiveYouTubeBroadcastDialog(QWidget* parent = nullptr);
    ~OneSevenLiveYouTubeBroadcastDialog();

    QString getBroadcastId() const { return m_selectedBroadcastId; }
    QString getLiveChatId() const { return m_selectedLiveChatId; }
    QString getStreamName() const { return m_streamName; }
    QString getIngestionUrl() const { return m_ingestionUrl; }
    QString getStreamKey() const { return m_streamKey; }

private slots:
    void onRefreshClicked();
    void onCreateNewClicked();
    void onSelectClicked();
    void onBackClicked();
    void onCreateConfirmClicked();
    
    // API Callbacks
    void onBroadcastsReceived(const YouTubeLiveBroadcastListResponse& response);
    void onStreamsReceived(const YouTubeLiveStreamListResponse& response);
    void onBroadcastCreated(const QString& broadcastId);
    void onStreamCreated(const YouTubeLiveStream& stream);
    void onBroadcastBound(const QString& broadcastId, const QString& streamId);
    void onSingleBroadcastReceived(const YouTubeLiveBroadcast& broadcast);
    void onBroadcastTransitioned(const QString& broadcastId, const QString& status);
    void onError(const QString& error, const QString& operation);

private:
    void setupUI();
    void setupConnections();
    void loadBroadcasts();
    void fetchNextBroadcastBatch();
    void createNewBroadcast(const QString& title, const QString& privacy, const QString& latency, bool autoStart, bool autoStop, bool dvr, bool scheduleLater);
    void processSelection(const QString& broadcastId, const QString& title, const QString& chatId);
    void bindAndFinish(const QString& broadcastId, const QString& streamId);

    // UI Elements
    QStackedWidget* m_stackedWidget;
    
    // Page 1: List
    QWidget* m_listPage;
    QListWidget* m_broadcastList;
    QPushButton* m_refreshButton;
    QPushButton* m_createButton;
    QPushButton* m_selectButton;
    
    // Page 2: Create
    QWidget* m_createPage;
    QLineEdit* m_titleEdit;
    QComboBox* m_privacyCombo;
    QComboBox* m_latencyCombo;
    QCheckBox* m_autoStartCheck;
    QCheckBox* m_autoStopCheck;
    QCheckBox* m_dvrCheck;
    QCheckBox* m_scheduleCheck;
    QPushButton* m_confirmCreateButton;
    QPushButton* m_backButton;

    // Data
    OneSevenLiveYouTubeClient* m_client;
    QStringList m_loadingStatuses;
    QString m_currentLoadingStatus;
    QString m_selectedBroadcastId;
    QString m_selectedLiveChatId;
    QString m_selectedTitle;
    
    // Stream Info for output
    QString m_streamName;
    QString m_ingestionUrl;
    QString m_streamKey;
    
    // Temp data during creation/binding
    QString m_pendingBroadcastId;
    bool m_isCreating;
    bool m_autoStartEnabled;
    QVector<YouTubeLiveStream> m_availableStreams;
};
