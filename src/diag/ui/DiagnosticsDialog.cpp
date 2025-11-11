#include "DiagnosticsDialog.hpp"
#include "../IDiagnosticsCollector.hpp"
#include "../DiagnosticsCollectorFactory.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <obs-module.h>

namespace seventeen {
namespace diag {
namespace ui {

DiagnosticsDialog::DiagnosticsDialog(QWidget* parent)
    : QDialog(parent)
    , m_workerThread(nullptr) {
    setupUI();
    
    // Create worker thread
    m_workerThread = new QThread(this);
    m_worker = new DiagnosticsWorker();
    m_worker->moveToThread(m_workerThread);
    
    // Connect signals
    connect(this, &DiagnosticsDialog::startCollection, m_worker, &DiagnosticsWorker::performCollection);
    connect(m_worker, &DiagnosticsWorker::collectionCompleted, this, &DiagnosticsDialog::onCollectionCompleted);
    connect(m_worker, &DiagnosticsWorker::progressUpdate, this, &DiagnosticsDialog::onProgressUpdate);
    connect(m_worker, &DiagnosticsWorker::error, this, &DiagnosticsDialog::onCollectionError);
    
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    
    m_workerThread->start();
    
    // Set default output path
    m_outputPath = getDefaultOutputPath();
    m_outputPathLabel->setText(m_outputPath);
    
    // Set default state
    m_privacyFilterCheckBox->setChecked(true);
    updateCategories();
}

DiagnosticsDialog::~DiagnosticsDialog() {
    if (m_workerThread) {
        // Stop thread and ensure queued deletes run
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
    }
    // Worker will be deleted by deleteLater when the thread finishes
    m_worker = nullptr;
}

void DiagnosticsDialog::setupUI() {
    setWindowTitle(obs_module_text("Diagnostics.Title"));
    setModal(true);
    resize(600, 500);
    
    auto* mainLayout = new QVBoxLayout(this);
    
    // Information label
    auto* infoLabel = new QLabel(obs_module_text("Diagnostics.Description"), this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { padding: 10px; background-color: #000000; color: white; border-radius: 5px; }");
    mainLayout->addWidget(infoLabel);
    
    // Categories group
    auto* categoriesGroup = new QGroupBox(obs_module_text("Diagnostics.Categories.Title"), this);
    auto* categoriesLayout = new QVBoxLayout(categoriesGroup);
    
    m_obsLogsCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.OBSLogs"), this);
    m_pluginLogsCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.PluginLogs"), this);
    m_networkLogsCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.NetworkLogs"), this);
    m_systemInfoCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.SystemInfo"), this);
    m_crashInfoCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.CrashInfo"), this);
    m_configSnapshotCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.ConfigSnapshot"), this);
    m_networkRequestsCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.NetworkRequests"), this);
    m_privacyFilterCheckBox = new QCheckBox(obs_module_text("Diagnostics.Categories.PrivacyFilter"), this);
    
    categoriesLayout->addWidget(m_obsLogsCheckBox);
    categoriesLayout->addWidget(m_pluginLogsCheckBox);
    categoriesLayout->addWidget(m_networkLogsCheckBox);
    categoriesLayout->addWidget(m_systemInfoCheckBox);
    categoriesLayout->addWidget(m_crashInfoCheckBox);
    categoriesLayout->addWidget(m_configSnapshotCheckBox);
    categoriesLayout->addWidget(m_networkRequestsCheckBox);
    categoriesLayout->addWidget(m_privacyFilterCheckBox);
    
    // 默认启用且选中四个分类：OBS 日志、插件日志、崩溃信息、配置快照
    m_obsLogsCheckBox->setChecked(true);
    m_pluginLogsCheckBox->setChecked(true);
    m_crashInfoCheckBox->setChecked(true);
    m_configSnapshotCheckBox->setChecked(true);

    // 隐藏暂不启用的分类
    m_networkLogsCheckBox->setVisible(false);
    m_systemInfoCheckBox->setVisible(false);
    m_networkRequestsCheckBox->setVisible(false);

    mainLayout->addWidget(categoriesGroup);
    
    // Output path selection
    auto* outputLayout = new QHBoxLayout();
    auto* outputLabel = new QLabel(obs_module_text("Diagnostics.OutputPath"), this);
    m_outputPathLabel = new QLabel(this);
    m_browseButton = new QPushButton(obs_module_text("Diagnostics.Browse"), this);
    
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(m_outputPathLabel, 1);
    outputLayout->addWidget(m_browseButton);
    
    mainLayout->addLayout(outputLayout);
    
    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    // Status text
    m_statusTextEdit = new QTextEdit(this);
    m_statusTextEdit->setReadOnly(true);
    m_statusTextEdit->setMaximumHeight(100);
    m_statusTextEdit->setPlainText(obs_module_text("Diagnostics.Status.Ready"));
    mainLayout->addWidget(m_statusTextEdit);
    
    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    m_collectButton = new QPushButton(obs_module_text("Diagnostics.Collect"), this);
    m_cancelButton = new QPushButton(obs_module_text("Diagnostics.Cancel"), this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_collectButton);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connect button signals
    connect(m_collectButton, &QPushButton::clicked, this, &DiagnosticsDialog::onCollectClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_browseButton, &QPushButton::clicked, this, &DiagnosticsDialog::onBrowseClicked);
    
    // Connect checkbox changes
    auto updateCategoriesSlot = [this]() { updateCategories(); };
    connect(m_obsLogsCheckBox, &QCheckBox::toggled, updateCategoriesSlot);
    connect(m_pluginLogsCheckBox, &QCheckBox::toggled, updateCategoriesSlot);
    connect(m_networkLogsCheckBox, &QCheckBox::toggled, updateCategoriesSlot);
    connect(m_systemInfoCheckBox, &QCheckBox::toggled, updateCategoriesSlot);
    connect(m_crashInfoCheckBox, &QCheckBox::toggled, updateCategoriesSlot);
    connect(m_configSnapshotCheckBox, &QCheckBox::toggled, updateCategoriesSlot);
    connect(m_networkRequestsCheckBox, &QCheckBox::toggled, updateCategoriesSlot);
}

void DiagnosticsDialog::onCollectClicked() {
    showPrivacyDialog();
    
    m_collectButton->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    DiagnosticConfig config = getCurrentConfig();
    config.outputDirectory = m_outputPath.toStdString();
    
    emit startCollection(config);
}

void DiagnosticsDialog::onBrowseClicked() {
    QString defaultPath = getDefaultOutputPath();
    QString fileName = QFileDialog::getSaveFileName(this, 
        obs_module_text("Diagnostics.SavePackage"), defaultPath, "ZIP Files (*.zip)");
    
    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".zip", Qt::CaseInsensitive)) {
            fileName += ".zip";
        }
        m_outputPath = fileName;
        m_outputPathLabel->setText(m_outputPath);
    }
}

void DiagnosticsDialog::onCollectionCompleted(const CollectResult& result) {
    m_collectButton->setEnabled(true);
    m_progressBar->setVisible(false);
    
    if (result.status == CollectStatus::SUCCESS) {
        m_statusTextEdit->setPlainText(QString("%1\n%2: %3\n%4: %5")
            .arg(obs_module_text("Diagnostics.Status.Success"))
            .arg(obs_module_text("Diagnostics.Status.Output"))
            .arg(QString::fromStdString(result.outputPath))
            .arg(obs_module_text("Diagnostics.Status.FilesCollected"))
            .arg(result.collectedFiles.size()));
        
        QMessageBox::information(this, obs_module_text("Diagnostics.Status.SuccessTitle"), 
            QString("%1\n%2\n\n%3")
            .arg(obs_module_text("Diagnostics.Status.SuccessMessage"))
            .arg(QString::fromStdString(result.outputPath))
            .arg(obs_module_text("Diagnostics.Status.OpenFolderQuestion")),
            QMessageBox::Yes | QMessageBox::No);
        
        if (QMessageBox::question(this, obs_module_text("Diagnostics.Status.OpenFolderTitle"), 
                obs_module_text("Diagnostics.Status.OpenFolderQuestion")) == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                QFileInfo(QString::fromStdString(result.outputPath)).absolutePath()));
        }
        
