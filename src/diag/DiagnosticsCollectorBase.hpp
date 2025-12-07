#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

#include "IDiagnosticsCollector.hpp"
#include "PrivacyFilter.hpp"

namespace seventeen {
    namespace diag {

        class DiagnosticsCollectorBase : public IDiagnosticsCollector {
           public:
            DiagnosticsCollectorBase();
            virtual ~DiagnosticsCollectorBase() = default;

            CollectResult collect(const DiagnosticConfig& config) override;

            std::vector<DiagnosticCategory> getAvailableCategories() const override;

            void setProgressCallback(ProgressCallback callback) override {
                m_progressCallback = callback;
            }

            std::string getLastError() const override {
                return m_lastError;
            }

           protected:
            virtual std::string getPlatformName() const = 0;
            std::string getSystemInfo() const override;
            virtual std::string getSystemInfoImpl() const = 0;
            virtual std::vector<std::string> collectOBSLogs() = 0;
            virtual std::vector<std::string> collectPluginLogs() = 0;
            virtual std::vector<std::string> collectNetworkLogs() = 0;
            virtual std::vector<std::string> collectCrashInfo() = 0;
            virtual std::vector<std::string> collectConfigSnapshot() = 0;
            virtual std::vector<std::string> collectNetworkRequests() = 0;
            virtual bool createZipArchive(const std::string& outputPath,
                                          const std::vector<std::string>& files) = 0;

            void reportProgress(const std::string& stage, double progress);
            void reportSubProgress(const std::string& detail, double subProgress);

            void setLastError(const std::string& error) {
                m_lastError = error;
            }

            std::string generateTempDirectory();
            std::string sanitizeFileName(const std::string& filename);
            bool writeToFile(const std::string& path, const std::string& content);
            std::string writeSystemInfoToFile();
            bool copyWithSizeLimit(const std::string& source, const std::string& destination,
                                   std::uintmax_t maxBytes = 1024 * 1024);

            std::unique_ptr<PrivacyFilter> m_privacyFilter;

           private:
            ProgressCallback m_progressCallback;
            std::string m_lastError;
            
            double m_currentBaseProgress = 0.0;
            double m_currentStageScale = 1.0;

            std::vector<std::string> collectCategory(DiagnosticCategory category);
            bool applyPrivacyFilter(const std::string& filePath);
        };

    }  // namespace diag
}  // namespace seventeen
