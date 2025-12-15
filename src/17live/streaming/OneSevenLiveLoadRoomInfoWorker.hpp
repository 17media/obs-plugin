#pragma once

#include "api/OneSevenLiveModels.hpp"

// Forward declarations
class OneSevenLiveApiWrappers;
class OneSevenLiveConfigManager;

/**
 * @brief Simple data structure without Qt dependencies
 */
struct OneSevenLiveLoadResult {
    bool roomInfoSuccess = false;
    bool configStreamerSuccess = false;
    bool userInfoSuccess = false;
    bool levelsSuccess = false;
    std::string errorMessage;

    OneSevenLiveLoadResult() = default;

    OneSevenLiveLoadResult(bool roomInfo, bool configStreamer, bool userInfo, bool levels,
                           const std::string& error = std::string())
        : roomInfoSuccess(roomInfo),
          configStreamerSuccess(configStreamer),
          userInfoSuccess(userInfo),
          levelsSuccess(levels),
          errorMessage(error) {}

    /**
     * @brief Check if all required data was loaded successfully
     * @return true if room info and config streamer are both successful
     */
    bool hasRequiredData() const {
        return roomInfoSuccess && configStreamerSuccess;
    }

    /**
     * @brief Check if all optional data was loaded successfully
     * @return true if user info and levels are both successful
     */
    bool hasOptionalData() const {
        return userInfoSuccess && levelsSuccess;
    }

    /**
     * @brief Check if any API call failed
     * @return true if any API call failed
     */
    bool hasFailures() const {
        return !roomInfoSuccess || !configStreamerSuccess || !userInfoSuccess || !levelsSuccess;
    }
};

/**
 * @brief Worker class for loading room information without Qt dependencies
 *
 * This class handles all API calls related to loading room information,
 * including room info, config streamer, user info, and army subscription levels.
 * It provides comprehensive error handling without Qt signal/slot mechanism.
 */
class OneSevenLiveLoadRoomInfoWorker {
   public:
    using LoadResult = OneSevenLiveLoadResult;

    explicit OneSevenLiveLoadRoomInfoWorker(OneSevenLiveApiWrappers* apiWrapper,
                                            OneSevenLiveConfigManager* configManager);

    ~OneSevenLiveLoadRoomInfoWorker() = default;

    /**
     * @brief Start loading room information
     * @param roomID The room ID to load information for
     * @return LoadResult containing the results of all API calls
     */
    LoadResult loadRoomInfo(std::int64_t roomID);

    /**
     * @brief Set the data structures that will be populated
     * This must be called before starting the loading process
     */
    void setDataStructures(OneSevenLiveRoomInfo* roomInfo,
                           OneSevenLiveConfigStreamer* configStreamer,
                           OneSevenLiveUserInfo* userInfo,
                           OneSevenLiveArmySubscriptionLevels* levels);

   private:
    OneSevenLiveApiWrappers* m_apiWrapper = nullptr;
    OneSevenLiveConfigManager* m_configManager = nullptr;

    // Data structures to hold the loaded information
    OneSevenLiveRoomInfo* m_roomInfo = nullptr;
    OneSevenLiveConfigStreamer* m_configStreamer = nullptr;
    OneSevenLiveUserInfo* m_userInfo = nullptr;
    OneSevenLiveArmySubscriptionLevels* m_levels = nullptr;

    /**
     * @brief Get configuration values needed for API calls
     * @param region Output parameter for region
     * @param language Output parameter for language
     * @param userID Output parameter for user ID
     * @return true if all config values were retrieved successfully
     */
    bool getConfigurationValues(std::string& region, std::string& language, std::string& userID);

    /**
     * @brief Validate the loaded data for consistency
     * @param result The load result to validate
     * @return true if data is valid and consistent
     */
    bool validateLoadedData(const LoadResult& result);

    /**
     * @brief Generate a comprehensive error message based on failed API calls
     * @param result The load result containing failure information
     * @return A user-friendly error message
     */
    std::string generateErrorMessage(const LoadResult& result);
};