        accept();
    } else {
        m_statusTextEdit->setPlainText(QString("%1: %2")
            .arg(obs_module_text("Diagnostics.Status.Failed"))
            .arg(QString::fromStdString(result.message)));
        
        QMessageBox::critical(this, obs_module_text("Diagnostics.Status.ErrorTitle"), 
            QString("%1: %2")
            .arg(obs_module_text("Diagnostics.Status.ErrorMessage"))
            .arg(QString::fromStdString(result.message)));
    }
}

void DiagnosticsDialog::onProgressUpdate(const QString& stage, double progress) {
    m_statusTextEdit->setPlainText(QString("%1: %2").arg(obs_module_text("Diagnostics.Status.Collecting")).arg(stage));
    m_progressBar->setValue(static_cast<int>(progress * 100));
}

void DiagnosticsDialog::onCollectionError(const QString& error) {
    m_statusTextEdit->setPlainText(QString("%1: %2").arg(obs_module_text("Diagnostics.Status.Error")).arg(error));
    m_collectButton->setEnabled(true);
    m_progressBar->setVisible(false);
}

void DiagnosticsDialog::updateCategories() {
    // Update UI based on selections
    bool hasSelection = m_obsLogsCheckBox->isChecked() ||
                        m_pluginLogsCheckBox->isChecked() ||
                        m_crashInfoCheckBox->isChecked() ||
                        m_configSnapshotCheckBox->isChecked();
    
    m_collectButton->setEnabled(hasSelection);
}

