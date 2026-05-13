#include "CustomizedCartoonDock.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>

#include <unordered_map>

#include "CustomizedCartoonService.hpp"

using json = nlohmann::json;

CustomizedCartoonDock::CustomizedCartoonDock(QWidget* parent, CustomizedCartoonService* service)
    : QDockWidget(parent), service_(service) {
    setWindowTitle(obs_module_text("CustomizedCartoon.Dock.Title"));
    setupUi();
    if (service_) {
        connect(service_, &CustomizedCartoonService::configChanged, this,
                &CustomizedCartoonDock::refreshUi);
        connect(service_, &CustomizedCartoonService::progressUpdated, this,
                &CustomizedCartoonDock::refreshProgress);
    }
    refreshUi();
}

void CustomizedCartoonDock::setupUi() {
    resize(980, 820);
    setMinimumSize(980, 820);

    auto* root = new QWidget(this);
    root->setMinimumSize(980, 820);
    auto* mainLayout = new QHBoxLayout(root);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(30);

    auto* leftContainer = new QWidget(root);
    leftContainer->setFixedWidth(400);
    auto* left = new QVBoxLayout(leftContainer);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(30);

    auto* mediaPanel = new QWidget(leftContainer);
    mediaPanel->setFixedSize(400, 269);
    auto* mediaLayout = new QVBoxLayout(mediaPanel);
    mediaLayout->setContentsMargins(0, 0, 0, 0);
    mediaLayout->setSpacing(14);

    auto* mediaTitle = new QLabel(obs_module_text("CustomizedCartoon.Media.VideoSetupTitle"), mediaPanel);
    QFont titleFont = mediaTitle->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    mediaTitle->setFont(titleFont);

    mediaList_ = new QListWidget(mediaPanel);
    mediaList_->setFrameShape(QFrame::NoFrame);
    mediaList_->setSpacing(12);
    mediaList_->setSelectionMode(QAbstractItemView::SingleSelection);
    mediaList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mediaList_->setStyleSheet("QListWidget { background: transparent; }");

    auto* mediaBottom = new QHBoxLayout();
    mediaBottom->setContentsMargins(0, 0, 0, 0);
    mediaBottom->setSpacing(12);

    mediaCountLabel_ = new QLabel(mediaPanel);
    QFont countFont = mediaCountLabel_->font();
    countFont.setPointSize(16);
    mediaCountLabel_->setFont(countFont);
    mediaCountLabel_->setStyleSheet("QLabel { color: white; }");

    addMediaButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Media.ChooseVideo"), mediaPanel);
    addMediaButton_->setMinimumHeight(48);
    addMediaButton_->setStyleSheet(
        "QPushButton { background-color: #007AFF; color: white; font-size: 16px; "
        "border-radius: 6px; padding: 10px 18px; }"
        "QPushButton:hover { background-color: #0A84FF; }"
        "QPushButton:pressed { background-color: #0060DF; }");

    mediaBottom->addWidget(mediaCountLabel_, 1);
    mediaBottom->addWidget(addMediaButton_, 0, Qt::AlignRight);

    mediaLayout->addWidget(mediaTitle);
    mediaLayout->addWidget(mediaList_, 1);
    mediaLayout->addLayout(mediaBottom);

    left->addWidget(mediaPanel);

    connect(addMediaButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onAddMedia);

    auto* positionGroup = new QGroupBox(obs_module_text("CustomizedCartoon.Position.Title"), leftContainer);
    positionGroup->setFixedSize(400, 425);
    auto* positionLayout = new QVBoxLayout(positionGroup);
    auto* positionForm = new QFormLayout();

    orientationCombo_ = new QComboBox(positionGroup);
    orientationCombo_->addItem(obs_module_text("CustomizedCartoon.Position.Orientation.Landscape"));
    orientationCombo_->addItem(obs_module_text("CustomizedCartoon.Position.Orientation.Portrait"));

    posXSpin_ = new QDoubleSpinBox(positionGroup);
    posXSpin_->setRange(-100000, 100000);
    posXSpin_->setDecimals(2);

    posYSpin_ = new QDoubleSpinBox(positionGroup);
    posYSpin_->setRange(-100000, 100000);
    posYSpin_->setDecimals(2);

    scaleXSpin_ = new QDoubleSpinBox(positionGroup);
    scaleXSpin_->setRange(0.01, 100.0);
    scaleXSpin_->setDecimals(3);
    scaleXSpin_->setValue(1.0);

    scaleYSpin_ = new QDoubleSpinBox(positionGroup);
    scaleYSpin_->setRange(0.01, 100.0);
    scaleYSpin_->setDecimals(3);
    scaleYSpin_->setValue(1.0);

    rotSpin_ = new QDoubleSpinBox(positionGroup);
    rotSpin_->setRange(-360.0, 360.0);
    rotSpin_->setDecimals(2);

    positionForm->addRow(obs_module_text("CustomizedCartoon.Position.Orientation"), orientationCombo_);
    positionForm->addRow(obs_module_text("CustomizedCartoon.Position.X"), posXSpin_);
    positionForm->addRow(obs_module_text("CustomizedCartoon.Position.Y"), posYSpin_);
    positionForm->addRow(obs_module_text("CustomizedCartoon.Position.ScaleX"), scaleXSpin_);
    positionForm->addRow(obs_module_text("CustomizedCartoon.Position.ScaleY"), scaleYSpin_);
    positionForm->addRow(obs_module_text("CustomizedCartoon.Position.Rot"), rotSpin_);
    positionLayout->addLayout(positionForm);

    auto* positionButtons = new QHBoxLayout();
    startPositionPreviewButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Position.Preview"), positionGroup);
    stopPositionPreviewButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Position.StopPreview"), positionGroup);
    readPositionButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Position.ReadFromCanvas"), positionGroup);
    applyPositionButton_ =
        new QPushButton(obs_module_text("CustomizedCartoon.Position.Apply"), positionGroup);

    positionButtons->addWidget(startPositionPreviewButton_);
    positionButtons->addWidget(stopPositionPreviewButton_);
    positionButtons->addWidget(readPositionButton_);
    positionButtons->addWidget(applyPositionButton_);
    positionLayout->addLayout(positionButtons);

    left->addWidget(positionGroup);

    connect(orientationCombo_, &QComboBox::currentIndexChanged, this,
            &CustomizedCartoonDock::onOrientationChanged);
    connect(applyPositionButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onApplyPosition);
    connect(readPositionButton_, &QPushButton::clicked, this,
            &CustomizedCartoonDock::onReadPositionFromCanvas);
    connect(startPositionPreviewButton_, &QPushButton::clicked, this,
            &CustomizedCartoonDock::onStartPositionPreview);
    connect(stopPositionPreviewButton_, &QPushButton::clicked, this,
            &CustomizedCartoonDock::onStopPositionPreview);

    left->addStretch(1);

    auto* rightPanel = new QWidget(root);
    rightPanel->setFixedSize(550, 704);
    auto* right = new QVBoxLayout(rightPanel);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(24);

    auto* ruleGroup = new QGroupBox(obs_module_text("CustomizedCartoon.Rules.Title"), rightPanel);
    auto* ruleLayout = new QVBoxLayout(ruleGroup);
    ruleTable_ = new QTableWidget(ruleGroup);
    ruleTable_->setColumnCount(6);
    ruleTable_->setHorizontalHeaderLabels(
        {obs_module_text("CustomizedCartoon.Rules.Col.Name"),
         obs_module_text("CustomizedCartoon.Rules.Col.Type"),
         obs_module_text("CustomizedCartoon.Rules.Col.Params"),
         obs_module_text("CustomizedCartoon.Rules.Col.Media"),
         obs_module_text("CustomizedCartoon.Rules.Col.Repeat"),
         obs_module_text("CustomizedCartoon.Rules.Col.Enabled")});
    ruleTable_->horizontalHeader()->setStretchLastSection(true);
    ruleTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ruleTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    ruleTable_->verticalHeader()->setVisible(false);
    ruleLayout->addWidget(ruleTable_);

    auto* ruleButtons = new QHBoxLayout();
    addRuleButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Rules.Add"), ruleGroup);
    removeRuleButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Rules.Remove"), ruleGroup);
    previewButton_ = new QPushButton(obs_module_text("CustomizedCartoon.Preview"), ruleGroup);
    ruleButtons->addWidget(addRuleButton_);
    ruleButtons->addWidget(removeRuleButton_);
    ruleButtons->addStretch(1);
    ruleButtons->addWidget(previewButton_);
    ruleLayout->addLayout(ruleButtons);
    right->addWidget(ruleGroup);

    connect(addRuleButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onAddRule);
    connect(removeRuleButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onRemoveRule);
    connect(previewButton_, &QPushButton::clicked, this, &CustomizedCartoonDock::onPreview);

    auto* progressGroup = new QGroupBox(obs_module_text("CustomizedCartoon.Progress.Title"), rightPanel);
    auto* progressLayout = new QVBoxLayout(progressGroup);
    progressTable_ = new QTableWidget(progressGroup);
    progressTable_->setColumnCount(4);
    progressTable_->setHorizontalHeaderLabels(
        {obs_module_text("CustomizedCartoon.Progress.Col.EngageID"),
         obs_module_text("CustomizedCartoon.Progress.Col.Current"),
         obs_module_text("CustomizedCartoon.Progress.Col.Target"),
         obs_module_text("CustomizedCartoon.Progress.Col.Round")});
    progressTable_->horizontalHeader()->setStretchLastSection(true);
    progressTable_->setSelectionMode(QAbstractItemView::NoSelection);
    progressTable_->verticalHeader()->setVisible(false);
    progressLayout->addWidget(progressTable_);
    right->addWidget(progressGroup);

    mainLayout->addWidget(leftContainer, 0, Qt::AlignTop);
    mainLayout->addWidget(rightPanel, 0, Qt::AlignTop);

    setWidget(root);
}

