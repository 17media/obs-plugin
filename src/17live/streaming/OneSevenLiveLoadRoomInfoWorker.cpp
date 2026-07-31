#include "OneSevenLiveLoadRoomInfoWorker.hpp"

#include <obs-module.h>

#include "OneSevenLiveConfigManager.hpp"
#include "api/OneSevenLiveApiWrappers.hpp"
#include "plugin-support.h"
#include "utility/Common.hpp"

OneSevenLiveLoadRoomInfoWorker::OneSevenLiveLoadRoomInfoWorker(
    OneSevenLiveApiWrappers* apiWrapper, OneSevenLiveConfigManager* configManager)
    : m_apiWrapper(apiWrapper),
      m_configManager(configManager),
      m_roomInfo(nullptr),
      m_configStreamer(nullptr),
      m_userInfo(nullptr),
      m_levels(nullptr) {
    if (!m_apiWrapper) {
        obs_log(LOG_ERROR, "OneSevenLiveLoadRoomInfoWorker: apiWrapper is null");
    }

    if (!m_configManager) {
        obs_log(LOG_ERROR, "OneSevenLiveLoadRoomInfoWorker: configManager is null");
    }
}

void OneSevenLiveLoadRoomInfoWorker::setDataStructures(OneSevenLiveRoomInfo* roomInfo,
                                                       OneSevenLiveConfigStreamer* configStreamer,
                                                       OneSevenLiveUserInfo* userInfo,
                                                       OneSevenLiveArmySubscriptionLevels* levels) {
    m_roomInfo = roomInfo;
    m_configStreamer = configStreamer;
    m_userInfo = userInfo;
    m_levels = levels;
}

OneSevenLiveLoadRoomInfoWorker::LoadResult OneSevenLiveLoadRoomInfoWorker::loadRoomInfo(
    qint64 roomID) {
    obs_log(LOG_INFO,
            "OneSevenLiveLoadRoomInfoWorker: Starting to load room info for room ID: %lld", roomID);

    LoadResult result;

    // Validate prerequisites
    if (!m_apiWrapper) {
        result.errorMessage = "API wrapper is not available";
        return result;
    }

    if (!m_configManager) {
        result.errorMessage = "Config manager is not available";
        return result;
    }

    if (!m_roomInfo || !m_configStreamer || !m_userInfo || !m_levels) {
        result.errorMessage = "Data structures are not properly initialized";
        return result;
    }

    if (roomID <= 0) {
        result.errorMessage = "Invalid room ID provided";
        return result;
    }

    try {
        // Step 1: Get configuration values
        std::string region, language, userID;
        if (!getConfigurationValues(region, language, userID)) {
            result.errorMessage = "Failed to retrieve configuration values";
            return result;
        }

        obs_log(
            LOG_INFO,
            "OneSevenLiveLoadRoomInfoWorker: Config values - Region: %s, Language: %s, UserID: %s",
            region.c_str(), language.c_str(), userID.c_str());

        // Step 2: Load room information
        result.roomInfoSuccess = m_apiWrapper->GetRoomInfo(roomID, *m_roomInfo);
        if (!result.roomInfoSuccess) {
            result.roomInfoError = m_apiWrapper->getLastError();
            obs_log(LOG_WARNING, "OneSevenLiveLoadRoomInfoWorker: Failed to get room info");
        }

        // if room info failed, set default values
        // landscape: false
        // streamerType: 0
        // archiveConfig
        // autoRecording: true
        // autoPublish: false
        // clipPermission: 0
        if (!result.roomInfoSuccess) {
            obs_log(LOG_WARNING,
                    "OneSevenLiveLoadRoomInfoWorker: GetRoomInfo failed, setting default values");
            m_roomInfo->landscape = false;
            m_roomInfo->streamerType = 0;
            m_roomInfo->archiveConfig = OneSevenLiveArchiveConfig();
            m_roomInfo->archiveConfig.autoRecording = true;
            m_roomInfo->archiveConfig.autoPublish = false;
            m_roomInfo->archiveConfig.clipPermission = 0;
            m_roomInfo->status = 0;
            result.roomInfoSuccess = true;
            result.roomInfoError.reset();
        }

        // Step 3: Load config streamer information
        result.configStreamerSuccess = m_apiWrapper->GetConfigStreamer(region, language, *m_configStreamer);
        if (!result.configStreamerSuccess) {
            result.configStreamerError = m_apiWrapper->getLastError();
            obs_log(LOG_WARNING, "OneSevenLiveLoadRoomInfoWorker: Failed to get config streamer");
        }

        // Step 4: Load user information
        result.userInfoSuccess = m_apiWrapper->GetUserInfo(userID, region, language, *m_userInfo);
        if (!result.userInfoSuccess) {
            result.userInfoError = m_apiWrapper->getLastError();
            obs_log(LOG_WARNING, "OneSevenLiveLoadRoomInfoWorker: Failed to get user info");
        }

        // Step 5: Load army subscription levels
        result.levelsSuccess = m_apiWrapper->GetArmySubscriptionLevels(region, language, *m_levels);
        if (!result.levelsSuccess) {
            result.levelsError = m_apiWrapper->getLastError();
            obs_log(LOG_WARNING,
                    "OneSevenLiveLoadRoomInfoWorker: Failed to get army subscription levels");
        }

        // Step 6: Validate loaded data
        if (!validateLoadedData(result)) {
            obs_log(LOG_WARNING, "OneSevenLiveLoadRoomInfoWorker: Data validation failed");
        }

        // Step 7: Generate error message if needed
        if (result.hasFailures()) {
            result.errorMessage = generateErrorMessage(result);
        }

        obs_log(LOG_INFO,
                "OneSevenLiveLoadRoomInfoWorker: Loading completed - Required data: %s, Optional "
                "data: %s",
                result.hasRequiredData() ? "OK" : "FAILED",
                result.hasOptionalData() ? "OK" : "PARTIAL");

    } catch (const std::exception& e) {
        result.errorMessage = std::string("Critical error during loading: ") + e.what();
        obs_log(LOG_ERROR, "OneSevenLiveLoadRoomInfoWorker: %s", result.errorMessage.c_str());
    } catch (...) {
        result.errorMessage = "Unknown critical error during loading";
        obs_log(LOG_ERROR, "OneSevenLiveLoadRoomInfoWorker: %s", result.errorMessage.c_str());
    }

    return result;
}

