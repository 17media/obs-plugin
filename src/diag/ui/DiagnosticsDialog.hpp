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
                QCheckBox* m_obsLogsCheckBox;
                QCheckBox* m_pluginLogsCheckBox;
                QCheckBox* m_networkLogsCheckBox;
                QCheckBox* m_systemInfoCheckBox;
                QCheckBox* m_crashInfoCheckBox;
                QCheckBox* m_configSnapshotCheckBox;
                QCheckBox* m_networkRequestsCheckBox;
                QCheckBox* m_privacyFilterCheckBox;

                QProgressBar* m_progressBar;
                QTextEdit* m_statusTextEdit;
                QPushButton* m_collectButton;
                QPushButton* m_cancelButton;
                QPushButton* m_browseButton;
                QLabel* m_outputPathLabel;

                QString m_outputPath;
                DiagnosticsWorker* m_worker;
                QThread* m_workerThread;
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