void CustomizedCartoonDock::refreshUi() {
    loadFromConfig();
    refreshPositionUi();
    refreshProgress();
}

void CustomizedCartoonDock::refreshPositionUi() {
    if (!service_ || !orientationCombo_) {
        return;
    }
    const json cfg = service_->getConfigSnapshot();
    const bool landscape = orientationCombo_->currentIndex() == 0;
    const char* key = landscape ? "landscape" : "portrait";
    if (!cfg.contains("position") || !cfg["position"].is_object() || !cfg["position"].contains(key) ||
        !cfg["position"][key].is_object()) {
        posXSpin_->setValue(0.0);
        posYSpin_->setValue(0.0);
        scaleXSpin_->setValue(1.0);
        scaleYSpin_->setValue(1.0);
        rotSpin_->setValue(0.0);
        return;
    }

    const auto& t = cfg["position"][key];
    posXSpin_->setValue(t.value("x", 0.0));
    posYSpin_->setValue(t.value("y", 0.0));
    scaleXSpin_->setValue(t.value("scaleX", 1.0));
    scaleYSpin_->setValue(t.value("scaleY", 1.0));
    rotSpin_->setValue(t.value("rot", 0.0));
}

void CustomizedCartoonDock::loadFromConfig() {
    if (!service_) {
        return;
    }
    const json cfg = service_->getConfigSnapshot();

    std::unordered_map<QString, QString> mediaNameById;
    if (cfg.contains("media") && cfg["media"].is_array()) {
        for (const auto& it : cfg["media"]) {
            if (!it.is_object())
                continue;
            const QString id =
                it.contains("id") && it["id"].is_string()
                    ? QString::fromStdString(it["id"].get<std::string>())
                    : QString();
            const QString name =
                it.contains("name") && it["name"].is_string()
                    ? QString::fromStdString(it["name"].get<std::string>())
                    : QString();
            if (!id.isEmpty()) {
                mediaNameById[id] = name.isEmpty() ? id : name;
            }
        }
    }

    mediaList_->clear();
    int videoCount = 0;
    if (cfg.contains("media") && cfg["media"].is_array()) {
        const QIcon videoIcon(":/resources/video.svg");
        const QIcon trashIcon(":/resources/trash-red.svg");
        for (const auto& it : cfg["media"]) {
            if (!it.is_object())
                continue;
            const QString id =
                it.contains("id") && it["id"].is_string()
                    ? QString::fromStdString(it["id"].get<std::string>())
                    : QString();
            const QString name =
                it.contains("name") && it["name"].is_string()
                    ? QString::fromStdString(it["name"].get<std::string>())
                    : QString();
            const QString type =
                it.contains("type") && it["type"].is_string()
                    ? QString::fromStdString(it["type"].get<std::string>())
                    : QString();
            if (id.isEmpty()) {
                continue;
            }
            if (type == "video") {
                videoCount++;
            }

            auto* item = new QListWidgetItem(mediaList_);
            item->setData(Qt::UserRole, id);
            item->setSizeHint(QSize(0, 64));

            auto* row = new QFrame(mediaList_);
            row->setObjectName("mediaRow");
            row->setStyleSheet(
                "QFrame#mediaRow { background-color: rgba(255,255,255,0.06); border-radius: 10px; }"
                "QLabel { color: #DDE1E8; font-size: 16px; }"
                "QPushButton { border: none; background: transparent; }");

            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(14, 10, 14, 10);
            rowLayout->setSpacing(12);

            auto* iconLabel = new QLabel(row);
            iconLabel->setPixmap(videoIcon.pixmap(30, 24));
            iconLabel->setFixedSize(30, 24);

            auto* nameLabel = new QLabel(name, row);
            nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);

            auto* delButton = new QPushButton(row);
            delButton->setIcon(trashIcon);
            delButton->setIconSize(QSize(24, 24));
            delButton->setFixedSize(40, 40);
            delButton->setCursor(Qt::PointingHandCursor);

            connect(delButton, &QPushButton::clicked, this, [this, id]() {
                if (!service_) {
                    return;
                }
                QString error;
                if (!service_->deleteMedia(id, error)) {
                    QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                                         QMessageBox::Ok);
                }
                refreshUi();
            });

            rowLayout->addWidget(iconLabel);
            rowLayout->addWidget(nameLabel, 1);
            rowLayout->addWidget(delButton);

            mediaList_->addItem(item);
            mediaList_->setItemWidget(item, row);
        }
    }

    if (mediaCountLabel_) {
        mediaCountLabel_->setText(
            QString(obs_module_text("CustomizedCartoon.Media.SelectedVideoCount")).arg(videoCount));
    }
    if (mediaList_->count() > 0 && mediaList_->currentRow() < 0) {
        mediaList_->setCurrentRow(0);
    }

    ruleTable_->setRowCount(0);
    if (cfg.contains("rules") && cfg["rules"].is_array()) {
        int row = 0;
        for (const auto& it : cfg["rules"]) {
            if (!it.is_object())
                continue;
            const QString id =
                it.contains("id") && it["id"].is_string()
                    ? QString::fromStdString(it["id"].get<std::string>())
                    : QString();
            const QString name =
                it.contains("name") && it["name"].is_string()
                    ? QString::fromStdString(it["name"].get<std::string>())
                    : QString();
            const QString engageType =
                it.contains("engageType") && it["engageType"].is_string()
                    ? QString::fromStdString(it["engageType"].get<std::string>())
                    : QString();
            const QString mediaId =
                it.contains("mediaId") && it["mediaId"].is_string()
                    ? QString::fromStdString(it["mediaId"].get<std::string>())
                    : QString();
            const int points = it.contains("points") && it["points"].is_number_integer()
                                   ? it["points"].get<int>()
                                   : 0;
            const int count = it.contains("count") && it["count"].is_number_integer()
                                  ? it["count"].get<int>()
                                  : 0;
            const bool repeatable =
                it.contains("repeatable") && it["repeatable"].is_boolean() ? it["repeatable"].get<bool>()
                                                                           : true;
            const bool enabled =
                it.contains("enabled") && it["enabled"].is_boolean() ? it["enabled"].get<bool>() : true;

            ruleTable_->insertRow(row);
            auto* nameItem = new QTableWidgetItem(name);
            nameItem->setData(Qt::UserRole, id);
            ruleTable_->setItem(row, 0, nameItem);
            const QString typeText =
                engageType == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE"
                    ? obs_module_text("CustomizedCartoon.Rules.Type.LuckyBag")
                    : obs_module_text("CustomizedCartoon.Rules.Type.Gift");
            ruleTable_->setItem(row, 1, new QTableWidgetItem(typeText));

            QString params;
            if (engageType == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE") {
                params = QString(obs_module_text("CustomizedCartoon.Rules.Params.LuckyBag"))
                             .arg(count);
            } else {
                params = QString(obs_module_text("CustomizedCartoon.Rules.Params.Gift"))
                             .arg(points)
                             .arg(count);
            }
            ruleTable_->setItem(row, 2, new QTableWidgetItem(params));
            ruleTable_->setItem(row, 3, new QTableWidgetItem(mediaNameById[mediaId]));
            ruleTable_->setItem(row, 4, new QTableWidgetItem(repeatable ? "Y" : "N"));
            ruleTable_->setItem(row, 5, new QTableWidgetItem(enabled ? "Y" : "N"));
            row++;
        }
    }
}