bool OneSevenLiveLoadRoomInfoWorker::getConfigurationValues(std::string& region,
                                                            std::string& language,
                                                            std::string& userID) {
    try {
        // Get region
        if (!m_configManager->getConfigValue("Region", region)) {
            obs_log(LOG_ERROR, "OneSevenLiveLoadRoomInfoWorker: Failed to get Region from config");
            return false;
        }

        // Get language
        language = GetCurrentLanguage();
        if (language.empty()) {
            obs_log(LOG_WARNING,
                    "OneSevenLiveLoadRoomInfoWorker: Language is empty, using default");
            language = "en-US";  // Default fallback
        }

        // Get user ID
        if (!m_configManager->getConfigValue("UserID", userID)) {
            obs_log(LOG_ERROR, "OneSevenLiveLoadRoomInfoWorker: Failed to get UserID from config");
            return false;
        }

        if (userID.empty()) {
            obs_log(LOG_ERROR, "OneSevenLiveLoadRoomInfoWorker: UserID is empty");
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        obs_log(LOG_ERROR,
                "OneSevenLiveLoadRoomInfoWorker: Exception in getConfigurationValues: %s",
                e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR,
                "OneSevenLiveLoadRoomInfoWorker: Unknown exception in getConfigurationValues");
        return false;
    }
}

bool OneSevenLiveLoadRoomInfoWorker::validateLoadedData(const LoadResult& result) {
    bool isValid = true;

    // Validate room info if it was loaded successfully
    if (result.roomInfoSuccess && m_roomInfo) {
        if (m_roomInfo->liveStreamID <= 0) {
            obs_log(LOG_WARNING,
                    "OneSevenLiveLoadRoomInfoWorker: Invalid liveStreamID in room info");
            isValid = false;
        }
    }

    // Validate config streamer if it was loaded successfully
    if (result.configStreamerSuccess && m_configStreamer) {
        if (m_configStreamer->subtabs.empty()) {
            obs_log(LOG_WARNING, "OneSevenLiveLoadRoomInfoWorker: No subtabs in config streamer");
            isValid = false;
        }
    }

    // Validate user info if it was loaded successfully
    if (result.userInfoSuccess && m_userInfo) {
        if (m_userInfo->userID.isEmpty()) {
            obs_log(LOG_WARNING, "OneSevenLiveLoadRoomInfoWorker: Empty userID in user info");
            isValid = false;
        }
    }

    return isValid;
}

std::string OneSevenLiveLoadRoomInfoWorker::generateErrorMessage(const LoadResult& result) {
    std::vector<std::string> failedOperations;

    if (!result.roomInfoSuccess) {
        failedOperations.push_back("room information");
    }

    if (!result.configStreamerSuccess) {
        failedOperations.push_back("streamer configuration");
    }

    if (!result.userInfoSuccess) {
        failedOperations.push_back("user information");
    }

    if (!result.levelsSuccess) {
        failedOperations.push_back("army subscription levels");
    }

    if (failedOperations.empty()) {
        return std::string();
    }

    std::string baseMessage = "Failed to load: ";
    for (size_t i = 0; i < failedOperations.size(); ++i) {
        if (i > 0)
            baseMessage += ", ";
        baseMessage += failedOperations[i];
    }

    auto appendError = [&baseMessage](const std::optional<ResultError>& err) {
        if (!err.has_value())
            return;
        if (!err->code.empty() || !err->message.empty()) {
            baseMessage += "\n";
            baseMessage += err->code.empty() ? "Error" : err->code;
            if (!err->message.empty()) {
                baseMessage += ": ";
                baseMessage += err->message;
            }
        }
    };

    appendError(result.roomInfoError);
    appendError(result.configStreamerError);
    appendError(result.userInfoError);
    appendError(result.levelsError);

    return baseMessage;
}
