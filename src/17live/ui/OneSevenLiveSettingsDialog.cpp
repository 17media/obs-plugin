#include "OneSevenLiveSettingsDialog.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "../OneSevenLiveConfigManager.hpp"

#include "moc_OneSevenLiveSettingsDialog.cpp"

namespace {
    constexpr const char* kConfigKeyObsAutoAdjustDontRemind = "ObsAutoAdjustDontRemind";
}

OneSevenLiveSettingsDialog::OneSevenLiveSettingsDialog(QWidget* parent,
                                                       OneSevenLiveConfigManager* configManager)
    : QDialog(parent), configManager_(configManager) {
    setupUi();
    loadValues();
    updateApplyState();
}

OneSevenLiveSettingsDialog::~OneSevenLiveSettingsDialog() = default;

void OneSevenLiveSettingsDialog::setupUi() {
    setWindowTitle(obs_module_text("Settings.Title"));
    setModal(true);
    resize(981, 730);
    setMinimumSize(700, 512);
    setSizeGripEnabled(true);

    auto* rootLayout = new QVBoxLayout(this);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(0);

    categoryList_ = new QListWidget(this);
    categoryList_->setMaximumWidth(180);
    categoryList_->setIconSize(QSize(16, 16));
    categoryList_->setSpacing(1);
    categoryList_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    categoryList_->setSelectionMode(QAbstractItemView::SingleSelection);
    categoryList_->addItem(obs_module_text("Settings.Category.Streaming"));
    contentLayout->addWidget(categoryList_, 0);

    stackedWidget_ = new QStackedWidget(this);

    auto* streamingPage = new QWidget(stackedWidget_);
    auto* streamingPageLayout = new QVBoxLayout(streamingPage);
    streamingPageLayout->setContentsMargins(9, 0, 0, 0);

    auto* scrollArea = new QScrollArea(streamingPage);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);

    auto* scrollContent = new QWidget(scrollArea);
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox(obs_module_text("Settings.Streaming.GroupTitle"), scrollContent);
    auto* groupLayout = new QVBoxLayout(group);

    autoAdjustDontRemindCheck_ =
        new QCheckBox(obs_module_text("Settings.Streaming.AutoAdjustDontRemind"), group);
    groupLayout->addWidget(autoAdjustDontRemindCheck_);

    scrollLayout->addWidget(group);
    scrollLayout->addStretch(1);

    scrollArea->setWidget(scrollContent);
    streamingPageLayout->addWidget(scrollArea);

    stackedWidget_->addWidget(streamingPage);

    contentLayout->addWidget(stackedWidget_, 1);
    rootLayout->addLayout(contentLayout, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    okButton_ = buttons->button(QDialogButtonBox::Ok);
    cancelButton_ = buttons->button(QDialogButtonBox::Cancel);
    applyButton_ = buttons->button(QDialogButtonBox::Apply);
    rootLayout->addWidget(buttons);

    connect(categoryList_, &QListWidget::currentRowChanged, stackedWidget_,
            &QStackedWidget::setCurrentIndex);
    categoryList_->setCurrentRow(0);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applyValues();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (applyButton_) {
        connect(applyButton_, &QPushButton::clicked, this, [this]() { applyValues(); });
    }

    connect(autoAdjustDontRemindCheck_, &QCheckBox::toggled, this,
            [this](bool) { updateApplyState(); });

    const bool enabled = configManager_ != nullptr;
    autoAdjustDontRemindCheck_->setEnabled(enabled);
    if (applyButton_) {
        applyButton_->setEnabled(false);
    }
}

void OneSevenLiveSettingsDialog::loadValues() {
    if (!configManager_) {
        initialAutoAdjustDontRemind_ = false;
        autoAdjustDontRemindCheck_->setChecked(false);
        return;
    }

    initialAutoAdjustDontRemind_ =
        configManager_->getBoolValue(kConfigKeyObsAutoAdjustDontRemind, false);
    autoAdjustDontRemindCheck_->setChecked(initialAutoAdjustDontRemind_);
}

void OneSevenLiveSettingsDialog::applyValues() {
    if (!configManager_) {
        return;
    }

    const bool dontRemind = autoAdjustDontRemindCheck_->isChecked();
    configManager_->setBoolValue(kConfigKeyObsAutoAdjustDontRemind, dontRemind);

    initialAutoAdjustDontRemind_ = dontRemind;
    updateApplyState();
}

void OneSevenLiveSettingsDialog::updateApplyState() {
    if (!applyButton_ || !autoAdjustDontRemindCheck_) {
        return;
    }

    const bool changed = autoAdjustDontRemindCheck_->isChecked() != initialAutoAdjustDontRemind_;
    applyButton_->setEnabled(configManager_ != nullptr && changed);
}