void CustomizedCartoonDock::refreshProgress() {
    if (!service_) {
        return;
    }
    const auto progress = service_->getProgressSnapshot();
    progressTable_->setRowCount(0);
    int row = 0;
    for (const auto& p : progress) {
        progressTable_->insertRow(row);
        progressTable_->setItem(row, 0, new QTableWidgetItem(p.engageID));
        progressTable_->setItem(row, 1, new QTableWidgetItem(QString::number(p.current)));
        progressTable_->setItem(row, 2, new QTableWidgetItem(QString::number(p.target)));
        progressTable_->setItem(row, 3, new QTableWidgetItem(QString::number(p.round)));
        row++;
    }
}

void CustomizedCartoonDock::onAddMedia() {
    if (!service_) {
        return;
    }

    const QString path =
        QFileDialog::getOpenFileName(this, obs_module_text("CustomizedCartoon.Media.ChooseVideo"),
                                                      QString(), QString());
    if (path.isEmpty()) {
        return;
    }

    QString mediaId;
    QString error;
    if (!service_->importMediaFile(path, mediaId, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
        return;
    }
    refreshUi();
}

void CustomizedCartoonDock::onRemoveMedia() {
    if (!service_) {
        return;
    }
    auto* item = mediaList_->currentItem();
    if (!item) {
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) {
        return;
    }
    QString error;
    if (!service_->deleteMedia(id, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
    }
    refreshUi();
}

void CustomizedCartoonDock::onAddRule() {
    if (!service_) {
        return;
    }

    json cfg = service_->getConfigSnapshot();
    if (!cfg.contains("rules") || !cfg["rules"].is_array()) {
        cfg["rules"] = json::array();
    }
    if (cfg["rules"].size() >= 5) {
        QMessageBox::information(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                                 obs_module_text("CustomizedCartoon.Rules.Limit"), QMessageBox::Ok);
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(obs_module_text("CustomizedCartoon.Rules.Add"));
    auto* layout = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout();

    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setText(
        QString(obs_module_text("CustomizedCartoon.Rules.DefaultName"))
            .arg((int)cfg["rules"].size() + 1));

    auto* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Type.Gift"), "GIFT_AMOUNT_MILESTONE");
    typeCombo->addItem(obs_module_text("CustomizedCartoon.Rules.Type.LuckyBag"),
                       "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE");

    auto* pointsSpin = new QSpinBox(&dlg);
    pointsSpin->setRange(0, 100000000);
    pointsSpin->setValue(1000);

    auto* countSpin = new QSpinBox(&dlg);
    countSpin->setRange(1, 1000000);
    countSpin->setValue(10);

    connect(typeCombo, &QComboBox::currentIndexChanged, &dlg, [pointsSpin, typeCombo](int) {
        const QString t = typeCombo->currentData().toString();
        const bool isLuckyBag = (t == "GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE");
        pointsSpin->setEnabled(!isLuckyBag);
        if (isLuckyBag) {
            pointsSpin->setValue(0);
        }
    });

    auto* mediaCombo = new QComboBox(&dlg);
    mediaCombo->addItem("", "");
    if (cfg.contains("media") && cfg["media"].is_array()) {
        for (const auto& m : cfg["media"]) {
            if (!m.is_object() || !m.contains("id") || !m["id"].is_string()) {
                continue;
            }
            const QString id = QString::fromStdString(m["id"].get<std::string>());
            const QString name = m.contains("name") && m["name"].is_string()
                                     ? QString::fromStdString(m["name"].get<std::string>())
                                     : id;
            mediaCombo->addItem(name, id);
        }
    }

    auto* repeatableCheck = new QCheckBox(&dlg);
    repeatableCheck->setChecked(true);
    auto* enabledCheck = new QCheckBox(&dlg);
    enabledCheck->setChecked(true);

    form->addRow(obs_module_text("CustomizedCartoon.Rules.Field.Name"), nameEdit);
    form->addRow(obs_module_text("CustomizedCartoon.Rules.Field.Type"), typeCombo);
    form->addRow(obs_module_text("CustomizedCartoon.Rules.Field.Points"), pointsSpin);
    form->addRow(obs_module_text("CustomizedCartoon.Rules.Field.Count"), countSpin);
    form->addRow(obs_module_text("CustomizedCartoon.Rules.Field.Media"), mediaCombo);
    form->addRow(obs_module_text("CustomizedCartoon.Rules.Field.Repeat"), repeatableCheck);
    form->addRow(obs_module_text("CustomizedCartoon.Rules.Field.Enabled"), enabledCheck);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString mediaId = mediaCombo->currentData().toString();
    if (mediaId.isEmpty()) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                             obs_module_text("CustomizedCartoon.Rules.MissingMedia"),
                             QMessageBox::Ok);
        return;
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cfg["rules"].push_back({{"id", id.toStdString()},
                            {"name", nameEdit->text().toStdString()},
                            {"mediaId", mediaId.toStdString()},
                            {"engageType", typeCombo->currentData().toString().toStdString()},
                            {"points", pointsSpin->value()},
                            {"count", countSpin->value()},
                            {"repeatable", repeatableCheck->isChecked()},
                            {"enabled", enabledCheck->isChecked()}});

    service_->saveConfig(cfg);
    refreshUi();
}

void CustomizedCartoonDock::onRemoveRule() {
    if (!service_) {
        return;
    }
    const int row = ruleTable_->currentRow();
    if (row < 0) {
        return;
    }
    auto* nameItem = ruleTable_->item(row, 0);
    if (!nameItem) {
        return;
    }
    const QString id = nameItem->data(Qt::UserRole).toString();
    if (id.isEmpty()) {
        return;
    }

    json cfg = service_->getConfigSnapshot();
    if (!cfg.contains("rules") || !cfg["rules"].is_array()) {
        return;
    }
    json newRules = json::array();
    for (const auto& r : cfg["rules"]) {
        if (!r.is_object() || !r.contains("id") || !r["id"].is_string()) {
            continue;
        }
        if (QString::fromStdString(r["id"].get<std::string>()) == id) {
            continue;
        }
        newRules.push_back(r);
    }
    cfg["rules"] = std::move(newRules);
    service_->saveConfig(cfg);
    refreshUi();
}

void CustomizedCartoonDock::onPreview() {
    if (!service_) {
        return;
    }
    service_->previewPlayAll();
}

void CustomizedCartoonDock::onOrientationChanged(int) {
    refreshPositionUi();
    if (service_) {
        const bool landscape = orientationCombo_ && orientationCombo_->currentIndex() == 0;
        service_->applyOverlayTransformForOrientation(landscape);
    }
}

void CustomizedCartoonDock::onApplyPosition() {
    if (!service_) {
        return;
    }
    json cfg = service_->getConfigSnapshot();
    if (!cfg.contains("position") || !cfg["position"].is_object()) {
        cfg["position"] = json::object();
    }
    const bool landscape = orientationCombo_ && orientationCombo_->currentIndex() == 0;
    const char* key = landscape ? "landscape" : "portrait";
    json t = cfg["position"].contains(key) && cfg["position"][key].is_object() ? cfg["position"][key]
                                                                               : json::object();
    t["x"] = posXSpin_ ? posXSpin_->value() : 0.0;
    t["y"] = posYSpin_ ? posYSpin_->value() : 0.0;
    t["scaleX"] = scaleXSpin_ ? scaleXSpin_->value() : 1.0;
    t["scaleY"] = scaleYSpin_ ? scaleYSpin_->value() : 1.0;
    t["rot"] = rotSpin_ ? rotSpin_->value() : 0.0;
    cfg["position"][key] = std::move(t);
    service_->saveConfig(cfg);
    service_->applyOverlayTransformForOrientation(landscape);
}

void CustomizedCartoonDock::onReadPositionFromCanvas() {
    if (!service_) {
        return;
    }
    json t;
    QString error;
    if (!service_->getCurrentOverlayTransform(t, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
        return;
    }
    posXSpin_->setValue(t.value("x", 0.0));
    posYSpin_->setValue(t.value("y", 0.0));
    scaleXSpin_->setValue(t.value("scaleX", 1.0));
    scaleYSpin_->setValue(t.value("scaleY", 1.0));
    rotSpin_->setValue(t.value("rot", 0.0));
}

void CustomizedCartoonDock::onStartPositionPreview() {
    if (!service_) {
        return;
    }
    auto* item = mediaList_ ? mediaList_->currentItem() : nullptr;
    if (!item) {
        QMessageBox::information(this, obs_module_text("CustomizedCartoon.Dock.Title"),
                                 obs_module_text("CustomizedCartoon.Position.NoMediaForPreview"),
                                 QMessageBox::Ok);
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) {
        return;
    }
    QString error;
    const bool landscape = orientationCombo_ && orientationCombo_->currentIndex() == 0;
    if (!service_->startPositionPreview(id, landscape, error)) {
        QMessageBox::warning(this, obs_module_text("CustomizedCartoon.Dock.Title"), error,
                             QMessageBox::Ok);
        return;
    }
}

void CustomizedCartoonDock::onStopPositionPreview() {
    if (service_) {
        service_->stopPositionPreview();
    }
}
