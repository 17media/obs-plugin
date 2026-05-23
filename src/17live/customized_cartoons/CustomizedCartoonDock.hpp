#pragma once

#include <QDockWidget>
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

class CustomizedCartoonService;

class CustomizedCartoonDock : public QDockWidget {
    Q_OBJECT

   public:
    explicit CustomizedCartoonDock(QWidget* parent, CustomizedCartoonService* service);

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

   private:
    void setupUi();
    void loadFromConfig();
    void refreshMediaList();
    void saveRulesToConfig();
    void refreshPositionUi();
    void rebuildRulesUi();
    void openMediaSettingsDialog(const QString& mediaId);
    void openHelpDialog();
    nlohmann::json buildCurrentPositionDraft(bool landscape) const;
    void syncPreviewDraftTransform();

    CustomizedCartoonService* service_{nullptr};

    QListWidget* mediaList_{nullptr};
    QPushButton* addMediaButton_{nullptr};
    QLabel* mediaCountLabel_{nullptr};

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

    QScrollArea* rulesScrollArea_{nullptr};
    QWidget* rulesListContainer_{nullptr};
    QVBoxLayout* rulesListLayout_{nullptr};
    bool pendingScrollToLatestRule_{false};
    QPushButton* addRuleButton_{nullptr};
    QPushButton* previewButton_{nullptr};
    QPushButton* cancelButton_{nullptr};
    QPushButton* confirmButton_{nullptr};
    QPushButton* applyButton_{nullptr};

    QTableWidget* progressTable_{nullptr};
};
