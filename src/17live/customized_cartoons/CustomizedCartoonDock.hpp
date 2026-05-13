#pragma once

#include <QDockWidget>
#include <QPointer>

class QListWidget;
class QTableWidget;
class QPushButton;
class QLabel;
class QComboBox;
class QDoubleSpinBox;

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
    void saveRulesToConfig();
    void refreshPositionUi();

    CustomizedCartoonService* service_{nullptr};

    QListWidget* mediaList_{nullptr};
    QPushButton* addMediaButton_{nullptr};
    QLabel* mediaCountLabel_{nullptr};

    QComboBox* orientationCombo_{nullptr};
    QDoubleSpinBox* posXSpin_{nullptr};
    QDoubleSpinBox* posYSpin_{nullptr};
    QDoubleSpinBox* scaleXSpin_{nullptr};
    QDoubleSpinBox* scaleYSpin_{nullptr};
    QDoubleSpinBox* rotSpin_{nullptr};
    QPushButton* applyPositionButton_{nullptr};
    QPushButton* readPositionButton_{nullptr};
    QPushButton* startPositionPreviewButton_{nullptr};
    QPushButton* stopPositionPreviewButton_{nullptr};

    QTableWidget* ruleTable_{nullptr};
    QPushButton* addRuleButton_{nullptr};
    QPushButton* removeRuleButton_{nullptr};
    QPushButton* previewButton_{nullptr};

    QTableWidget* progressTable_{nullptr};
};