DiagnosticConfig DiagnosticsDialog::getCurrentConfig() const {
    DiagnosticConfig config;
    config.enablePrivacyFilter = m_privacyFilterCheckBox->isChecked();
    config.includeSensitiveData = false;
    
    if (m_obsLogsCheckBox->isChecked()) {
        config.categories.push_back(DiagnosticCategory::OBS_LOGS);
    }
    if (m_pluginLogsCheckBox->isChecked()) {
        config.categories.push_back(DiagnosticCategory::PLUGIN_LOGS);
    }
    // 暂不导出网络日志与系统信息
    if (m_crashInfoCheckBox->isChecked()) {
        config.categories.push_back(DiagnosticCategory::CRASH_INFO);
    }
    if (m_configSnapshotCheckBox->isChecked()) {
        config.categories.push_back(DiagnosticCategory::CONFIG_SNAPSHOT);
    }
    // 暂不导出网络请求
    
    return config;
}

QString DiagnosticsDialog::getDefaultOutputPath() const {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return desktopPath + "/17live_diagnostics_" + timestamp + ".zip";
}

void DiagnosticsDialog::showPrivacyDialog() {
    if (!m_privacyFilterCheckBox->isChecked()) {
        int result = QMessageBox::warning(this, obs_module_text("Diagnostics.Privacy.WarningTitle"),
            obs_module_text("Diagnostics.Privacy.WarningMessage"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        
        if (result == QMessageBox::No) {
            m_privacyFilterCheckBox->setChecked(true);
        }
    }
}

// DiagnosticsWorker implementation

DiagnosticsWorker::DiagnosticsWorker(QObject* parent)
    : QObject(parent)
    , m_collector(createDiagnosticsCollector()) {
}

DiagnosticsWorker::~DiagnosticsWorker() = default;

void DiagnosticsWorker::performCollection(const DiagnosticConfig& config) {
    if (!m_collector) {
        emit error(obs_module_text("Diagnostics.Status.CollectorFailed"));
        return;
    }
    
    m_collector->setProgressCallback([this](const std::string& stage, double progress) {
        emit progressUpdate(QString::fromStdString(stage), progress);
    });
    
    CollectResult result = m_collector->collect(config);
    
    if (result.status == CollectStatus::ERROR) {
        emit error(QString::fromStdString(result.message));
    } else {
        emit collectionCompleted(result);
    }
}

} // namespace ui
} // namespace diag
} // namespace seventeen

#include "moc_DiagnosticsDialog.cpp"
