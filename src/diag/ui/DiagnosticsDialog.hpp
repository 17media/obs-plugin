#pragma once

#include <QDialog>
#include <QThread>
#include <memory>

#include "../IDiagnosticsCollector.hpp"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QProgressBar;
class QTextEdit;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

namespace seventeen {
    namespace diag {
        namespace ui {

            class DiagnosticsWorker;

            class DiagnosticsDialog : public QDialog {
                Q_OBJECT

               public:
                explicit DiagnosticsDialog(QWidget* parent = nullptr);
                ~DiagnosticsDialog();

               signals:
                void startCollection(const DiagnosticConfig& config);

               private slots:
                void onCollectClicked();
                void onBrowseClicked();
                void onCollectionCompleted(const CollectResult& result);
                void onProgressUpdate(const QString& stage, double progress);
                void onCollectionError(const QString& error);

               private:
                void setupUI();
                void updateCategories();
                DiagnosticConfig getCurrentConfig() const;
                QString getDefaultOutputPath() const;
                void showPrivacyDialog();

                // UI Elements
                QCheckBox* m_obsLogsCheckBox = nullptr;
                QCheckBox* m_pluginLogsCheckBox = nullptr;
                QCheckBox* m_networkLogsCheckBox = nullptr;
                QCheckBox* m_systemInfoCheckBox = nullptr;
                QCheckBox* m_crashInfoCheckBox = nullptr;
                QCheckBox* m_configSnapshotCheckBox = nullptr;
                QCheckBox* m_networkRequestsCheckBox = nullptr;
                QCheckBox* m_privacyFilterCheckBox = nullptr;

                QProgressBar* m_progressBar = nullptr;
                QTextEdit* m_statusTextEdit = nullptr;
                QPushButton* m_collectButton = nullptr;
                QPushButton* m_cancelButton = nullptr;
                QPushButton* m_browseButton = nullptr;
                QLabel* m_outputPathLabel = nullptr;

                QString m_outputPath;
                DiagnosticsWorker* m_worker = nullptr;
                QThread* m_workerThread = nullptr;
            };

            class DiagnosticsWorker : public QObject {
                Q_OBJECT

               public:
                explicit DiagnosticsWorker(QObject* parent = nullptr);
                ~DiagnosticsWorker();

               public slots:
                void performCollection(const DiagnosticConfig& config);

               signals:
                void collectionCompleted(const CollectResult& result);
                void progressUpdate(const QString& stage, double progress);
                void error(const QString& message);

               private:
                std::unique_ptr<IDiagnosticsCollector> m_collector;
            };

        }  // namespace ui
    }  // namespace diag
}  // namespace seventeen
