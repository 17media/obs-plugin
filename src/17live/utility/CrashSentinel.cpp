#include "CrashSentinel.hpp"

#include <obs-module.h>

#include <QDir>
#include <QUuid>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace seventeen {
    namespace utility {
        namespace {
            constexpr std::string_view kSentinelSubdir = ".17Live/.sentinel";
            constexpr std::string_view kSentinelPrefix = "run_";

            std::mutex g_mutex;
            bool g_initialized = false;
            bool g_previousRunClean = true;
            std::filesystem::path g_sentinelDir;
            std::filesystem::path g_currentSentinelFile;

            std::filesystem::path getSentinelDirPath() {
                const std::string homeDir = QDir::homePath().toStdString();
                return std::filesystem::u8path(homeDir) / std::filesystem::u8path(kSentinelSubdir);
            }

            bool hasAnySentinelFile(const std::filesystem::path& dir) {
                try {
                    if (!std::filesystem::exists(dir)) {
                        return false;
                    }
                    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                        if (!entry.is_regular_file()) {
                            continue;
                        }
                        const std::string name = entry.path().filename().u8string();
                        if (name.rfind(kSentinelPrefix.data(), 0) == 0) {
                            return true;
                        }
                    }
                } catch (...) {
                }
                return false;
            }

            void ensureDirExists(const std::filesystem::path& dir) {
                try {
                    if (!std::filesystem::exists(dir)) {
                        std::filesystem::create_directories(dir);
                    }
                } catch (...) {
                }
            }

            void createSentinelFile(const std::filesystem::path& path) {
                try {
                    std::fstream f;
                    f.open(path, std::ios::out);
                    f.close();
                } catch (...) {
                }
            }

            void removeAllSentinels(const std::filesystem::path& dir) {
                try {
                    if (!std::filesystem::exists(dir)) {
                        return;
                    }
                    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                        if (!entry.is_regular_file()) {
                            continue;
                        }
                        const std::string name = entry.path().filename().u8string();
                        if (name.rfind(kSentinelPrefix.data(), 0) != 0) {
                            continue;
                        }
                        try {
                            std::filesystem::remove(entry.path());
                        } catch (...) {
                        }
                    }
                } catch (...) {
                }
            }
        }  // namespace

        void CrashSentinel::Initialize() {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_initialized) {
                return;
            }

            g_sentinelDir = getSentinelDirPath();
            ensureDirExists(g_sentinelDir);

            const bool hadOldSentinel = hasAnySentinelFile(g_sentinelDir);
            g_previousRunClean = !hadOldSentinel;

            const std::string uuid =
                QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            g_currentSentinelFile =
                g_sentinelDir / std::filesystem::u8path(std::string(kSentinelPrefix) + uuid);
            createSentinelFile(g_currentSentinelFile);

            g_initialized = true;
        }

        bool CrashSentinel::PreviousRunClean() {
            std::lock_guard<std::mutex> lock(g_mutex);
            return g_previousRunClean;
        }

        void CrashSentinel::Shutdown() {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_initialized) {
                return;
            }
            removeAllSentinels(g_sentinelDir);
            g_initialized = false;
        }
    }  // namespace utility
}  // namespace seventeen

