#pragma once

#include <QDockWidget>
#include <QCloseEvent>
#include <QPointer>
#include <nlohmann/json.hpp>

class QListWidget;
class QTableWidget;
class QPushButton;
class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QTabWidget;
class QVBoxLayout;
class QWidget;
class QScrollArea;
class QTimer;
class QEvent;
class QWheelEvent;
class QGraphicsOpacityEffect;
class QPropertyAnimation;

class CustomizedCartoonService;

class CustomizedCartoonDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit CustomizedCartoonDock(QWidget* parent, CustomizedCartoonService* service);
    ~CustomizedCartoonDock() override;

   protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

   private slots:
    void refreshUi();
    void refreshProgress();
    void onAddMedia();
    void onRemoveMedia();
    void onAddRule();
    void onRemoveRule();
    void onPreview();
    void onOrientationChanged(int index);
    void onApplyPosition();
    void onReadPositionFromCanvas();
    void onStartPositionPreview();
    void onStopPositionPreview();
    void setBroadcastState(bool streamingActive, bool recordingActive);

   private:
    void setupUi();
    void loadFromConfig();
    void refreshMediaList();
    void refreshPositionUi();
    void rebuildRulesUi();
    void openMediaSettingsDialog(const QString& mediaId);
    void openHelpDialog();
    nlohmann::json buildCurrentPositionDraft(bool landscape) const;
    void resetDraftFromService();
    void updatePositionDraft(bool landscape, const QRect& rect);
    bool saveDraft();
    bool saveAndApplyDraft();
    bool confirmCloseWithUnsavedChanges();
    void closeDock();
    void syncPreviewDraftTransform();
    void updateMediaPreviewAvailability();
    void updateDraftUi();
    bool isPreviewBlocked() const;
    QString previewBlockedTooltip() const;
    void showToast(const QString& text, bool danger = false);
    void repositionToast();
    bool redirectWheelToSettingsScroll(QWheelEvent* event);
    bool isDraftDirty() const;
    bool removeMediaFromDraft(const QString& mediaId);
    bool isSettingsTabDirty() const;
    bool isRulesTabDirty() const;
    bool isPositionOrientationDirty(bool landscape) const;

    CustomizedCartoonService* service_{nullptr};

    QListWidget* mediaList_{nullptr};
    QPushButton* addMediaButton_{nullptr};
    QLabel* mediaCountLabel_{nullptr};
    QTabWidget* mainTabWidget_{nullptr};

    QTabWidget* positionTabWidget_{nullptr};
    QLabel* canvasRangeLabel_{nullptr};
    QWidget* positionCanvas_{nullptr};
    QSpinBox* posXSpin_{nullptr};
    QSpinBox* posYSpin_{nullptr};
    QSpinBox* widthSpin_{nullptr};
    QSpinBox* heightSpin_{nullptr};
    QPushButton* applyPositionButton_{nullptr};
    QPushButton* readPositionButton_{nullptr};
    QPushButton* startPositionPreviewButton_{nullptr};
    QPushButton* stopPositionPreviewButton_{nullptr};
    QScrollArea* settingsScrollArea_{nullptr};

    QScrollArea* rulesScrollArea_{nullptr};
    QWidget* rulesListContainer_{nullptr};
    QVBoxLayout* rulesListLayout_{nullptr};
    bool pendingScrollToLatestRule_{false};
    QPushButton* addRuleButton_{nullptr};
    QPushButton* previewButton_{nullptr};
    QLabel* draftStatusLabel_{nullptr};
    QPushButton* cancelButton_{nullptr};
    QPushButton* confirmButton_{nullptr};
    QPushButton* applyButton_{nullptr};

    QTableWidget* progressTable_{nullptr};
    bool streamingActive_{false};
    bool recordingActive_{false};

    QWidget* rootWidget_{nullptr};
    QWidget* toastWidget_{nullptr};
    QLabel* toastLabel_{nullptr};
    QTimer* toastTimer_{nullptr};
    QGraphicsOpacityEffect* toastOpacity_{nullptr};
    QPropertyAnimation* toastAnim_{nullptr};

    nlohmann::json savedConfig_{nlohmann::json::object()};
    nlohmann::json draftConfig_{nlohmann::json::object()};
    bool draftDirty_{false};
    bool bypassClosePrompt_{false};
};
