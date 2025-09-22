// OBS includes
#include <obs-module.h>

#include "plugin-support.h"

// Qt includes
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Project includes
#include "OneSevenLiveModels.hpp"

// Third-party includes
#include <nlohmann/json.hpp>

using Json = nlohmann::json;
using namespace std;

bool JsonToOneSevenLiveLoginData(const Json &json, OneSevenLiveLoginData &loginData) {
    try {
        if (!json.is_object()) {
            obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveLoginData - Invalid JSON object");
            return false;
        }

        // Handle user information
        const auto &userInfoJson = json["userInfo"];
        if (userInfoJson.is_object()) {
            // Basic user information
            loginData.userInfo.userID = QString::fromStdString(userInfoJson.value("userID", ""));
            loginData.userInfo.openID = QString::fromStdString(userInfoJson.value("openID", ""));
            loginData.userInfo.displayName =
                QString::fromStdString(userInfoJson.value("displayName", ""));
            loginData.userInfo.name = QString::fromStdString(userInfoJson.value("name", ""));
            loginData.userInfo.bio = QString::fromStdString(userInfoJson.value("bio", ""));
            loginData.userInfo.picture = QString::fromStdString(userInfoJson.value("picture", ""));
            loginData.userInfo.website = QString::fromStdString(userInfoJson.value("website", ""));

            // Count information
            loginData.userInfo.followerCount = userInfoJson.value("followerCount", 0);
            loginData.userInfo.followingCount = userInfoJson.value("followingCount", 0);
            loginData.userInfo.receivedLikeCount = userInfoJson.value("receivedLikeCount", 0);
            loginData.userInfo.likeCount = userInfoJson.value("likeCount", 0);

            // Follow status
            loginData.userInfo.isFollowing = userInfoJson.value("isFollowing", 0);
            loginData.userInfo.isNotif = userInfoJson.value("isNotif", 0);
            loginData.userInfo.isBlocked = userInfoJson.value("isBlocked", 0);
            loginData.userInfo.followTime = userInfoJson.value("followTime", 0);
            loginData.userInfo.followRequestTime = userInfoJson.value("followRequestTime", 0);

            // Room and privacy settings
            loginData.userInfo.roomID = userInfoJson.value("roomID", 0);
            loginData.userInfo.privacyMode =
                QString::fromStdString(userInfoJson.value("privacyMode", ""));
            loginData.userInfo.followPrivacyMode = userInfoJson.value("followPrivacyMode", 0);

            // Level and status information
            loginData.userInfo.ballerLevel = userInfoJson.value("ballerLevel", 0);
            loginData.userInfo.postCount = userInfoJson.value("postCount", 0);
            loginData.userInfo.isCelebrity = userInfoJson.value("isCelebrity", 0);
            loginData.userInfo.baller = userInfoJson.value("baller", 0);
            loginData.userInfo.level = userInfoJson.value("level", 0);

            // Other attributes
            loginData.userInfo.revenueShareIndicator =
                QString::fromStdString(userInfoJson.value("revenueShareIndicator", ""));
            loginData.userInfo.clanStatus = userInfoJson.value("clanStatus", 0);
            loginData.userInfo.region = QString::fromStdString(userInfoJson.value("region", ""));
            loginData.userInfo.hideAllPointToLeaderboard =
                userInfoJson.value("hideAllPointToLeaderboard", 0);
            loginData.userInfo.enableShop = userInfoJson.value("enableShop", 0);

            // Timestamp information
            loginData.userInfo.lastLiveTimestamp = userInfoJson.value("lastLiveTimestamp", 0);
            loginData.userInfo.lastCreateLiveTimestamp =
                userInfoJson.value("lastCreateLiveTimestamp", 0);
            loginData.userInfo.lastLiveRegion =
                QString::fromStdString(userInfoJson.value("lastLiveRegion", ""));

            // Boolean attributes
            loginData.userInfo.streamerRecapEnable =
                userInfoJson.value("streamerRecapEnable", false);
            loginData.userInfo.newbieDisplayAllGiftTabsToast =
                userInfoJson.value("newbieDisplayAllGiftTabsToast", false);
            loginData.userInfo.isUnderaged = userInfoJson.value("isUnderaged", false);
            loginData.userInfo.isFreePrivateMsgEnabled =
                userInfoJson.value("isFreePrivateMsgEnabled", false);
            loginData.userInfo.isVliverOnlyModeEnabled =
                userInfoJson.value("isVliverOnlyModeEnabled", false);

            // Integer attributes
            loginData.userInfo.gloryroadMode = userInfoJson.value("gloryroadMode", 0);
            loginData.userInfo.avatarOnboardingPhase =
                userInfoJson.value("avatarOnboardingPhase", 0);
            loginData.userInfo.isEmailVerified = userInfoJson.value("isEmailVerified", 0);

            // String attributes
            loginData.userInfo.extIDAppleTransfer =
                QString::fromStdString(userInfoJson.value("extIDAppleTransfer", ""));
            loginData.userInfo.commentShadowColor =
                QString::fromStdString(userInfoJson.value("commentShadowColor", ""));

            // Array attributes
            if (userInfoJson.contains("badgeInfo") && userInfoJson["badgeInfo"].is_array()) {
                for (const auto &badge : userInfoJson["badgeInfo"]) {
                    loginData.userInfo.badgeInfo.append(
                        QString::fromStdString(badge.get<std::string>()));
                }
            }

            if (userInfoJson.contains("loyaltyInfo") && userInfoJson["loyaltyInfo"].is_array()) {
                for (const auto &loyalty : userInfoJson["loyaltyInfo"]) {
                    loginData.userInfo.loyaltyInfo.append(
                        QString::fromStdString(loyalty.get<std::string>()));
                }
            }

            if (userInfoJson.contains("lastUsedHashtags") &&
                userInfoJson["lastUsedHashtags"].is_array()) {
                for (const auto &hashtag : userInfoJson["lastUsedHashtags"]) {
                    loginData.userInfo.lastUsedHashtags.append(
                        QString::fromStdString(hashtag.get<std::string>()));
                }
            }

            if (userInfoJson.contains("levelBadges") && userInfoJson["levelBadges"].is_array()) {
                for (const auto &badge : userInfoJson["levelBadges"]) {
                    loginData.userInfo.levelBadges.append(
                        QString::fromStdString(badge.get<std::string>()));
                }
            }

            // Object attributes - monthlyVIPBadges
            // Note: This assumes QVariantMap can be built directly from JSON object, actual
            // implementation may need adjustment
            if (userInfoJson["monthlyVIPBadges"].is_object()) {
                // Need to handle monthlyVIPBadges based on actual situation
                // Simple example:
                // const auto& badges = userInfoJson["monthlyVIPBadges"].object_items();
                // for (const auto& pair : badges) {
                //     loginData.userInfo.monthlyVIPBadges.insert(QString::fromStdString(pair.first),
                //     QVariant::fromValue(pair.second));
                // }
            }
        }

        // Handle basic response information
        loginData.message = QString::fromStdString(json.value("message", ""));
        loginData.result = QString::fromStdString(json.value("result", ""));
        loginData.refreshToken = QString::fromStdString(json.value("refreshToken", ""));
        loginData.jwtAccessToken = QString::fromStdString(json.value("jwtAccessToken", ""));
        loginData.accessToken = QString::fromStdString(json.value("accessToken", ""));
        loginData.giftModuleState = json.value("giftModuleState", 0);
        loginData.word = QString::fromStdString(json.value("word", ""));

        // Handle A/B testing related fields
        loginData.abtestNewbieFocus = QString::fromStdString(json.value("abtestNewbieFocus", ""));
        loginData.abtestNewbieGuidance =
            QString::fromStdString(json.value("abtestNewbieGuidance", ""));
        loginData.abtestNewbieGuide = QString::fromStdString(json.value("abtestNewbieGuide", ""));

        // Handle recommendation and onboarding related fields
        loginData.showRecommend = json.value("showRecommend", false);
        loginData.newbieEnhanceGuidanceStyle = json.value("newbieEnhanceGuidanceStyle", 0);
        loginData.newbieGuidanceFocusMissionEnable =
            json.value("newbieGuidanceFocusMissionEnable", false);

        // Handle auto-enter live streaming related fields
        if (json.contains("autoEnterLive") && json["autoEnterLive"].is_object()) {
            const auto &autoEnterJson = json["autoEnterLive"];
            // Note: Field name in JSON is "auto", but field name in struct is "autoEnter"
            loginData.autoEnterLive.autoEnter = autoEnterJson.value("auto", false);
            loginData.autoEnterLive.liveStreamID = autoEnterJson.value("liveStreamID", 0);
        }

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveLoginData exception: %s", e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveLoginData unknown exception");
        return false;
    }
}

bool OneSevenLiveChangeEventRequestToJson(const OneSevenLiveChangeEventRequest &request,
                                          nlohmann::json &json) {
    json = nlohmann::json{
        {"eventID", static_cast<int>(request.eventID)},
    };

    return true;
}

bool JsonToOneSevenLiveArmyName(const nlohmann::json &json, OneSevenLiveArmyName &armyName) {
    if (!json.is_object()) {
        return false;
    }

    armyName.customName = QString::fromStdString(json.value("customName", ""));
    armyName.defaultName = QString::fromStdString(json.value("defaultName", ""));

    return true;
}

bool OneSevenLiveArmyNameToJson(const OneSevenLiveArmyName &armyName, nlohmann::json &json) {
    json = nlohmann::json{
        {"customName", armyName.customName.toStdString()},
        {"defaultName", armyName.defaultName.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveArmyRankName(const nlohmann::json &json,
                                    OneSevenLiveArmyRankName &rankName) {
    if (!json.is_object()) {
        return false;
    }

    rankName.rank = json.value("rank", 0);
    rankName.rankTier = json.value("rankTier", 0);
    rankName.customName = QString::fromStdString(json.value("customName", ""));
    rankName.defaultName = QString::fromStdString(json.value("defaultName", ""));

    return true;
}

bool OneSevenLiveArmyRankNameToJson(const OneSevenLiveArmyRankName &rankName,
                                    nlohmann::json &json) {
    json = nlohmann::json{
        {"rank", rankName.rank},
        {"rankTier", rankName.rankTier},
        {"customName", rankName.customName.toStdString()},
        {"defaultName", rankName.defaultName.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveArmyNameResponse(const nlohmann::json &json,
                                        OneSevenLiveArmyNameResponse &response) {
    if (!json.is_object()) {
        return false;
    }

    // Parse armyName object
    if (json.contains("armyName") && json["armyName"].is_object()) {
        const auto &armyNameJson = json["armyName"];
        if (!JsonToOneSevenLiveArmyName(armyNameJson, response.armyName)) {
            return false;
        }
    }

    // Parse rankName array
    if (json.contains("rankName") && json["rankName"].is_array()) {
        const auto &rankNameArray = json["rankName"];
        for (const auto &rankNameJson : rankNameArray) {
            OneSevenLiveArmyRankName rankName;
            if (JsonToOneSevenLiveArmyRankName(rankNameJson, rankName)) {
                response.rankName.append(rankName);
            }
        }
    }

    return true;
}

bool OneSevenLiveArmyNameResponseToJson(const OneSevenLiveArmyNameResponse &response,
                                        nlohmann::json &json) {
    // Convert armyName object
    nlohmann::json armyNameJson;
    OneSevenLiveArmyNameToJson(response.armyName, armyNameJson);

    // Convert rankName array
    nlohmann::json rankNameJsonArray = nlohmann::json::array();
    for (const auto &rankName : response.rankName) {
        nlohmann::json rankNameJson;
        OneSevenLiveArmyRankNameToJson(rankName, rankNameJson);
        rankNameJsonArray.push_back(rankNameJson);
    }

    json = nlohmann::json{
        {"armyName", armyNameJson},
        {"rankName", rankNameJsonArray},
    };

    return true;
}

// Convert JSON to OneSevenLiveLabelToken
bool JsonToOneSevenLiveLabelToken(const nlohmann::json &json, OneSevenLiveLabelToken &labelToken) {
    if (!json.is_object()) {
        return false;
    }

    if (json.contains("key") && json["key"].is_string()) {
        labelToken.key = QString::fromStdString(json["key"].get<std::string>());
    }

    return true;
}

// Convert OneSevenLiveLabelToken to JSON
bool OneSevenLiveLabelTokenToJson(const OneSevenLiveLabelToken &labelToken, nlohmann::json &json) {
    json = nlohmann::json{
        {"key", labelToken.key.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveGloryroadInfo(const nlohmann::json &jsonData,
                                     OneSevenLiveGloryroadInfo &gloryroadInfo) {
    if (!jsonData.is_object()) {
        return false;
    }

    // int point;
    if (jsonData.contains("point") && jsonData["point"].is_number()) {
        gloryroadInfo.point = jsonData["point"].get<int>();
    }

    // int level;
    if (jsonData.contains("level") && jsonData["level"].is_number()) {
        gloryroadInfo.level = jsonData["level"].get<int>();
    }

    // QString iconURL;
    if (jsonData.contains("iconURL") && jsonData["iconURL"].is_string()) {
        gloryroadInfo.iconURL = QString::fromStdString(jsonData["iconURL"].get<std::string>());
    }

    // QString badgeIconURL;
    if (jsonData.contains("badgeIconURL") && jsonData["badgeIconURL"].is_string()) {
        gloryroadInfo.badgeIconURL =
            QString::fromStdString(jsonData["badgeIconURL"].get<std::string>());
    }

    return true;
}

// Convert JSON to OneSevenLiveArmyInfoUser
bool JsonToOneSevenLiveArmyInfoUser(const nlohmann::json &json, OneSevenLiveArmyInfoUser &user) {
    if (!json.is_object()) {
        return false;
    }

    if (json.contains("userID") && json["userID"].is_string()) {
        user.userID = QString::fromStdString(json["userID"].get<std::string>());
    }

    if (json.contains("displayName") && json["displayName"].is_string()) {
        user.displayName = QString::fromStdString(json["displayName"].get<std::string>());
    }

    if (json.contains("picture") && json["picture"].is_string()) {
        user.picture = QString::fromStdString(json["picture"].get<std::string>());
    }

    if (json.contains("name") && json["name"].is_string()) {
        user.name = QString::fromStdString(json["name"].get<std::string>());
    }

    if (json.contains("level") && json["level"].is_number()) {
        user.level = json["level"].get<int>();
    }

    if (json.contains("openID") && json["openID"].is_string()) {
        user.openID = QString::fromStdString(json["openID"].get<std::string>());
    }

    if (json.contains("region") && json["region"].is_string()) {
        user.region = QString::fromStdString(json["region"].get<std::string>());
    }

    if (json.contains("gloryroadInfo") && json["gloryroadInfo"].is_object()) {
        JsonToOneSevenLiveGloryroadInfo(json["gloryroadInfo"], user.gloryroadInfo);
    }

    if (json.contains("gloryroadMode") && json["gloryroadMode"].is_number()) {
        user.gloryroadMode = json["gloryroadMode"].get<int>();
    }

    return true;
}

bool OneSevenLiveGloryroadInfoToJson(const OneSevenLiveGloryroadInfo &gloryroadInfo,
                                     nlohmann::json &jsonData) {
    jsonData = nlohmann::json{
        {"point", gloryroadInfo.point},
        {"level", gloryroadInfo.level},
        {"iconURL", gloryroadInfo.iconURL.toStdString()},
        {"badgeIconURL", gloryroadInfo.badgeIconURL.toStdString()},
    };

    return true;
}

// Convert OneSevenLiveArmyInfoUser to JSON
bool OneSevenLiveArmyInfoUserToJson(const OneSevenLiveArmyInfoUser &user, nlohmann::json &json) {
    nlohmann::json gloryroadInfoJson;
    OneSevenLiveGloryroadInfoToJson(user.gloryroadInfo, gloryroadInfoJson);

    json = nlohmann::json{
        {"userID", user.userID.toStdString()},
        {"displayName", user.displayName.toStdString()},
        {"picture", user.picture.toStdString()},
        {"name", user.name.toStdString()},
        {"level", user.level},
        {"openID", user.openID.toStdString()},
        {"region", user.region.toStdString()},
        {"gloryroadInfo", gloryroadInfoJson},
        {"gloryroadMode", user.gloryroadMode},
    };

    return true;
}

// Convert JSON to OneSevenLiveArmyInfo
bool JsonToOneSevenLiveArmyInfo(const nlohmann::json &json, OneSevenLiveArmyInfo &armyInfo) {
    if (!json.is_object()) {
        return false;
    }

    if (json["user"].is_object()) {
        JsonToOneSevenLiveArmyInfoUser(json["user"], armyInfo.user);
    }

    if (json.contains("rank") && json["rank"].is_number()) {
        armyInfo.rank = json["rank"].get<int>();
    }

    if (json.contains("pointContribution") && json["pointContribution"].is_number()) {
        armyInfo.pointContribution = json["pointContribution"].get<int>();
    }

    if (json.contains("seniority") && json["seniority"].is_number()) {
        armyInfo.seniority = json["seniority"].get<int>();
    }

    if (json.contains("startTime") && json["startTime"].is_number()) {
        armyInfo.startTime = json["startTime"].get<int>();
    }

    if (json.contains("endTime") && json["endTime"].is_number()) {
        armyInfo.endTime = json["endTime"].get<int>();
    }

    if (json.contains("isOnLive") && json["isOnLive"].is_boolean()) {
        armyInfo.isOnLive = json["isOnLive"].get<bool>();
    }

    if (json.contains("newStatus") && json["newStatus"].is_number()) {
        armyInfo.newStatus = json["newStatus"].get<int>();
    }

    if (json.contains("periodStartTime") && json["periodStartTime"].is_number()) {
        armyInfo.periodStartTime = json["periodStartTime"].get<int>();
    }

    return true;
}

// Convert OneSevenLiveArmyInfo to JSON
bool OneSevenLiveArmyInfoToJson(const OneSevenLiveArmyInfo &armyInfo, nlohmann::json &json) {
    nlohmann::json userJson;
    OneSevenLiveArmyInfoUserToJson(armyInfo.user, userJson);

    json = nlohmann::json{
        {"user", userJson},
        {"rank", armyInfo.rank},
        {"pointContribution", static_cast<int>(armyInfo.pointContribution)},
        {"seniority", armyInfo.seniority},
        {"startTime", static_cast<int>(armyInfo.startTime)},
        {"endTime", static_cast<int>(armyInfo.endTime)},
        {"isOnLive", armyInfo.isOnLive},
        {"newStatus", armyInfo.newStatus},
        {"periodStartTime", static_cast<int>(armyInfo.periodStartTime)},
    };

    return true;
}

// Convert JSON to OneSevenLiveUserAttr
bool JsonToOneSevenLiveUserAttr(const nlohmann::json &json, OneSevenLiveUserAttr &userAttr) {
    if (!json.is_object()) {
        return false;
    }

    if (json.contains("level") && json["level"].is_number()) {
        userAttr.level = json["level"].get<int>();
    }

    if (json.contains("sentPoint") && json["sentPoint"].is_number()) {
        userAttr.sentPoint = json["sentPoint"].get<int>();
    }

    if (json.contains("checkinLevel") && json["checkinLevel"].is_number()) {
        userAttr.checkinLevel = json["checkinLevel"].get<int>();
    }

    if (json.contains("checkinCount") && json["checkinCount"].is_number()) {
        userAttr.checkinCount = json["checkinCount"].get<int>();
    }

    if (json.contains("checkinBdgURL") && json["checkinBdgURL"].is_string()) {
        userAttr.checkinBdgURL = QString::fromStdString(json["checkinBdgURL"].get<std::string>());
    }

    if (json.contains("noteStatus") && json["noteStatus"].is_number()) {
        userAttr.noteStatus = json["noteStatus"].get<int>();
    }

    if (json.contains("followStatus") && json["followStatus"].is_number()) {
        userAttr.followStatus = json["followStatus"].get<int>();
    }

    if (json.contains("gloryroadMode") && json["gloryroadMode"].is_number()) {
        userAttr.gloryroadMode = json["gloryroadMode"].get<int>();
    }

    if (json["gloryroadInfo"].is_object()) {
        JsonToOneSevenLiveGloryroadInfo(json["gloryroadInfo"], userAttr.gloryroadInfo);
    }

    return true;
}

// Convert OneSevenLiveUserAttr to JSON
bool OneSevenLiveUserAttrToJson(const OneSevenLiveUserAttr &userAttr, nlohmann::json &json) {
    nlohmann::json gloryroadInfoJson;
    OneSevenLiveGloryroadInfoToJson(userAttr.gloryroadInfo, gloryroadInfoJson);

    json = nlohmann::json{
        {"level", userAttr.level},
        {"sentPoint", userAttr.sentPoint},
        {"checkinLevel", userAttr.checkinLevel},
        {"checkinCount", userAttr.checkinCount},
        {"checkinBdgURL", userAttr.checkinBdgURL.toStdString()},
        {"noteStatus", userAttr.noteStatus},
        {"followStatus", userAttr.followStatus},
        {"gloryroadMode", userAttr.gloryroadMode},
        {"gloryroadInfo", gloryroadInfoJson},
    };

    return true;
}

// Convert JSON to OneSevenLiveAnonymousInfo
bool JsonToOneSevenLiveAnonymousInfo(const nlohmann::json &json,
                                     OneSevenLiveAnonymousInfo &anonymousInfo) {
    if (!json.is_object()) {
        return false;
    }

    if (json.contains("isInvisible") && json["isInvisible"].is_boolean()) {
        anonymousInfo.isInvisible = json["isInvisible"].get<bool>();
    }

    if (json.contains("pureText") && json["pureText"].is_string()) {
        anonymousInfo.pureText = QString::fromStdString(json["pureText"].get<std::string>());
    }

    return true;
}

// Convert OneSevenLiveAnonymousInfo to JSON
bool OneSevenLiveAnonymousInfoToJson(const OneSevenLiveAnonymousInfo &anonymousInfo,
                                     nlohmann::json &json) {
    json = nlohmann::json{
        {"isInvisible", anonymousInfo.isInvisible},
        {"pureText", anonymousInfo.pureText.toStdString()},
    };

    return true;
}

// Convert JSON to OneSevenLiveDisplayUser
bool JsonToOneSevenLiveDisplayUser(const nlohmann::json &json,
                                   OneSevenLiveDisplayUser &displayUser) {
    if (!json.is_object()) {
        return false;
    }

    if (json.contains("armyRank") && json["armyRank"].is_number()) {
        displayUser.armyRank = json["armyRank"].get<int>();
    }

    if (json.contains("badgeURL") && json["badgeURL"].is_string()) {
        displayUser.badgeURL = QString::fromStdString(json["badgeURL"].get<std::string>());
    }

    if (json.contains("bgColor") && json["bgColor"].is_string()) {
        displayUser.bgColor = QString::fromStdString(json["bgColor"].get<std::string>());
    }

    if (json.contains("checkinBdgURL") && json["checkinBdgURL"].is_string()) {
        displayUser.checkinBdgURL =
            QString::fromStdString(json["checkinBdgURL"].get<std::string>());
    }

    if (json.contains("checkinLevel") && json["checkinLevel"].is_number()) {
        displayUser.checkinLevel = json["checkinLevel"].get<int>();
    }

    if (json.contains("circleBadgeURL") && json["circleBadgeURL"].is_string()) {
        displayUser.circleBadgeURL =
            QString::fromStdString(json["circleBadgeURL"].get<std::string>());
    }

    if (json.contains("displayName") && json["displayName"].is_string()) {
        displayUser.displayName = QString::fromStdString(json["displayName"].get<std::string>());
    }

    if (json.contains("fgColor") && json["fgColor"].is_string()) {
        displayUser.fgColor = QString::fromStdString(json["fgColor"].get<std::string>());
    }

    if (json["gloryroadInfo"].is_object()) {
        JsonToOneSevenLiveGloryroadInfo(json["gloryroadInfo"], displayUser.gloryroadInfo);
    }

    if (json.contains("gloryroadMode") && json["gloryroadMode"].is_number()) {
        displayUser.gloryroadMode = json["gloryroadMode"].get<int>();
    }

    if (json.contains("hasProgram") && json["hasProgram"].is_boolean()) {
        displayUser.hasProgram = json["hasProgram"].get<bool>();
    }

    if (json.contains("isDirty") && json["isDirty"].is_boolean()) {
        displayUser.isDirty = json["isDirty"].get<bool>();
    }

    if (json.contains("isDirtyUser") && json["isDirtyUser"].is_boolean()) {
        displayUser.isDirtyUser = json["isDirtyUser"].get<bool>();
    }

    if (json.contains("isGuardian") && json["isGuardian"].is_boolean()) {
        displayUser.isGuardian = json["isGuardian"].get<bool>();
    }

    if (json.contains("isProducer") && json["isProducer"].is_boolean()) {
        displayUser.isProducer = json["isProducer"].get<bool>();
    }

    if (json.contains("isStreamer") && json["isStreamer"].is_boolean()) {
        displayUser.isStreamer = json["isStreamer"].get<bool>();
    }

    if (json.contains("isVIP") && json["isVIP"].is_boolean()) {
        displayUser.isVIP = json["isVIP"].get<bool>();
    }

    if (json.contains("level") && json["level"].is_number()) {
        displayUser.level = json["level"].get<int>();
    }

    if (json.contains("mLevel") && json["mLevel"].is_number()) {
        displayUser.mLevel = json["mLevel"].get<int>();
    }

    if (json.contains("pfxBadgeURL") && json["pfxBadgeURL"].is_string()) {
        displayUser.pfxBadgeURL = QString::fromStdString(json["pfxBadgeURL"].get<std::string>());
    }

    if (json.contains("picture") && json["picture"].is_string()) {
        displayUser.picture = QString::fromStdString(json["picture"].get<std::string>());
    }

    if (json.contains("producer") && json["producer"].is_number()) {
        displayUser.producer = json["producer"].get<int>();
    }

    if (json.contains("program") && json["program"].is_number()) {
        displayUser.program = json["program"].get<int>();
    }

    if (json.contains("topRightIconURL") && json["topRightIconURL"].is_string()) {
        displayUser.topRightIconURL =
            QString::fromStdString(json["topRightIconURL"].get<std::string>());
    }

    if (json.contains("userID") && json["userID"].is_string()) {
        displayUser.userID = QString::fromStdString(json["userID"].get<std::string>());
    }

    if (json.contains("vipCharmURL") && json["vipCharmURL"].is_string()) {
        displayUser.vipCharmURL = QString::fromStdString(json["vipCharmURL"].get<std::string>());
    }

    return true;
}

// Convert OneSevenLiveDisplayUser to JSON
bool OneSevenLiveDisplayUserToJson(const OneSevenLiveDisplayUser &displayUser,
                                   nlohmann::json &json) {
    nlohmann::json gloryroadInfoJson;
    OneSevenLiveGloryroadInfoToJson(displayUser.gloryroadInfo, gloryroadInfoJson);

    json = nlohmann::json{
        {"armyRank", displayUser.armyRank},
        {"badgeURL", displayUser.badgeURL.toStdString()},
        {"bgColor", displayUser.bgColor.toStdString()},
        {"checkinBdgURL", displayUser.checkinBdgURL.toStdString()},
        {"checkinLevel", displayUser.checkinLevel},
        {"circleBadgeURL", displayUser.circleBadgeURL.toStdString()},
        {"displayName", displayUser.displayName.toStdString()},
        {"fgColor", displayUser.fgColor.toStdString()},
        {"gloryroadInfo", gloryroadInfoJson},
        {"gloryroadMode", displayUser.gloryroadMode},
        {"hasProgram", displayUser.hasProgram},
        {"isDirty", displayUser.isDirty},
        {"isDirtyUser", displayUser.isDirtyUser},
        {"isGuardian", displayUser.isGuardian},
        {"isProducer", displayUser.isProducer},
        {"isStreamer", displayUser.isStreamer},
        {"isVIP", displayUser.isVIP},
        {"level", displayUser.level},
        {"mLevel", displayUser.mLevel},
        {"pfxBadgeURL", displayUser.pfxBadgeURL.toStdString()},
        {"picture", displayUser.picture.toStdString()},
        {"producer", displayUser.producer},
        {"program", displayUser.program},
        {"topRightIconURL", displayUser.topRightIconURL.toStdString()},
        {"userID", displayUser.userID.toStdString()},
        {"vipCharmURL", displayUser.vipCharmURL.toStdString()},
    };

    return true;
}

// Convert JSON to OneSevenLiveGiftRankOne
bool JsonToOneSevenLiveGiftRankOne(const nlohmann::json &json,
                                   OneSevenLiveGiftRankOne &giftRankOne) {
    if (!json.is_object()) {
        return false;
    }

    if (json.contains("displayName") && json["displayName"].is_string()) {
        giftRankOne.displayName = QString::fromStdString(json["displayName"].get<std::string>());
    }

    if (json.contains("picture") && json["picture"].is_string()) {
        giftRankOne.picture = QString::fromStdString(json["picture"].get<std::string>());
    }

    if (json.contains("timestampMs") && json["timestampMs"].is_number()) {
        giftRankOne.timestampMs = static_cast<qint64>(json["timestampMs"].get<double>());
    }

    if (json.contains("userID") && json["userID"].is_string()) {
        giftRankOne.userID = QString::fromStdString(json["userID"].get<std::string>());
    }

    return true;
}

// Convert OneSevenLiveGiftRankOne to JSON
bool OneSevenLiveGiftRankOneToJson(const OneSevenLiveGiftRankOne &giftRankOne,
                                   nlohmann::json &json) {
    json = nlohmann::json{
        {"displayName", giftRankOne.displayName.toStdString()},
        {"picture", giftRankOne.picture.toStdString()},
        {"timestampMs", static_cast<int>(giftRankOne.timestampMs)},
        {"userID", giftRankOne.userID.toStdString()},
    };

    return true;
}

// Convert JSON to OneSevenLiveRockZoneViewer
bool JsonToOneSevenLiveRockZoneViewer(const nlohmann::json &json,
                                      OneSevenLiveRockZoneViewer &viewer) {
    if (!json.is_object()) {
        return false;
    }

    if (json.contains("type") && json["type"].is_number()) {
        viewer.type = json["type"].get<int>();
    }

    if (json.contains("armyInfo") && json["armyInfo"].is_object()) {
        JsonToOneSevenLiveArmyInfo(json["armyInfo"], viewer.armyInfo);
    }

    if (json.contains("labelToken") && json["labelToken"].is_object()) {
        JsonToOneSevenLiveLabelToken(json["labelToken"], viewer.labelToken);
    }

    if (json.contains("userAttr") && json["userAttr"].is_object()) {
        JsonToOneSevenLiveUserAttr(json["userAttr"], viewer.userAttr);
    }

    if (json.contains("anonymousInfo") && json["anonymousInfo"].is_object()) {
        JsonToOneSevenLiveAnonymousInfo(json["anonymousInfo"], viewer.anonymousInfo);
    }

    if (json.contains("armyLevel") && json["armyLevel"].is_number()) {
        viewer.armyLevel = json["armyLevel"].get<int>();
    }

    if (json.contains("displayUser") && json["displayUser"].is_object()) {
        JsonToOneSevenLiveDisplayUser(json["displayUser"], viewer.displayUser);
    }

    if (json.contains("giftRankOne") && json["giftRankOne"].is_object()) {
        JsonToOneSevenLiveGiftRankOne(json["giftRankOne"], viewer.giftRankOne);
    }

    return true;
}

// Convert OneSevenLiveRockZoneViewer to JSON
bool OneSevenLiveRockZoneViewerToJson(const OneSevenLiveRockZoneViewer &viewer,
                                      nlohmann::json &json) {
    nlohmann::json armyInfoJson;
    OneSevenLiveArmyInfoToJson(viewer.armyInfo, armyInfoJson);

    nlohmann::json labelTokenJson;
    OneSevenLiveLabelTokenToJson(viewer.labelToken, labelTokenJson);

    nlohmann::json userAttrJson;
    OneSevenLiveUserAttrToJson(viewer.userAttr, userAttrJson);

    nlohmann::json anonymousInfoJson;
    OneSevenLiveAnonymousInfoToJson(viewer.anonymousInfo, anonymousInfoJson);

    nlohmann::json displayUserJson;
    OneSevenLiveDisplayUserToJson(viewer.displayUser, displayUserJson);

    nlohmann::json giftRankOneJson;
    OneSevenLiveGiftRankOneToJson(viewer.giftRankOne, giftRankOneJson);

    json = nlohmann::json{
        {"type", viewer.type},
        {"armyInfo", armyInfoJson},
        {"labelToken", labelTokenJson},
        {"userAttr", userAttrJson},
        {"anonymousInfo", anonymousInfoJson},
        {"armyLevel", viewer.armyLevel},
        {"displayUser", displayUserJson},
        {"giftRankOne", giftRankOneJson},
    };

    return true;
}

bool JsonToOneSevenLiveRockViewers(const nlohmann::json &json,
                                   QList<OneSevenLiveRockZoneViewer> &viewers) {
    if (!json.is_array()) {
        return false;
    }

    for (const auto &itemJson : json) {
        OneSevenLiveRockZoneViewer viewer;
        JsonToOneSevenLiveRockZoneViewer(itemJson, viewer);
        viewers.append(viewer);
    }

    return true;
}

bool JsonToOneSevenLiveRtmpUrl(const nlohmann::json &urlJson, OneSevenLiveRtmpUrl &rtmpUrl) {
    if (!urlJson.is_object()) {
        return false;
    }

    if (urlJson.contains("provider") && urlJson["provider"].is_number()) {
        rtmpUrl.provider = urlJson["provider"].get<int>();
    }
    if (urlJson.contains("streamType") && urlJson["streamType"].is_string()) {
        rtmpUrl.streamType = QString::fromStdString(urlJson["streamType"].get<std::string>());
    }
    if (urlJson.contains("url") && urlJson["url"].is_string()) {
        rtmpUrl.url = QString::fromStdString(urlJson["url"].get<std::string>());
    }
    if (urlJson.contains("urlLowQuality") && urlJson["urlLowQuality"].is_string()) {
        rtmpUrl.urlLowQuality = QString::fromStdString(urlJson["urlLowQuality"].get<std::string>());
    }
    if (urlJson.contains("webUrl") && urlJson["webUrl"].is_string()) {
        rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].get<std::string>());
    }
    if (urlJson.contains("webUrlLowQuality") && urlJson["webUrlLowQuality"].is_string()) {
        rtmpUrl.webUrlLowQuality =
            QString::fromStdString(urlJson["webUrlLowQuality"].get<std::string>());
    }
    if (urlJson.contains("urlHighQuality") && urlJson["urlHighQuality"].is_string()) {
        rtmpUrl.urlHighQuality =
            QString::fromStdString(urlJson["urlHighQuality"].get<std::string>());
    }
    if (urlJson.contains("weight") && urlJson["weight"].is_number()) {
        rtmpUrl.weight = urlJson["weight"].get<int>();
    }
    if (urlJson.contains("throttle") && urlJson["throttle"].is_boolean()) {
        rtmpUrl.throttle = urlJson["throttle"].get<bool>();
    }
    return true;
}

// Helper function to parse RTMP URLs array from JSON
bool JsonToOneSevenLiveRtmpUrls(const nlohmann::json &rtmpUrlsJson,
                                QList<OneSevenLiveRtmpUrl> &rtmpUrls) {
    if (!rtmpUrlsJson.is_array()) {
        return false;
    }

    for (const auto &urlJson : rtmpUrlsJson) {
        OneSevenLiveRtmpUrl rtmpUrl;
        if (JsonToOneSevenLiveRtmpUrl(urlJson, rtmpUrl)) {
            rtmpUrls.append(rtmpUrl);
        }
    }
    return true;
}

// Helper function to parse pull URLs info from JSON
bool JsonToOneSevenLivePullUrlsInfo(const nlohmann::json &pullUrlsInfoJson,
                                    OneSevenLivePullUrlsInfo &pullUrlsInfo) {
    if (!pullUrlsInfoJson.is_object()) {
        return false;
    }

    if (pullUrlsInfoJson.contains("seqNo") && pullUrlsInfoJson["seqNo"].is_number()) {
        pullUrlsInfo.seqNo = pullUrlsInfoJson["seqNo"].get<int>();
    }
    if (pullUrlsInfoJson.contains("rtmpURLs") && pullUrlsInfoJson["rtmpURLs"].is_array()) {
        JsonToOneSevenLiveRtmpUrls(pullUrlsInfoJson["rtmpURLs"], pullUrlsInfo.rtmpURLs);
    }
    return true;
}

bool JsonToOneSevenLiveEventList(const nlohmann::json &eventListJson,
                                 QList<OneSevenLiveEventInfo> &eventList) {
    if (!eventListJson.is_array()) {
        return false;
    }

    for (const auto &eventJson : eventListJson) {
        OneSevenLiveEventInfo eventInfo;
        if (eventJson.contains("ID") && eventJson["ID"].is_number()) {
            eventInfo.ID = eventJson["ID"].get<int>();
        }
        if (eventJson.contains("type") && eventJson["type"].is_number()) {
            eventInfo.type = eventJson["type"].get<int>();
        }
        if (eventJson.contains("icon") && eventJson["icon"].is_string()) {
            eventInfo.icon = QString::fromStdString(eventJson["icon"].get<std::string>());
        }
        if (eventJson.contains("endTime") && eventJson["endTime"].is_number()) {
            eventInfo.endTime = eventJson["endTime"].get<int>();
        }
        if (eventJson.contains("showTimer") && eventJson["showTimer"].is_number()) {
            eventInfo.showTimer = eventJson["showTimer"].get<int>();
        }
        if (eventJson.contains("name") && eventJson["name"].is_string()) {
            eventInfo.name = QString::fromStdString(eventJson["name"].get<std::string>());
        }
        if (eventJson.contains("URL") && eventJson["URL"].is_string()) {
            eventInfo.URL = QString::fromStdString(eventJson["URL"].get<std::string>());
        }
        if (eventJson.contains("pageSize") && eventJson["pageSize"].is_number()) {
            eventInfo.pageSize = eventJson["pageSize"].get<int>();
        }
        if (eventJson.contains("webViewTitle") && eventJson["webViewTitle"].is_string()) {
            eventInfo.webViewTitle =
                QString::fromStdString(eventJson["webViewTitle"].get<std::string>());
        }

        // Parse icon list
        if (eventJson.contains("icons") && eventJson["icons"].is_array()) {
            const auto &iconsJson = eventJson["icons"];
            for (const auto &iconJson : iconsJson) {
                OneSevenLiveEventIcon icon;
                if (iconJson.contains("language") && iconJson["language"].is_string()) {
                    icon.language = QString::fromStdString(iconJson["language"].get<std::string>());
                }
                if (iconJson.contains("value") && iconJson["value"].is_string()) {
                    icon.value = QString::fromStdString(iconJson["value"].get<std::string>());
                }
                eventInfo.icons.append(icon);
            }
        }

        eventList.append(eventInfo);
    }
    return true;
}

// Helper function to parse hashtags from JSON
bool JsonToOneSevenLiveHashtags(const nlohmann::json &hashtagsJson,
                                QList<OneSevenLiveHashtag> &hashtags) {
    if (!hashtagsJson.is_array()) {
        return false;
    }

    for (const auto &hashtagJson : hashtagsJson) {
        OneSevenLiveHashtag hashtag;
        if (hashtagJson.contains("text") && hashtagJson["text"].is_string()) {
            hashtag.text = QString::fromStdString(hashtagJson["text"].get<std::string>());
        }
        if (hashtagJson.contains("isOfficial") && hashtagJson["isOfficial"].is_boolean()) {
            hashtag.isOfficial = hashtagJson["isOfficial"].get<bool>();
        }
        hashtags.append(hashtag);
    }
    return true;
}

bool JsonToOneSevenLiveArchiveConfig(const nlohmann::json &archiveConfigJson,
                                     OneSevenLiveArchiveConfig &archiveConfig) {
    if (!archiveConfigJson.is_object()) {
        return false;
    }

    if (archiveConfigJson.contains("autoRecording") &&
        archiveConfigJson["autoRecording"].is_boolean()) {
        archiveConfig.autoRecording = archiveConfigJson["autoRecording"].get<bool>();
    }
    if (archiveConfigJson.contains("autoPublish") &&
        archiveConfigJson["autoPublish"].is_boolean()) {
        archiveConfig.autoPublish = archiveConfigJson["autoPublish"].get<bool>();
    }
    if (archiveConfigJson.contains("clipPermission") &&
        archiveConfigJson["clipPermission"].is_number()) {
        archiveConfig.clipPermission = archiveConfigJson["clipPermission"].get<int>();
    }
    if (archiveConfigJson.contains("clipPermissionDownload") &&
        archiveConfigJson["clipPermissionDownload"].is_number()) {
        archiveConfig.clipPermissionDownload =
            archiveConfigJson["clipPermissionDownload"].get<int>();
    }

    return true;
}

bool JsonToOneSevenLiveRoomInfo(const nlohmann::json &json, OneSevenLiveRoomInfo &roomInfo) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Basic information
        if (json.contains("userID") && json["userID"].is_string()) {
            roomInfo.userID = QString::fromStdString(json["userID"].get<std::string>());
        }
        if (json.contains("streamerType") && json["streamerType"].is_number_integer()) {
            roomInfo.streamerType = json["streamerType"].get<int>();
        }
        if (json.contains("streamType") && json["streamType"].is_string()) {
            roomInfo.streamType = QString::fromStdString(json["streamType"].get<std::string>());
        }
        if (json.contains("status") && json["status"].is_number_integer()) {
            roomInfo.status = json["status"].get<int>();
        }
        if (json.contains("caption") && json["caption"].is_string()) {
            roomInfo.caption = QString::fromStdString(json["caption"].get<std::string>());
        }
        if (json.contains("thumbnail") && json["thumbnail"].is_string()) {
            roomInfo.thumbnail = QString::fromStdString(json["thumbnail"].get<std::string>());
        }

        // Parse RTMP URLs
        if (json.contains("rtmpUrls") && json["rtmpUrls"].is_array()) {
            JsonToOneSevenLiveRtmpUrls(json["rtmpUrls"], roomInfo.rtmpUrls);
        }

        // Parse Pull URLs Info
        if (json.contains("pullURLsInfo") && json["pullURLsInfo"].is_object()) {
            JsonToOneSevenLivePullUrlsInfo(json["pullURLsInfo"], roomInfo.pullURLsInfo);
        }

        // Live streaming information
        if (json.contains("allowCallin") && json["allowCallin"].is_number_integer()) {
            roomInfo.allowCallin = json["allowCallin"].get<int>();
        }
        if (json.contains("restreamerOpenID") && json["restreamerOpenID"].is_string()) {
            roomInfo.restreamerOpenID =
                QString::fromStdString(json["restreamerOpenID"].get<std::string>());
        }
        if (json.contains("streamID") && json["streamID"].is_string()) {
            roomInfo.streamID = QString::fromStdString(json["streamID"].get<std::string>());
        }
        if (json.contains("liveStreamID") && json["liveStreamID"].is_number_integer()) {
            roomInfo.liveStreamID = json["liveStreamID"].get<int>();
        }
        if (json.contains("endTime") && json["endTime"].is_number_integer()) {
            roomInfo.endTime = json["endTime"].get<int>();
        }
        if (json.contains("beginTime") && json["beginTime"].is_number_integer()) {
            roomInfo.beginTime = json["beginTime"].get<int>();
        }
        if (json.contains("receivedLikeCount") && json["receivedLikeCount"].is_number_integer()) {
            roomInfo.receivedLikeCount = json["receivedLikeCount"].get<int>();
        }
        if (json.contains("duration") && json["duration"].is_number_integer()) {
            roomInfo.duration = json["duration"].get<int>();
        }
        if (json.contains("viewerCount") && json["viewerCount"].is_number_integer()) {
            roomInfo.viewerCount = json["viewerCount"].get<int>();
        }
        if (json.contains("totalViewTime") && json["totalViewTime"].is_number_integer()) {
            roomInfo.totalViewTime = json["totalViewTime"].get<int>();
        }
        if (json.contains("liveViewerCount") && json["liveViewerCount"].is_number_integer()) {
            roomInfo.liveViewerCount = json["liveViewerCount"].get<int>();
        }
        if (json.contains("audioOnly") && json["audioOnly"].is_number_integer()) {
            roomInfo.audioOnly = json["audioOnly"].get<int>();
        }
        if (json.contains("locationName") && json["locationName"].is_string()) {
            roomInfo.locationName = QString::fromStdString(json["locationName"].get<std::string>());
        }
        if (json.contains("coverPhoto") && json["coverPhoto"].is_string()) {
            roomInfo.coverPhoto = QString::fromStdString(json["coverPhoto"].get<std::string>());
        }
        if (json.contains("latitude") && json["latitude"].is_number()) {
            roomInfo.latitude = json["latitude"].get<double>();
        }
        if (json.contains("longitude") && json["longitude"].is_number()) {
            roomInfo.longitude = json["longitude"].get<double>();
        }

        // Room settings
        if (json.contains("shareLocation") && json["shareLocation"].is_number_integer()) {
            roomInfo.shareLocation = json["shareLocation"].get<int>();
        }
        if (json.contains("followerOnlyChat") && json["followerOnlyChat"].is_number_integer()) {
            roomInfo.followerOnlyChat = json["followerOnlyChat"].get<int>();
        }
        if (json.contains("chatAvailable") && json["chatAvailable"].is_number_integer()) {
            roomInfo.chatAvailable = json["chatAvailable"].get<int>();
        }
        if (json.contains("replayCount") && json["replayCount"].is_number_integer()) {
            roomInfo.replayCount = json["replayCount"].get<int>();
        }
        if (json.contains("replayAvailable") && json["replayAvailable"].is_number_integer()) {
            roomInfo.replayAvailable = json["replayAvailable"].get<int>();
        }
        if (json.contains("numberOfChunks") && json["numberOfChunks"].is_number_integer()) {
            roomInfo.numberOfChunks = json["numberOfChunks"].get<int>();
        }
        if (json.contains("canSendGift") && json["canSendGift"].is_number_integer()) {
            roomInfo.canSendGift = json["canSendGift"].get<int>();
        }

        // Parse user information
        if (json.contains("userInfo") && json["userInfo"].is_object()) {
            JsonToOneSevenLiveUserInfo(json["userInfo"], roomInfo.userInfo);
        }

        // Other settings
        if (json.contains("landscape") && json["landscape"].is_boolean()) {
            roomInfo.landscape = json["landscape"].get<bool>();
        }
        if (json.contains("mute") && json["mute"].is_boolean()) {
            roomInfo.mute = json["mute"].get<bool>();
        }
        if (json.contains("birthdayState") && json["birthdayState"].is_number_integer()) {
            roomInfo.birthdayState = json["birthdayState"].get<int>();
        }
        if (json.contains("dayBeforeBirthday") && json["dayBeforeBirthday"].is_number_integer()) {
            roomInfo.dayBeforeBirthday = json["dayBeforeBirthday"].get<int>();
        }
        if (json.contains("achievementValue") && json["achievementValue"].is_number_integer()) {
            roomInfo.achievementValue = json["achievementValue"].get<int>();
        }
        if (json.contains("mediaMessageReadState") &&
            json["mediaMessageReadState"].is_number_integer()) {
            roomInfo.mediaMessageReadState = json["mediaMessageReadState"].get<int>();
        }
        if (json.contains("region") && json["region"].is_string()) {
            roomInfo.region = QString::fromStdString(json["region"].get<std::string>());
        }
        if (json.contains("device") && json["device"].is_string()) {
            roomInfo.device = QString::fromStdString(json["device"].get<std::string>());
        }

        // Parse event list
        if (json.contains("eventList") && json["eventList"].is_array()) {
            JsonToOneSevenLiveEventList(json["eventList"], roomInfo.eventList);
        }

        // Parse archive configuration
        if (json.contains("archiveConfig") && json["archiveConfig"].is_object()) {
            JsonToOneSevenLiveArchiveConfig(json["archiveConfig"], roomInfo.archiveConfig);
        }

        // Archive ID and game marquee settings
        if (json.contains("archiveID") && json["archiveID"].is_string()) {
            roomInfo.archiveID = QString::fromStdString(json["archiveID"].get<std::string>());
        }
        if (json.contains("hideGameMarquee") && json["hideGameMarquee"].is_boolean()) {
            roomInfo.hideGameMarquee = json["hideGameMarquee"].get<bool>();
        }
        if (json.contains("enableOBSGroupCall") && json["enableOBSGroupCall"].is_boolean()) {
            roomInfo.enableOBSGroupCall = json["enableOBSGroupCall"].get<bool>();
        }

        // Parse subtabs
        if (json.contains("subtabs") && json["subtabs"].is_array()) {
            for (const auto &subtabJson : json["subtabs"]) {
                if (subtabJson.is_string()) {
                    roomInfo.subtabs.append(QString::fromStdString(subtabJson.get<std::string>()));
                }
            }
        }

        // Parse last used hashtags
        if (json.contains("lastUsedHashtags") && json["lastUsedHashtags"].is_array()) {
            JsonToOneSevenLiveHashtags(json["lastUsedHashtags"], roomInfo.lastUsedHashtags);
        }

    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveRoomInfo error: %s", e.what());
        return false;
    }

    return true;
}

bool OneSevenLiveRoomInfoToJson(const OneSevenLiveRoomInfo &roomInfo, nlohmann::json &json) {
    try {
        nlohmann::json jsonObject;

        // Basic information
        jsonObject["userID"] = roomInfo.userID.toStdString();
        jsonObject["streamerType"] = roomInfo.streamerType;
        jsonObject["streamType"] = roomInfo.streamType.toStdString();
        jsonObject["status"] = roomInfo.status;
        jsonObject["caption"] = roomInfo.caption.toStdString();
        jsonObject["thumbnail"] = roomInfo.thumbnail.toStdString();

        // RTMP URLs
        nlohmann::json::array_t rtmpUrlsArray;
        for (const auto &rtmpUrl : roomInfo.rtmpUrls) {
            nlohmann::json rtmpUrlJson;
            rtmpUrlJson["provider"] = rtmpUrl.provider;
            rtmpUrlJson["streamType"] = rtmpUrl.streamType.toStdString();
            rtmpUrlJson["url"] = rtmpUrl.url.toStdString();
            rtmpUrlJson["urlLowQuality"] = rtmpUrl.urlLowQuality.toStdString();
            rtmpUrlJson["webUrl"] = rtmpUrl.webUrl.toStdString();
            rtmpUrlJson["webUrlLowQuality"] = rtmpUrl.webUrlLowQuality.toStdString();
            rtmpUrlJson["urlHighQuality"] = rtmpUrl.urlHighQuality.toStdString();
            rtmpUrlJson["weight"] = rtmpUrl.weight;
            rtmpUrlJson["throttle"] = rtmpUrl.throttle;
            rtmpUrlsArray.push_back(rtmpUrlJson);
        }
        jsonObject["rtmpUrls"] = rtmpUrlsArray;

        // Pull URLs Info
        nlohmann::json pullUrlsInfoObject;
        pullUrlsInfoObject["seqNo"] = static_cast<int>(roomInfo.pullURLsInfo.seqNo);
        nlohmann::json::array_t pullRtmpUrlsArray;
        for (const auto &rtmpUrl : roomInfo.pullURLsInfo.rtmpURLs) {
            nlohmann::json rtmpUrlJson;
            rtmpUrlJson["provider"] = rtmpUrl.provider;
            rtmpUrlJson["streamType"] = rtmpUrl.streamType.toStdString();
            rtmpUrlJson["url"] = rtmpUrl.url.toStdString();
            rtmpUrlJson["urlLowQuality"] = rtmpUrl.urlLowQuality.toStdString();
            rtmpUrlJson["webUrl"] = rtmpUrl.webUrl.toStdString();
            rtmpUrlJson["webUrlLowQuality"] = rtmpUrl.webUrlLowQuality.toStdString();
            rtmpUrlJson["urlHighQuality"] = rtmpUrl.urlHighQuality.toStdString();
            rtmpUrlJson["weight"] = rtmpUrl.weight;
            rtmpUrlJson["throttle"] = rtmpUrl.throttle;
            pullRtmpUrlsArray.push_back(rtmpUrlJson);
        }
        pullUrlsInfoObject["rtmpURLs"] = pullRtmpUrlsArray;
        jsonObject["pullURLsInfo"] = pullUrlsInfoObject;

        // Live streaming information
        jsonObject["allowCallin"] = roomInfo.allowCallin;
        jsonObject["restreamerOpenID"] = roomInfo.restreamerOpenID.toStdString();
        jsonObject["streamID"] = roomInfo.streamID.toStdString();
        jsonObject["liveStreamID"] = static_cast<int>(roomInfo.liveStreamID);
        jsonObject["endTime"] = static_cast<int>(roomInfo.endTime);
        jsonObject["beginTime"] = static_cast<int>(roomInfo.beginTime);
        jsonObject["receivedLikeCount"] = static_cast<int>(roomInfo.receivedLikeCount);
        jsonObject["duration"] = roomInfo.duration;
        jsonObject["viewerCount"] = roomInfo.viewerCount;
        jsonObject["totalViewTime"] = static_cast<int>(roomInfo.totalViewTime);
        jsonObject["liveViewerCount"] = roomInfo.liveViewerCount;
        jsonObject["audioOnly"] = roomInfo.audioOnly;
        jsonObject["locationName"] = roomInfo.locationName.toStdString();
        jsonObject["coverPhoto"] = roomInfo.coverPhoto.toStdString();
        jsonObject["latitude"] = roomInfo.latitude;
        jsonObject["longitude"] = roomInfo.longitude;

        // Room settings
        jsonObject["shareLocation"] = roomInfo.shareLocation;
        jsonObject["followerOnlyChat"] = roomInfo.followerOnlyChat;
        jsonObject["chatAvailable"] = roomInfo.chatAvailable;
        jsonObject["replayCount"] = roomInfo.replayCount;
        jsonObject["replayAvailable"] = roomInfo.replayAvailable;
        jsonObject["numberOfChunks"] = roomInfo.numberOfChunks;
        jsonObject["canSendGift"] = roomInfo.canSendGift;

        // User information
        nlohmann::json userInfoObject;
        userInfoObject["userID"] = roomInfo.userInfo.userID.toStdString();
        userInfoObject["openID"] = roomInfo.userInfo.openID.toStdString();
        userInfoObject["displayName"] = roomInfo.userInfo.displayName.toStdString();
        userInfoObject["gender"] = roomInfo.userInfo.gender.toStdString();
        userInfoObject["isChoice"] = roomInfo.userInfo.isChoice;
        userInfoObject["isInternational"] = roomInfo.userInfo.isInternational;
        userInfoObject["adsOn"] = roomInfo.userInfo.adsOn;
        userInfoObject["experience"] = roomInfo.userInfo.experience;
        userInfoObject["deviceType"] = roomInfo.userInfo.deviceType.toStdString();
        userInfoObject["picture"] = roomInfo.userInfo.picture.toStdString();

        // Glory road information
        nlohmann::json gloryroadInfoObject;
        gloryroadInfoObject["point"] = roomInfo.userInfo.gloryroadInfo.point;
        gloryroadInfoObject["level"] = roomInfo.userInfo.gloryroadInfo.level;
        gloryroadInfoObject["iconURL"] = roomInfo.userInfo.gloryroadInfo.iconURL.toStdString();
        gloryroadInfoObject["badgeIconURL"] =
            roomInfo.userInfo.gloryroadInfo.badgeIconURL.toStdString();
        userInfoObject["gloryroadInfo"] = gloryroadInfoObject;
        jsonObject["userInfo"] = userInfoObject;

        // Other settings
        jsonObject["landscape"] = roomInfo.landscape;
        jsonObject["mute"] = roomInfo.mute;
        jsonObject["birthdayState"] = roomInfo.birthdayState;
        jsonObject["dayBeforeBirthday"] = roomInfo.dayBeforeBirthday;
        jsonObject["achievementValue"] = roomInfo.achievementValue;
        jsonObject["mediaMessageReadState"] = roomInfo.mediaMessageReadState;
        jsonObject["region"] = roomInfo.region.toStdString();
        jsonObject["device"] = roomInfo.device.toStdString();

        // Activity list
        nlohmann::json::array_t eventListArray;
        for (const auto &eventInfo : roomInfo.eventList) {
            nlohmann::json eventJson;
            eventJson["ID"] = static_cast<int>(eventInfo.ID);
            eventJson["type"] = eventInfo.type;
            eventJson["icon"] = eventInfo.icon.toStdString();
            eventJson["endTime"] = static_cast<int>(eventInfo.endTime);
            eventJson["showTimer"] = eventInfo.showTimer;
            eventJson["name"] = eventInfo.name.toStdString();
            eventJson["URL"] = eventInfo.URL.toStdString();
            eventJson["pageSize"] = eventInfo.pageSize;
            eventJson["webViewTitle"] = eventInfo.webViewTitle.toStdString();

            nlohmann::json::array_t iconsArray;
            for (const auto &icon : eventInfo.icons) {
                nlohmann::json iconJson;
                iconJson["language"] = icon.language.toStdString();
                iconJson["value"] = icon.value.toStdString();
                iconsArray.push_back(iconJson);
            }
            eventJson["icons"] = iconsArray;
            eventListArray.push_back(eventJson);
        }
        jsonObject["eventList"] = eventListArray;

        // Archive configuration
        nlohmann::json archiveConfigObject;
        archiveConfigObject["autoRecording"] = roomInfo.archiveConfig.autoRecording;
        archiveConfigObject["autoPublish"] = roomInfo.archiveConfig.autoPublish;
        archiveConfigObject["clipPermission"] = roomInfo.archiveConfig.clipPermission;
        archiveConfigObject["clipPermissionDownload"] =
            roomInfo.archiveConfig.clipPermissionDownload;
        jsonObject["archiveConfig"] = archiveConfigObject;

        // Archive ID and game marquee settings
        jsonObject["archiveID"] = roomInfo.archiveID.toStdString();
        jsonObject["hideGameMarquee"] = roomInfo.hideGameMarquee;
        jsonObject["enableOBSGroupCall"] = roomInfo.enableOBSGroupCall;

        nlohmann::json::array_t subtabsArray;
        for (const auto &subtab : roomInfo.subtabs) {
            subtabsArray.push_back(subtab.toStdString());
        }
        jsonObject["subtabs"] = subtabsArray;

        nlohmann::json::array_t lastUsedHashtagsArray;
        for (const auto &hashtag : roomInfo.lastUsedHashtags) {
            nlohmann::json hashtagJson;
            hashtagJson["text"] = hashtag.text.toStdString();
            hashtagJson["isOfficial"] = hashtag.isOfficial;
            lastUsedHashtagsArray.push_back(hashtagJson);
        }
        jsonObject["lastUsedHashtags"] = lastUsedHashtagsArray;

        json = jsonObject;
        return true;

    } catch (const std::exception &e) {
        // You can add logging here, for example using obs_log
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveRoomInfoToJson error: %s", e.what());
        return false;
    }
}

bool OneSevenLiveRtmpRequestToJson(const OneSevenLiveRtmpRequest &request, nlohmann::json &json) {
    // Create archive configuration JSON object
    nlohmann::json archiveConfig = {{"autoRecording", request.archiveConfig.autoRecording},
                                    {"autoPublish", request.archiveConfig.autoPublish},
                                    {"clipPermission", request.archiveConfig.clipPermission}};

    nlohmann::json armyOnly = {{"enable", request.armyOnly.enable},
                               {"requiredArmyRank", request.armyOnly.requiredArmyRank},
                               {"showOnHotPage", request.armyOnly.showOnHotPage},
                               {"armyOnlyPN", request.armyOnly.armyOnlyPN}};

    // Create virtual streamer information JSON object
    nlohmann::json vliverInfo = {{"vliverModel", request.vliverInfo.vliverModel}};

    // Convert hashtags to Json array
    nlohmann::json::array_t hashtagsArray;
    for (const QString &tag : request.hashtags) {
        hashtagsArray.push_back(tag.toStdString());
    }

    // Create main JSON object
    json = {{"userID", request.userID.toStdString()},
            {"caption", request.caption.toStdString()},
            {"device", request.device.toStdString()},
            {"eventID", static_cast<int>(request.eventID)},
            {"hashtags", hashtagsArray},
            {"landscape", request.landscape},
            {"streamerType", request.streamerType},
            {"subtabID", request.subtabID.toStdString()},
            {"archiveConfig", archiveConfig},
            {"vliverInfo", vliverInfo},
            {"armyOnly", armyOnly},
            {"enableOBSGroupCall", request.enableOBSGroupCall}};

    return true;
}

bool JsonToOneSevenLiveRtmpRequest(const nlohmann::json &json, OneSevenLiveRtmpRequest &request) {
    if (!json.is_object()) {
        return false;
    }
    // Basic information
    if (json.contains("userID") && json["userID"].is_string()) {
        request.userID = QString::fromStdString(json["userID"].get<std::string>());
    }
    if (json.contains("caption") && json["caption"].is_string()) {
        request.caption = QString::fromStdString(json["caption"].get<std::string>());
    }
    if (json.contains("device") && json["device"].is_string()) {
        request.device = QString::fromStdString(json["device"].get<std::string>());
    }
    if (json.contains("eventID") && json["eventID"].is_number_integer()) {
        request.eventID = json["eventID"].get<int>();
    }
    // hashtags
    if (json.contains("hashtags") && json["hashtags"].is_array()) {
        for (const auto &tagJson : json["hashtags"]) {
            if (tagJson.is_string()) {
                request.hashtags.append(QString::fromStdString(tagJson.get<std::string>()));
            }
        }
    }
    if (json.contains("landscape") && json["landscape"].is_boolean()) {
        request.landscape = json["landscape"].get<bool>();
    }
    if (json.contains("streamerType") && json["streamerType"].is_number_integer()) {
        request.streamerType = json["streamerType"].get<int>();
    }
    if (json.contains("subtabID") && json["subtabID"].is_string()) {
        request.subtabID = QString::fromStdString(json["subtabID"].get<std::string>());
    }
    // archiveConfig
    if (json.contains("archiveConfig") && json["archiveConfig"].is_object()) {
        const auto &archiveConfigJson = json["archiveConfig"];
        if (archiveConfigJson.contains("autoRecording") &&
            archiveConfigJson["autoRecording"].is_boolean()) {
            request.archiveConfig.autoRecording = archiveConfigJson["autoRecording"].get<bool>();
        }
        if (archiveConfigJson.contains("autoPublish") &&
            archiveConfigJson["autoPublish"].is_boolean()) {
            request.archiveConfig.autoPublish = archiveConfigJson["autoPublish"].get<bool>();
        }
        if (archiveConfigJson.contains("clipPermission") &&
            archiveConfigJson["clipPermission"].is_number_integer()) {
            request.archiveConfig.clipPermission = archiveConfigJson["clipPermission"].get<int>();
        }
    }
    // vliverInfo
    if (json.contains("vliverInfo") && json["vliverInfo"].is_object()) {
        const auto &vliverInfoJson = json["vliverInfo"];
        if (vliverInfoJson.contains("vliverModel") &&
            vliverInfoJson["vliverModel"].is_number_integer()) {
            request.vliverInfo.vliverModel = vliverInfoJson["vliverModel"].get<int>();
        }
    }
    // armyOnly
    if (json.contains("armyOnly") && json["armyOnly"].is_object()) {
        const auto &armyOnlyJson = json["armyOnly"];
        if (armyOnlyJson.contains("enable") && armyOnlyJson["enable"].is_boolean()) {
            request.armyOnly.enable = armyOnlyJson["enable"].get<bool>();
        }
        if (armyOnlyJson.contains("requiredArmyRank") &&
            armyOnlyJson["requiredArmyRank"].is_number_integer()) {
            request.armyOnly.requiredArmyRank = armyOnlyJson["requiredArmyRank"].get<int>();
        }
        if (armyOnlyJson.contains("showOnHotPage") && armyOnlyJson["showOnHotPage"].is_boolean()) {
            request.armyOnly.showOnHotPage = armyOnlyJson["showOnHotPage"].get<bool>();
        }
        if (armyOnlyJson.contains("armyOnlyPN") && armyOnlyJson["armyOnlyPN"].is_boolean()) {
            request.armyOnly.armyOnlyPN = armyOnlyJson["armyOnlyPN"].get<bool>();
        }
    }
    // enableOBSGroupCall
    if (json.contains("enableOBSGroupCall") && json["enableOBSGroupCall"].is_boolean()) {
        request.enableOBSGroupCall = json["enableOBSGroupCall"].get<bool>();
    }
    return true;
}

bool OneSevenLiveStreamInfoToJson(const OneSevenLiveStreamInfo &streamInfo, nlohmann::json &json) {
    nlohmann::json jsonRequest;
    if (!OneSevenLiveRtmpRequestToJson(streamInfo.request, jsonRequest)) {
        return false;
    }
    json = {{"request", jsonRequest},
            {"categoryName", streamInfo.categoryName.toStdString()},
            {"createdAt", streamInfo.createdAt.toString(Qt::ISODate).toStdString()},
            {"streamUuid", streamInfo.streamUuid.toStdString()}};
    return true;
}

bool JsonToOneSevenLiveStreamInfo(const nlohmann::json &json, OneSevenLiveStreamInfo &streamInfo) {
    if (!json.is_object()) {
        return false;
    }
    // Basic information
    if (json.contains("categoryName") && json["categoryName"].is_string()) {
        streamInfo.categoryName = QString::fromStdString(json["categoryName"].get<std::string>());
    }
    if (json.contains("createdAt") && json["createdAt"].is_string()) {
        streamInfo.createdAt = QDateTime::fromString(
            QString::fromStdString(json["createdAt"].get<std::string>()), Qt::ISODate);
    }
    if (json.contains("streamUuid") && json["streamUuid"].is_string()) {
        streamInfo.streamUuid = QString::fromStdString(json["streamUuid"].get<std::string>());
    }
    // Handle request object
    if (json.contains("request") && json["request"].is_object()) {
        if (!JsonToOneSevenLiveRtmpRequest(json["request"], streamInfo.request)) {
            return false;
        }
    }
    return true;
}

bool JsonToOneSevenLiveRtmpResponse(const nlohmann::json &json,
                                    OneSevenLiveRtmpResponse &response) {
    if (!json.is_object()) {
        return false;
    }

    // Basic information
    if (json.contains("liveStreamID") && json["liveStreamID"].is_string()) {
        response.liveStreamID = QString::fromStdString(json["liveStreamID"].get<std::string>());
    }
    if (json.contains("streamID") && json["streamID"].is_string()) {
        response.streamID = QString::fromStdString(json["streamID"].get<std::string>());
    }
    if (json.contains("rtmpURL") && json["rtmpURL"].is_string()) {
        response.rtmpURL = QString::fromStdString(json["rtmpURL"].get<std::string>());
    }
    if (json.contains("rtmpProvider") && json["rtmpProvider"].is_string()) {
        response.rtmpProvider = QString::fromStdString(json["rtmpProvider"].get<std::string>());
    }
    if (json.contains("messageProvider") && json["messageProvider"].is_number_integer()) {
        response.messageProvider = json["messageProvider"].get<int>();
    }

    // firstStreamInfo is empty object, assign directly
    if (json.contains("firstStreamInfo")) {
        response.firstStreamInfo = json["firstStreamInfo"];
    }

    // Handle rtmpURLs array
    if (json.contains("rtmpURLs") && json["rtmpURLs"].is_array()) {
        for (const auto &urlJson : json["rtmpURLs"]) {
            if (urlJson.is_object()) {
                OneSevenLiveRtmpUrl rtmpUrl;
                if (urlJson.contains("provider") && urlJson["provider"].is_number_integer()) {
                    rtmpUrl.provider = urlJson["provider"].get<int>();
                }
                if (urlJson.contains("streamType") && urlJson["streamType"].is_string()) {
                    rtmpUrl.streamType =
                        QString::fromStdString(urlJson["streamType"].get<std::string>());
                }
                if (urlJson.contains("url") && urlJson["url"].is_string()) {
                    rtmpUrl.url = QString::fromStdString(urlJson["url"].get<std::string>());
                }
                if (urlJson.contains("urlLowQuality") && urlJson["urlLowQuality"].is_string()) {
                    rtmpUrl.urlLowQuality =
                        QString::fromStdString(urlJson["urlLowQuality"].get<std::string>());
                }
                if (urlJson.contains("webUrl") && urlJson["webUrl"].is_string()) {
                    rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].get<std::string>());
                }
                if (urlJson.contains("webUrlLowQuality") &&
                    urlJson["webUrlLowQuality"].is_string()) {
                    rtmpUrl.webUrlLowQuality =
                        QString::fromStdString(urlJson["webUrlLowQuality"].get<std::string>());
                }
                if (urlJson.contains("urlHighQuality") && urlJson["urlHighQuality"].is_string()) {
                    rtmpUrl.urlHighQuality =
                        QString::fromStdString(urlJson["urlHighQuality"].get<std::string>());
                }
                if (urlJson.contains("weight") && urlJson["weight"].is_number_integer()) {
                    rtmpUrl.weight = urlJson["weight"].get<int>();
                }
                if (urlJson.contains("throttle") && urlJson["throttle"].is_boolean()) {
                    rtmpUrl.throttle = urlJson["throttle"].get<bool>();
                }
                response.rtmpURLs.append(rtmpUrl);
            }
        }
    }

    // Handle achievement value status
    if (json.contains("achievementValueState") && json["achievementValueState"].is_object()) {
        const auto &achievementValueStateJson = json["achievementValueState"];
        if (achievementValueStateJson.contains("isValueCarryOver") &&
            achievementValueStateJson["isValueCarryOver"].is_boolean()) {
            response.achievementValueState.isValueCarryOver =
                achievementValueStateJson["isValueCarryOver"].get<bool>();
        }
        if (achievementValueStateJson.contains("initSeconds") &&
            achievementValueStateJson["initSeconds"].is_number_integer()) {
            response.achievementValueState.initSeconds =
                achievementValueStateJson["initSeconds"].get<int>();
        }
    }

    // Subtitle enable status
    if (json.contains("subtitleEnabled") && json["subtitleEnabled"].is_boolean()) {
        response.subtitleEnabled = json["subtitleEnabled"].get<bool>();
    }

    // Handle WHIP information
    if (json.contains("WHIP") && json["WHIP"].is_object()) {
        const auto &whipJson = json["WHIP"];
        if (whipJson.contains("server") && whipJson["server"].is_string()) {
            response.whipInfo.server =
                QString::fromStdString(whipJson["server"].get<std::string>());
        }
        if (whipJson.contains("token") && whipJson["token"].is_string()) {
            response.whipInfo.token = QString::fromStdString(whipJson["token"].get<std::string>());
        }
    }

    return true;
}

bool OneSevenLiveCloseLiveRequestToJson(const OneSevenLiveCloseLiveRequest &request,
                                        nlohmann::json &json) {
    json = {{"reason", request.reason.toStdString()}, {"userID", request.userID.toStdString()}};

    return true;
}

// Helper function to parse event items from JSON
bool JsonToOneSevenLiveEventItems(const nlohmann::json &eventsJson,
                                  QList<OneSevenLiveEventItem> &events) {
    if (!eventsJson.is_array()) {
        return false;
    }

    for (const auto &eventItem : eventsJson) {
        if (eventItem.is_object()) {
            OneSevenLiveEventItem item;
            if (eventItem.contains("ID") && eventItem["ID"].is_number_integer()) {
                item.ID = eventItem["ID"].get<int>();
            }
            if (eventItem.contains("name") && eventItem["name"].is_string()) {
                item.name = QString::fromStdString(eventItem["name"].get<std::string>());
            }
            if (eventItem.contains("bannerURL") && eventItem["bannerURL"].is_string()) {
                item.bannerURL = QString::fromStdString(eventItem["bannerURL"].get<std::string>());
            }
            if (eventItem.contains("descriptionURL") && eventItem["descriptionURL"].is_string()) {
                item.descriptionURL =
                    QString::fromStdString(eventItem["descriptionURL"].get<std::string>());
            }
            if (eventItem.contains("endTime") && eventItem["endTime"].is_number_integer()) {
                item.endTime = eventItem["endTime"].get<int>();
            }

            // Parse tagIDs array
            if (eventItem.contains("tagIDs") && eventItem["tagIDs"].is_array()) {
                for (const auto &tagID : eventItem["tagIDs"]) {
                    if (tagID.is_string()) {
                        item.tagIDs.append(QString::fromStdString(tagID.get<std::string>()));
                    }
                }
            }

            events.append(item);
        }
    }
    return true;
}

// Helper function to parse event tags from JSON
bool JsonToOneSevenLiveEventTags(const nlohmann::json &tagsJson,
                                 QList<OneSevenLiveEventTag> &tags) {
    if (!tagsJson.is_array()) {
        return false;
    }

    for (const auto &tagItem : tagsJson) {
        if (tagItem.is_object()) {
            OneSevenLiveEventTag tag;
            if (tagItem.contains("ID") && tagItem["ID"].is_string()) {
                tag.ID = QString::fromStdString(tagItem["ID"].get<std::string>());
            }
            if (tagItem.contains("name") && tagItem["name"].is_string()) {
                tag.name = QString::fromStdString(tagItem["name"].get<std::string>());
            }
            tags.append(tag);
        }
    }
    return true;
}

// Helper function to parse event section from JSON
bool JsonToOneSevenLiveEventSection(const nlohmann::json &eventJson, OneSevenLiveEventList &event) {
    if (!eventJson.is_object()) {
        return false;
    }

    // Parse events array
    if (eventJson.contains("events") && eventJson["events"].is_array()) {
        JsonToOneSevenLiveEventItems(eventJson["events"], event.events);
    }

    if (eventJson.contains("notEligibleForAllEvents") &&
        eventJson["notEligibleForAllEvents"].is_boolean()) {
        event.notEligibleForAllEvents = eventJson["notEligibleForAllEvents"].get<bool>();
    }
    if (eventJson.contains("promotionIndex") && eventJson["promotionIndex"].is_number_integer()) {
        event.promotionIndex = eventJson["promotionIndex"].get<int>();
    }
    if (eventJson.contains("instructionURL") && eventJson["instructionURL"].is_string()) {
        event.instructionURL =
            QString::fromStdString(eventJson["instructionURL"].get<std::string>());
    }

    // Parse tags array
    if (eventJson.contains("tags") && eventJson["tags"].is_array()) {
        JsonToOneSevenLiveEventTags(eventJson["tags"], event.tags);
    }

    return true;
}

// Helper function to parse subtabs from JSON
bool JsonToOneSevenLiveSubtabs(const nlohmann::json &subtabsJson,
                               QList<OneSevenLiveSubtab> &subtabs) {
    if (!subtabsJson.is_array()) {
        return false;
    }

    for (const auto &subtabItem : subtabsJson) {
        if (subtabItem.is_object()) {
            OneSevenLiveSubtab subtab;
            if (subtabItem.contains("displayName") && subtabItem["displayName"].is_string()) {
                subtab.displayName =
                    QString::fromStdString(subtabItem["displayName"].get<std::string>());
            }
            if (subtabItem.contains("ID") && subtabItem["ID"].is_string()) {
                subtab.ID = QString::fromStdString(subtabItem["ID"].get<std::string>());
            }
            subtabs.append(subtab);
        }
    }
    return true;
}

bool JsonToOneSevenLiveConfigStreamer(const nlohmann::json &json,
                                      OneSevenLiveConfigStreamer &response) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Parse event section
        if (json["event"].is_object()) {
            JsonToOneSevenLiveEventSection(json["event"], response.event);
        }

        // Parse customEvent section
        if (json.contains("customEvent") && json["customEvent"].is_object()) {
            const auto &customEventJson = json["customEvent"];
            if (customEventJson.contains("endTime") &&
                customEventJson["endTime"].is_number_integer()) {
                response.customEvent.endTime = customEventJson["endTime"].get<int>();
            }
            if (customEventJson.contains("status") &&
                customEventJson["status"].is_number_integer()) {
                response.customEvent.status = customEventJson["status"].get<int>();
            }
        }

        // Parse boxGacha section
        if (json.contains("boxGacha") && json["boxGacha"].is_object()) {
            const auto &boxGachaJson = json["boxGacha"];
            if (boxGachaJson.contains("previousSettingStatus") &&
                boxGachaJson["previousSettingStatus"].is_boolean()) {
                response.boxGacha.previousSettingStatus =
                    boxGachaJson["previousSettingStatus"].get<bool>();
            }
            if (boxGachaJson.contains("availableEventID") &&
                boxGachaJson["availableEventID"].is_string()) {
                response.boxGacha.availableEventID =
                    QString::fromStdString(boxGachaJson["availableEventID"].get<std::string>());
            }
        }

        // Parse subtabs array
        if (json.contains("subtabs") && json["subtabs"].is_array()) {
            JsonToOneSevenLiveSubtabs(json["subtabs"], response.subtabs);
        }

        // Parse lastStreamState
        if (json.contains("lastStreamState") && json["lastStreamState"].is_object()) {
            const auto &lastStreamStateJson = json["lastStreamState"];
            OneSevenLiveStreamState lastStreamState;
            if (lastStreamStateJson.contains("vliverInfo") &&
                lastStreamStateJson["vliverInfo"].is_object()) {
                OneSevenLiveVliverInfo vliverInfo;
                if (lastStreamStateJson["vliverInfo"].contains("vliverModel") &&
                    lastStreamStateJson["vliverInfo"]["vliverModel"].is_number_integer()) {
                    vliverInfo.vliverModel =
                        lastStreamStateJson["vliverInfo"]["vliverModel"].get<int>();
                }
                lastStreamState.vliverInfo = vliverInfo;
            }
            response.lastStreamState = lastStreamState;
        }

        if (json.contains("hashtagSelectLimit") && json["hashtagSelectLimit"].is_number_integer()) {
            response.hashtagSelectLimit = json["hashtagSelectLimit"].get<int>();
        }
        if (json.contains("armyOnly") && json["armyOnly"].is_number_integer()) {
            response.armyOnly = json["armyOnly"].get<int>();
        }

        // Parse archive configuration
        if (json.contains("archiveConfig") && json["archiveConfig"].is_object()) {
            JsonToOneSevenLiveArchiveConfig(json["archiveConfig"], response.archiveConfig);
        }
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveConfigStreamer error: %s", e.what());
        return false;
    }

    return true;
}

// Helper function to create events array JSON
static nlohmann::json::array_t createEventsArrayJson(const QList<OneSevenLiveEventItem> &events) {
    nlohmann::json::array_t eventsArray;
    for (const auto &event : events) {
        // Create tagIDs array
        nlohmann::json::array_t tagIDsArray;
        for (const QString &tagID : event.tagIDs) {
            tagIDsArray.push_back(tagID.toStdString());
        }

        // Create single event object
        nlohmann::json eventJson = {{"ID", static_cast<int>(event.ID)},
                                    {"name", event.name.toStdString()},
                                    {"bannerURL", event.bannerURL.toStdString()},
                                    {"descriptionURL", event.descriptionURL.toStdString()},
                                    {"tagIDs", tagIDsArray},
                                    {"endTime", static_cast<int>(event.endTime)}};
        eventsArray.push_back(eventJson);
    }
    return eventsArray;
}

// Helper function to create tags array JSON
static nlohmann::json::array_t createTagsArrayJson(const QList<OneSevenLiveEventTag> &tags) {
    nlohmann::json::array_t tagsArray;
    for (const auto &tag : tags) {
        nlohmann::json tagJson = {{"ID", tag.ID.toStdString()}, {"name", tag.name.toStdString()}};
        tagsArray.push_back(tagJson);
    }
    return tagsArray;
}

// Helper function to create event section JSON
static nlohmann::json createEventSectionJson(const OneSevenLiveEventList &event) {
    nlohmann::json::array_t eventsArray = createEventsArrayJson(event.events);
    nlohmann::json::array_t tagsArray = createTagsArrayJson(event.tags);

    return {{"events", eventsArray},
            {"notEligibleForAllEvents", event.notEligibleForAllEvents},
            {"promotionIndex", event.promotionIndex},
            {"tags", tagsArray},
            {"instructionURL", event.instructionURL.toStdString()}};
}

// Helper function to create subtabs array JSON
static nlohmann::json::array_t createSubtabsArrayJson(const QList<OneSevenLiveSubtab> &subtabs) {
    nlohmann::json::array_t subtabsArray;
    for (const auto &subtab : subtabs) {
        nlohmann::json subtabJson = {{"displayName", subtab.displayName.toStdString()},
                                     {"ID", subtab.ID.toStdString()}};
        subtabsArray.push_back(subtabJson);
    }
    return subtabsArray;
}

// Helper function to create archive config JSON
static nlohmann::json createArchiveConfigJson(const OneSevenLiveArchiveConfig &archiveConfig) {
    nlohmann::json archiveConfigObject;
    archiveConfigObject["autoRecording"] = archiveConfig.autoRecording;
    archiveConfigObject["autoPublish"] = archiveConfig.autoPublish;
    archiveConfigObject["clipPermission"] = archiveConfig.clipPermission;
    archiveConfigObject["clipPermissionDownload"] = archiveConfig.clipPermissionDownload;
    return archiveConfigObject;
}

bool OneSevenLiveConfigStreamerToJson(const OneSevenLiveConfigStreamer &response,
                                      nlohmann::json &json) {
    try {
        // Create event section
        nlohmann::json eventJson = createEventSectionJson(response.event);

        // Create customEvent object
        nlohmann::json customEventJson = {
            {"endTime", static_cast<int>(response.customEvent.endTime)},
            {"status", response.customEvent.status}};

        // Create boxGacha object
        nlohmann::json boxGachaJson = {
            {"previousSettingStatus", response.boxGacha.previousSettingStatus},
            {"availableEventID", response.boxGacha.availableEventID.toStdString()}};

        // Create subtabs array
        nlohmann::json::array_t subtabsArray = createSubtabsArrayJson(response.subtabs);

        // Create lastStreamState object
        nlohmann::json lastStreamStateJson = {
            {"vliverInfo", {{"vliverModel", response.lastStreamState.vliverInfo.vliverModel}}}};

        // Create archive configuration
        nlohmann::json archiveConfigJson = createArchiveConfigJson(response.archiveConfig);

        // Create main JSON object
        json = {{"event", eventJson},
                {"customEvent", customEventJson},
                {"boxGacha", boxGachaJson},
                {"subtabs", subtabsArray},
                {"lastStreamState", lastStreamStateJson},
                {"hashtagSelectLimit", response.hashtagSelectLimit},
                {"armyOnly", response.armyOnly},
                {"archiveConfig", archiveConfigJson}};

    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveConfigStreamerToJson error: %s", e.what());
        return false;
    } catch (...) {
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveConfigStreamerToJson unknown error");
        return false;
    }

    return true;
}

bool JsonToOneSevenLiveAblyTokenResponse(const nlohmann::json &json,
                                         OneSevenLiveAblyTokenResponse &response) {
    if (!json.is_object()) {
        return false;
    }

    // Parse provider field
    if (json.contains("provider") && json["provider"].is_number_integer()) {
        response.provider = json["provider"].get<int>();
    } else {
        return false;
    }

    // Parse token field
    if (json.contains("token") && json["token"].is_string()) {
        response.token = QString::fromStdString(json["token"].get<std::string>());
    } else {
        return false;
    }

    // Parse channels array
    if (json.contains("channels") && json["channels"].is_array()) {
        response.channels.clear();
        for (const auto &channel : json["channels"]) {
            if (channel.is_string()) {
                response.channels.append(QString::fromStdString(channel.get<std::string>()));
            }
        }
    } else {
        return false;
    }

    return true;
}

bool OneSevenLiveAblyTokenResponseToJson(const OneSevenLiveAblyTokenResponse &response,
                                         nlohmann::json &json) {
    // Create channels array
    nlohmann::json::array_t channelsArray;
    for (const QString &channel : response.channels) {
        channelsArray.push_back(channel.toStdString());
    }

    // Create main JSON object
    json = {{"provider", response.provider},
            {"token", response.token.toStdString()},
            {"channels", channelsArray}};

    return true;
}

bool JsonToOneSevenLiveUserInfo(const nlohmann::json &json, OneSevenLiveUserInfo &userInfo) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Basic user information
        if (json.contains("userID") && json["userID"].is_string()) {
            userInfo.userID = QString::fromStdString(json["userID"].get<std::string>());
        }
        if (json.contains("openID") && json["openID"].is_string()) {
            userInfo.openID = QString::fromStdString(json["openID"].get<std::string>());
        }
        if (json.contains("displayName") && json["displayName"].is_string()) {
            userInfo.displayName = QString::fromStdString(json["displayName"].get<std::string>());
        }
        if (json.contains("name") && json["name"].is_string()) {
            userInfo.name = QString::fromStdString(json["name"].get<std::string>());
        }
        if (json.contains("bio") && json["bio"].is_string()) {
            userInfo.bio = QString::fromStdString(json["bio"].get<std::string>());
        }
        if (json.contains("picture") && json["picture"].is_string()) {
            userInfo.picture = QString::fromStdString(json["picture"].get<std::string>());
        }
        if (json.contains("website") && json["website"].is_string()) {
            userInfo.website = QString::fromStdString(json["website"].get<std::string>());
        }

        // Numeric fields
        if (json.contains("followerCount") && json["followerCount"].is_number_integer()) {
            userInfo.followerCount = json["followerCount"].get<int>();
        }
        if (json.contains("followingCount") && json["followingCount"].is_number_integer()) {
            userInfo.followingCount = json["followingCount"].get<int>();
        }
        if (json.contains("receivedLikeCount") && json["receivedLikeCount"].is_number_integer()) {
            userInfo.receivedLikeCount = json["receivedLikeCount"].get<int>();
        }
        if (json.contains("likeCount") && json["likeCount"].is_number_integer()) {
            userInfo.likeCount = json["likeCount"].get<int>();
        }
        if (json.contains("isFollowing") && json["isFollowing"].is_number_integer()) {
            userInfo.isFollowing = json["isFollowing"].get<int>();
        }
        if (json.contains("isNotif") && json["isNotif"].is_number_integer()) {
            userInfo.isNotif = json["isNotif"].get<int>();
        }
        if (json.contains("isBlocked") && json["isBlocked"].is_number_integer()) {
            userInfo.isBlocked = json["isBlocked"].get<int>();
        }

        // Timestamp fields
        if (json.contains("followTime") && json["followTime"].is_number()) {
            userInfo.followTime = static_cast<qint64>(json["followTime"].get<double>());
        }
        if (json.contains("followRequestTime") && json["followRequestTime"].is_number()) {
            userInfo.followRequestTime =
                static_cast<qint64>(json["followRequestTime"].get<double>());
        }
        if (json.contains("roomID") && json["roomID"].is_number()) {
            userInfo.roomID = static_cast<qint64>(json["roomID"].get<double>());
        }

        // Additional string fields
        if (json.contains("privacyMode") && json["privacyMode"].is_string()) {
            userInfo.privacyMode = QString::fromStdString(json["privacyMode"].get<std::string>());
        }
        if (json.contains("revenueShareIndicator") && json["revenueShareIndicator"].is_string()) {
            userInfo.revenueShareIndicator =
                QString::fromStdString(json["revenueShareIndicator"].get<std::string>());
        }
        if (json.contains("region") && json["region"].is_string()) {
            userInfo.region = QString::fromStdString(json["region"].get<std::string>());
        }
        if (json.contains("lastLiveRegion") && json["lastLiveRegion"].is_string()) {
            userInfo.lastLiveRegion =
                QString::fromStdString(json["lastLiveRegion"].get<std::string>());
        }
        if (json.contains("extIDAppleTransfer") && json["extIDAppleTransfer"].is_string()) {
            userInfo.extIDAppleTransfer =
                QString::fromStdString(json["extIDAppleTransfer"].get<std::string>());
        }
        if (json.contains("commentShadowColor") && json["commentShadowColor"].is_string()) {
            userInfo.commentShadowColor =
                QString::fromStdString(json["commentShadowColor"].get<std::string>());
        }

        // Additional numeric fields
        if (json.contains("ballerLevel") && json["ballerLevel"].is_number_integer()) {
            userInfo.ballerLevel = json["ballerLevel"].get<int>();
        }
        if (json.contains("postCount") && json["postCount"].is_number_integer()) {
            userInfo.postCount = json["postCount"].get<int>();
        }
        if (json.contains("isCelebrity") && json["isCelebrity"].is_number_integer()) {
            userInfo.isCelebrity = json["isCelebrity"].get<int>();
        }
        if (json.contains("baller") && json["baller"].is_number_integer()) {
            userInfo.baller = json["baller"].get<int>();
        }
        if (json.contains("level") && json["level"].is_number_integer()) {
            userInfo.level = json["level"].get<int>();
        }
        if (json.contains("followPrivacyMode") && json["followPrivacyMode"].is_number_integer()) {
            userInfo.followPrivacyMode = json["followPrivacyMode"].get<int>();
        }
        if (json.contains("clanStatus") && json["clanStatus"].is_number_integer()) {
            userInfo.clanStatus = json["clanStatus"].get<int>();
        }
        if (json.contains("hideAllPointToLeaderboard") &&
            json["hideAllPointToLeaderboard"].is_number_integer()) {
            userInfo.hideAllPointToLeaderboard = json["hideAllPointToLeaderboard"].get<int>();
        }
        if (json.contains("enableShop") && json["enableShop"].is_number_integer()) {
            userInfo.enableShop = json["enableShop"].get<int>();
        }
        if (json.contains("gloryroadMode") && json["gloryroadMode"].is_number_integer()) {
            userInfo.gloryroadMode = json["gloryroadMode"].get<int>();
        }
        if (json.contains("avatarOnboardingPhase") &&
            json["avatarOnboardingPhase"].is_number_integer()) {
            userInfo.avatarOnboardingPhase = json["avatarOnboardingPhase"].get<int>();
        }
        if (json.contains("isEmailVerified") && json["isEmailVerified"].is_number_integer()) {
            userInfo.isEmailVerified = json["isEmailVerified"].get<int>();
        }

        // Additional timestamp fields
        if (json.contains("lastLiveTimestamp") && json["lastLiveTimestamp"].is_number()) {
            userInfo.lastLiveTimestamp =
                static_cast<qint64>(json["lastLiveTimestamp"].get<double>());
        }
        if (json.contains("lastCreateLiveTimestamp") &&
            json["lastCreateLiveTimestamp"].is_number()) {
            userInfo.lastCreateLiveTimestamp =
                static_cast<qint64>(json["lastCreateLiveTimestamp"].get<double>());
        }

        // Boolean fields
        if (json.contains("streamerRecapEnable") && json["streamerRecapEnable"].is_boolean()) {
            userInfo.streamerRecapEnable = json["streamerRecapEnable"].get<bool>();
        }
        if (json.contains("newbieDisplayAllGiftTabsToast") &&
            json["newbieDisplayAllGiftTabsToast"].is_boolean()) {
            userInfo.newbieDisplayAllGiftTabsToast =
                json["newbieDisplayAllGiftTabsToast"].get<bool>();
        }
        if (json.contains("isUnderaged") && json["isUnderaged"].is_boolean()) {
            userInfo.isUnderaged = json["isUnderaged"].get<bool>();
        }
        if (json.contains("isFreePrivateMsgEnabled") &&
            json["isFreePrivateMsgEnabled"].is_boolean()) {
            userInfo.isFreePrivateMsgEnabled = json["isFreePrivateMsgEnabled"].get<bool>();
        }
        if (json.contains("isVliverOnlyModeEnabled") &&
            json["isVliverOnlyModeEnabled"].is_boolean()) {
            userInfo.isVliverOnlyModeEnabled = json["isVliverOnlyModeEnabled"].get<bool>();
        }

        // Array fields
        if (json.contains("badgeInfo") && json["badgeInfo"].is_array()) {
            userInfo.badgeInfo.clear();
            for (const auto &item : json["badgeInfo"]) {
                if (item.is_string()) {
                    userInfo.badgeInfo.append(QString::fromStdString(item.get<std::string>()));
                }
            }
        }

        if (json.contains("loyaltyInfo") && json["loyaltyInfo"].is_array()) {
            userInfo.loyaltyInfo.clear();
            for (const auto &item : json["loyaltyInfo"]) {
                if (item.is_string()) {
                    userInfo.loyaltyInfo.append(QString::fromStdString(item.get<std::string>()));
                }
            }
        }

        if (json.contains("lastUsedHashtags") && json["lastUsedHashtags"].is_array()) {
            userInfo.lastUsedHashtags.clear();
            for (const auto &item : json["lastUsedHashtags"]) {
                if (item.is_string()) {
                    userInfo.lastUsedHashtags.append(
                        QString::fromStdString(item.get<std::string>()));
                }
            }
        }

        if (json.contains("levelBadges") && json["levelBadges"].is_array()) {
            userInfo.levelBadges.clear();
            for (const auto &item : json["levelBadges"]) {
                if (item.is_string()) {
                    userInfo.levelBadges.append(QString::fromStdString(item.get<std::string>()));
                }
            }
        }

        // QVariantMap field
        if (json.contains("monthlyVIPBadges") && json["monthlyVIPBadges"].is_object()) {
            userInfo.monthlyVIPBadges.clear();
            for (const auto &item : json["monthlyVIPBadges"].items()) {
                const std::string &key = item.key();
                const nlohmann::json &value = item.value();
                if (value.is_string()) {
                    userInfo.monthlyVIPBadges[QString::fromStdString(key)] =
                        QString::fromStdString(value.get<std::string>());
                } else if (value.is_number()) {
                    userInfo.monthlyVIPBadges[QString::fromStdString(key)] = value.get<double>();
                } else if (value.is_boolean()) {
                    userInfo.monthlyVIPBadges[QString::fromStdString(key)] = value.get<bool>();
                }
            }
        }

        // OnliveInfo nested object
        OneSevenLiveOnliveInfo onliveInfo;
        if (json.contains("onliveInfo") && json["onliveInfo"].is_object()) {
            const auto &onliveInfoJson = json["onliveInfo"];
            if (onliveInfoJson.contains("premiumType") &&
                onliveInfoJson["premiumType"].is_number_integer()) {
                onliveInfo.premiumType = onliveInfoJson["premiumType"].get<int>();
            }
        }
        userInfo.onliveInfo = onliveInfo;

    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "Error parsing JSON: %s", e.what());
        return false;
    }

    return true;
}

bool JsonToOneSevenLiveConfig(const nlohmann::json &json, OneSevenLiveConfig &config) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Handle addOns object
        if (json.contains("addOns") && json["addOns"].is_object()) {
            const auto &addOnsJson = json["addOns"];
            // Handle features object
            if (addOnsJson.contains("features") && addOnsJson["features"].is_object()) {
                const auto &featuresJson = addOnsJson["features"];
                // Iterate through all key-value pairs in features object
                for (const auto &item : featuresJson.items()) {
                    const std::string &key = item.key();
                    if (item.value().is_number_integer()) {
                        const int value = item.value().get<int>();
                        config.addOns.features[QString::fromStdString(key)] = value;
                    }
                }
            }
        }
        return true;
    } catch (const std::exception &e) {
        // Log error
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveConfig error: %s", e.what());
        return false;
    }
}

bool OneSevenLiveConfigToJson(const OneSevenLiveConfig &config, nlohmann::json &json) {
    try {
        // Create features object
        nlohmann::json featuresObject;
        for (auto it = config.addOns.features.constBegin(); it != config.addOns.features.constEnd();
             ++it) {
            featuresObject[it.key().toStdString()] = it.value();
        }

        // Create addOns object
        nlohmann::json addOnsObject;
        addOnsObject["features"] = featuresObject;

        // Create main JSON object
        nlohmann::json jsonObject;
        jsonObject["addOns"] = addOnsObject;

        json = jsonObject;
        return true;
    } catch (const std::exception &e) {
        // Log error message
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveConfigToJson error: %s", e.what());
        return false;
    }
}

bool JsonToOneSevenLiveArmySubscriptionLevels(const nlohmann::json &json,
                                              OneSevenLiveArmySubscriptionLevels &levels) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Handle subscriptionLevels array
        if (json.contains("subscriptionLevels") && json["subscriptionLevels"].is_array()) {
            const auto &subscriptionLevelsJson = json["subscriptionLevels"];
            levels.subscriptionLevels.clear();

            for (const auto &levelJson : subscriptionLevelsJson) {
                if (!levelJson.is_object())
                    continue;

                OneSevenLiveArmySubscriptionLevel level;

                // Parse basic fields
                if (levelJson.contains("rank") && levelJson["rank"].is_number_integer()) {
                    level.rank = levelJson["rank"].get<int>();
                }
                if (levelJson.contains("subscribersAmount") &&
                    levelJson["subscribersAmount"].is_number_integer()) {
                    level.subscribersAmount = levelJson["subscribersAmount"].get<int>();
                }

                // Parse i18nToken object
                if (levelJson.contains("i18nToken") && levelJson["i18nToken"].is_object()) {
                    const auto &i18nTokenJson = levelJson["i18nToken"];
                    if (i18nTokenJson.contains("key") && i18nTokenJson["key"].is_string()) {
                        level.i18nToken.key =
                            QString::fromStdString(i18nTokenJson["key"].get<std::string>());
                    }

                    // Parse params array
                    if (i18nTokenJson.contains("params") && i18nTokenJson["params"].is_array()) {
                        const auto &paramsJson = i18nTokenJson["params"];
                        for (const auto &paramJson : paramsJson) {
                            if (paramJson.is_object() && paramJson.contains("value") &&
                                paramJson["value"].is_string()) {
                                OneSevenLiveI18nTokenParam param;
                                param.value =
                                    QString::fromStdString(paramJson["value"].get<std::string>());
                                level.i18nToken.params.append(param);
                            }
                        }
                    }
                }

                levels.subscriptionLevels.append(level);
            }
        }

        return true;
    } catch (const std::exception &e) {
        // Log error message
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveArmySubscriptionLevels error: %s",
                e.what());
        return false;
    }
}

bool OneSevenLiveArmySubscriptionLevelsToJson(const OneSevenLiveArmySubscriptionLevels &levels,
                                              nlohmann::json &json) {
    try {
        // Create subscriptionLevels array
        nlohmann::json::array_t subscriptionLevelsArray;

        for (const auto &level : levels.subscriptionLevels) {
            // Create params array
            nlohmann::json::array_t paramsArray;
            for (const auto &param : level.i18nToken.params) {
                nlohmann::json paramJson = {{"value", param.value.toStdString()}};
                paramsArray.push_back(paramJson);
            }

            // Create i18nToken object
            nlohmann::json i18nTokenJson;
            if (level.i18nToken.params.isEmpty()) {
                i18nTokenJson = {{"key", level.i18nToken.key.toStdString()}};
            } else {
                i18nTokenJson = {{"key", level.i18nToken.key.toStdString()},
                                 {"params", paramsArray}};
            }

            // Create level object
            nlohmann::json levelJson = {{"rank", level.rank},
                                        {"subscribersAmount", level.subscribersAmount},
                                        {"i18nToken", i18nTokenJson}};

            subscriptionLevelsArray.push_back(levelJson);
        }

        // Create main JSON object
        json = {{"subscriptionLevels", subscriptionLevelsArray}};

        return true;
    } catch (const std::exception &e) {
        // Log error message
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveArmySubscriptionLevelsToJson error: %s",
                e.what());
        return false;
    }
}

bool JsonToOneSevenLiveGiftTabsResponse(const nlohmann::json &json,
                                        OneSevenLiveGiftTabsResponse &response) {
    try {
        // Parse giftLastUpdate
        if (json.contains("giftLastUpdate") && json["giftLastUpdate"].is_number_integer()) {
            response.giftLastUpdate = json["giftLastUpdate"].get<int>();
        }

        // Parse tabs array
        if (json.contains("tabs") && json["tabs"].is_array()) {
            const auto &tabsArray = json["tabs"];
            for (const auto &tabItem : tabsArray) {
                if (!tabItem.is_object())
                    continue;

                OneSevenLiveGiftTab tab;
                if (tabItem.contains("id") && tabItem["id"].is_string()) {
                    tab.id = QString::fromStdString(tabItem["id"].get<std::string>());
                }
                if (tabItem.contains("type") && tabItem["type"].is_number_integer()) {
                    tab.type = tabItem["type"].get<int>();
                }
                if (tabItem.contains("name") && tabItem["name"].is_string()) {
                    tab.name = QString::fromStdString(tabItem["name"].get<std::string>());
                }

                // Parse gifts array
                if (tabItem.contains("gifts") && tabItem["gifts"].is_array()) {
                    const auto &giftsArray = tabItem["gifts"];
                    for (const auto &giftItem : giftsArray) {
                        if (!giftItem.is_object())
                            continue;

                        OneSevenLiveGift gift;
                        if (giftItem.contains("giftID") && giftItem["giftID"].is_string()) {
                            gift.giftID =
                                QString::fromStdString(giftItem["giftID"].get<std::string>());
                        }
                        if (giftItem.contains("isHidden") &&
                            giftItem["isHidden"].is_number_integer()) {
                            gift.isHidden = giftItem["isHidden"].get<int>();
                        }
                        if (giftItem.contains("regionMode") &&
                            giftItem["regionMode"].is_number_integer()) {
                            gift.regionMode = giftItem["regionMode"].get<int>();
                        }
                        if (giftItem.contains("name") && giftItem["name"].is_string()) {
                            gift.name = QString::fromStdString(giftItem["name"].get<std::string>());
                        }
                        if (giftItem.contains("point") && giftItem["point"].is_number_integer()) {
                            gift.point = giftItem["point"].get<int>();
                        }
                        if (giftItem.contains("leaderboardIcon") &&
                            giftItem["leaderboardIcon"].is_string()) {
                            gift.leaderboardIcon = QString::fromStdString(
                                giftItem["leaderboardIcon"].get<std::string>());
                        }
                        if (giftItem.contains("vffURL") && giftItem["vffURL"].is_string()) {
                            gift.vffURL =
                                QString::fromStdString(giftItem["vffURL"].get<std::string>());
                        }
                        if (giftItem.contains("vffMD5") && giftItem["vffMD5"].is_string()) {
                            gift.vffMD5 =
                                QString::fromStdString(giftItem["vffMD5"].get<std::string>());
                        }
                        if (giftItem.contains("vffJson") && giftItem["vffJson"].is_string()) {
                            gift.vffJson =
                                QString::fromStdString(giftItem["vffJson"].get<std::string>());
                        }

                        // Parse regions array
                        if (giftItem.contains("regions") && giftItem["regions"].is_array()) {
                            const auto &regionsArray = giftItem["regions"];
                            for (const auto &region : regionsArray) {
                                if (region.is_string()) {
                                    gift.regions.append(
                                        QString::fromStdString(region.get<std::string>()));
                                }
                            }
                        }

                        tab.gifts.append(gift);
                    }
                }

                response.tabs.append(tab);
            }
        }

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveGiftTabsResponse error: %s", e.what());
        return false;
    }
}

bool OneSevenLiveGiftTabsResponseToJson(const OneSevenLiveGiftTabsResponse &response,
                                        nlohmann::json &json) {
    try {
        // Create tabs array
        nlohmann::json::array_t tabsArray;
        for (const auto &tab : response.tabs) {
            // Create gifts array
            nlohmann::json::array_t giftsArray;
            for (const auto &gift : tab.gifts) {
                // Create regions array
                nlohmann::json::array_t regionsArray;
                for (const auto &region : gift.regions) {
                    regionsArray.push_back(region.toStdString());
                }

                nlohmann::json giftJson = {{"giftID", gift.giftID.toStdString()},
                                           {"isHidden", gift.isHidden},
                                           {"regionMode", gift.regionMode},
                                           {"name", gift.name.toStdString()},
                                           {"point", gift.point},
                                           {"leaderboardIcon", gift.leaderboardIcon.toStdString()},
                                           {"vffURL", gift.vffURL.toStdString()},
                                           {"vffMD5", gift.vffMD5.toStdString()},
                                           {"vffJson", gift.vffJson.toStdString()},
                                           {"regions", regionsArray}};
                giftsArray.push_back(giftJson);
            }

            nlohmann::json tabJson = {{"id", tab.id.toStdString()},
                                      {"type", tab.type},
                                      {"name", tab.name.toStdString()},
                                      {"gifts", giftsArray}};
            tabsArray.push_back(tabJson);
        }

        // Create main JSON object
        json = {{"giftLastUpdate", static_cast<int>(response.giftLastUpdate)}, {"tabs", tabsArray}};

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveGiftTabsResponseToJson error: %s", e.what());
        return false;
    }
}

bool JsonToOneSevenLiveGiftsResponse(const nlohmann::json &json,
                                     OneSevenLiveGiftsResponse &response) {
    try {
        // Parse lastUpdate
        if (json.contains("lastUpdate") && json["lastUpdate"].is_number_integer()) {
            response.lastUpdate = json["lastUpdate"].get<int>();
        }

        // Parse gifts array
        if (json.contains("gifts") && json["gifts"].is_array()) {
            const auto &giftsArray = json["gifts"];
            for (const auto &giftItem : giftsArray) {
                if (!giftItem.is_object())
                    continue;

                OneSevenLiveGift gift;
                if (giftItem.contains("giftID") && giftItem["giftID"].is_string()) {
                    gift.giftID = QString::fromStdString(giftItem["giftID"].get<std::string>());
                }
                if (giftItem.contains("isHidden") && giftItem["isHidden"].is_number_integer()) {
                    gift.isHidden = giftItem["isHidden"].get<int>();
                }
                if (giftItem.contains("regionMode") && giftItem["regionMode"].is_number_integer()) {
                    gift.regionMode = giftItem["regionMode"].get<int>();
                }
                if (giftItem.contains("name") && giftItem["name"].is_string()) {
                    gift.name = QString::fromStdString(giftItem["name"].get<std::string>());
                }
                if (giftItem.contains("point") && giftItem["point"].is_number_integer()) {
                    gift.point = giftItem["point"].get<int>();
                }
                if (giftItem.contains("leaderboardIcon") &&
                    giftItem["leaderboardIcon"].is_string()) {
                    gift.leaderboardIcon =
                        QString::fromStdString(giftItem["leaderboardIcon"].get<std::string>());
                }
                if (giftItem.contains("vffURL") && giftItem["vffURL"].is_string()) {
                    gift.vffURL = QString::fromStdString(giftItem["vffURL"].get<std::string>());
                }
                if (giftItem.contains("vffMD5") && giftItem["vffMD5"].is_string()) {
                    gift.vffMD5 = QString::fromStdString(giftItem["vffMD5"].get<std::string>());
                }
                if (giftItem.contains("vffJson") && giftItem["vffJson"].is_string()) {
                    gift.vffJson = QString::fromStdString(giftItem["vffJson"].get<std::string>());
                }

                // Parse regions array
                if (giftItem.contains("regions") && giftItem["regions"].is_array()) {
                    const auto &regionsArray = giftItem["regions"];
                    for (const auto &region : regionsArray) {
                        if (region.is_string()) {
                            gift.regions.append(QString::fromStdString(region.get<std::string>()));
                        }
                    }
                }

                response.gifts.append(gift);
            }
        }

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveGiftsResponse error: %s", e.what());
        return false;
    }
}

bool OneSevenLiveGiftsResponseToJson(const OneSevenLiveGiftsResponse &response,
                                     nlohmann::json &json) {
    try {
        // Create gifts array
        nlohmann::json::array_t giftsArray;
        for (const auto &gift : response.gifts) {
            // Create regions array
            nlohmann::json::array_t regionsArray;
            for (const auto &region : gift.regions) {
                regionsArray.push_back(region.toStdString());
            }

            nlohmann::json giftJson = {{"giftID", gift.giftID.toStdString()},
                                       {"isHidden", gift.isHidden},
                                       {"regionMode", gift.regionMode},
                                       {"name", gift.name.toStdString()},
                                       {"point", gift.point},
                                       {"leaderboardIcon", gift.leaderboardIcon.toStdString()},
                                       {"vffURL", gift.vffURL.toStdString()},
                                       {"vffMD5", gift.vffMD5.toStdString()},
                                       {"vffJson", gift.vffJson.toStdString()},
                                       {"regions", regionsArray}};
            giftsArray.push_back(giftJson);
        }

        // Create main JSON object
        json = {{"lastUpdate", static_cast<int>(response.lastUpdate)}, {"gifts", giftsArray}};

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveGiftsResponseToJson error: %s", e.what());
        return false;
    }
}

bool OneSevenLiveCustomEventToJson(const OneSevenLiveCustomEvent &request, nlohmann::json &json) {
    json = {
        {"eventName", request.eventName.toStdString()},
        {"description", request.description.toStdString()},
        {"endTime", static_cast<int>(request.endTime)},
        {"dailyGoalPoints", static_cast<int>(request.dailyGoalPoints)},
        {"goalPoints", static_cast<int>(request.goalPoints)},
        {"userID", request.userID.toStdString()},
    };

    // Add gift ID array
    nlohmann::json::array_t giftIDsJson;
    for (const auto &giftID : request.giftIDs) {
        giftIDsJson.push_back(giftID.toStdString());
    }
    json["giftIDs"] = giftIDsJson;

    return true;
}

bool OneSevenLiveChangeCustomEventStatusRequestToJson(
    const OneSevenLiveCustomEventStatusRequest &request, nlohmann::json &json) {
    json = {
        {"status", request.status},
        {"userID", request.userID.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveCustomEvent(const nlohmann::json &json, OneSevenLiveCustomEvent &response) {
    if (json.contains("eventID") && json["eventID"].is_string()) {
        response.eventID = QString::fromStdString(json["eventID"].get<std::string>());
    }
    if (json.contains("userID") && json["userID"].is_string()) {
        response.userID = QString::fromStdString(json["userID"].get<std::string>());
    }
    if (json.contains("status") && json["status"].is_number_integer()) {
        response.status = json["status"].get<int>();
    }
    if (json.contains("eventName") && json["eventName"].is_string()) {
        response.eventName = QString::fromStdString(json["eventName"].get<std::string>());
    }
    if (json.contains("description") && json["description"].is_string()) {
        response.description = QString::fromStdString(json["description"].get<std::string>());
    }
    if (json.contains("startTime") && json["startTime"].is_number_integer()) {
        response.startTime = json["startTime"].get<int>();
    }
    if (json.contains("endTime") && json["endTime"].is_number_integer()) {
        response.endTime = json["endTime"].get<int>();
    }
    if (json.contains("realEndTime") && json["realEndTime"].is_number_integer()) {
        response.realEndTime = json["realEndTime"].get<int>();
    }
    if (json.contains("isAchieved") && json["isAchieved"].is_boolean()) {
        response.isAchieved = json["isAchieved"].get<bool>();
    }
    if (json.contains("goalPoints") && json["goalPoints"].is_number_integer()) {
        response.goalPoints = json["goalPoints"].get<int>();
    }
    if (json.contains("dailyGoalPoints") && json["dailyGoalPoints"].is_number_integer()) {
        response.dailyGoalPoints = json["dailyGoalPoints"].get<int>();
    }
    if (json.contains("displayStatus") && json["displayStatus"].is_string()) {
        response.displayStatus = QString::fromStdString(json["displayStatus"].get<std::string>());
    }
    if (json.contains("currentGoalPoints") && json["currentGoalPoints"].is_number_integer()) {
        response.currentGoalPoints = json["currentGoalPoints"].get<int>();
    }
    if (json.contains("currentDailyGoalPoints") &&
        json["currentDailyGoalPoints"].is_number_integer()) {
        response.currentDailyGoalPoints = json["currentDailyGoalPoints"].get<int>();
    }

    // Process giftIDs array
    if (json.contains("giftIDs") && json["giftIDs"].is_array()) {
        const auto &giftIDsJson = json["giftIDs"];
        for (const auto &giftIDJson : giftIDsJson) {
            if (giftIDJson.is_string()) {
                response.giftIDs.append(QString::fromStdString(giftIDJson.get<std::string>()));
            }
        }
    }

    // Process gifts array
    if (json.contains("gifts") && json["gifts"].is_array()) {
        const auto &giftsJson = json["gifts"];
        for (const auto &giftJson : giftsJson) {
            if (!giftJson.is_object())
                continue;

            OneSevenLiveGift gift;
            if (giftJson.contains("giftID") && giftJson["giftID"].is_string()) {
                gift.giftID = QString::fromStdString(giftJson["giftID"].get<std::string>());
            }
            if (giftJson.contains("name") && giftJson["name"].is_string()) {
                gift.name = QString::fromStdString(giftJson["name"].get<std::string>());
            }
            if (giftJson.contains("point") && giftJson["point"].is_number_integer()) {
                gift.point = giftJson["point"].get<int>();
            }
            if (giftJson.contains("isHidden") && giftJson["isHidden"].is_number_integer()) {
                gift.isHidden = giftJson["isHidden"].get<int>();
            }
            if (giftJson.contains("regionMode") && giftJson["regionMode"].is_number_integer()) {
                gift.regionMode = giftJson["regionMode"].get<int>();
            }
            if (giftJson.contains("leaderboardIcon") && giftJson["leaderboardIcon"].is_string()) {
                gift.leaderboardIcon =
                    QString::fromStdString(giftJson["leaderboardIcon"].get<std::string>());
            }
            if (giftJson.contains("vffURL") && giftJson["vffURL"].is_string()) {
                gift.vffURL = QString::fromStdString(giftJson["vffURL"].get<std::string>());
            }
            if (giftJson.contains("vffMD5") && giftJson["vffMD5"].is_string()) {
                gift.vffMD5 = QString::fromStdString(giftJson["vffMD5"].get<std::string>());
            }
            if (giftJson.contains("vffJson") && giftJson["vffJson"].is_string()) {
                gift.vffJson = QString::fromStdString(giftJson["vffJson"].get<std::string>());
            }

            // Process regions array
            if (giftJson.contains("regions") && giftJson["regions"].is_array()) {
                const auto &regionsJson = giftJson["regions"];
                for (const auto &regionJson : regionsJson) {
                    if (regionJson.is_string()) {
                        gift.regions.append(QString::fromStdString(regionJson.get<std::string>()));
                    }
                }
            }

            response.gifts.append(gift);
        }
    }

    // Process rewards array
    if (json.contains("rewards") && json["rewards"].is_array()) {
        const auto &rewardsJson = json["rewards"];
        for (const auto &rewardJson : rewardsJson) {
            response.rewards.append(rewardJson);
        }
    }

    return true;
}

bool OneSevenLivePokeRequestToJson(const OneSevenLivePokeRequest &request, nlohmann::json &json) {
    json = {
        {"isPokeBack", request.isPokeBack},
        {"srcID", request.srcID.toStdString()},
        {"userID", request.userID.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLivePokeResponse(const nlohmann::json &json,
                                    OneSevenLivePokeResponse &response) {
    if (json.contains("pokeAnimationID") && json["pokeAnimationID"].is_string()) {
        response.pokeAnimationID =
            QString::fromStdString(json["pokeAnimationID"].get<std::string>());
    }
    return true;
}

bool OneSevenLivePokeAllRequestToJson(const OneSevenLivePokeAllRequest &request,
                                      nlohmann::json &json) {
    json = {
        {"liveStreamID", request.liveStreamID.toStdString()},
        {"receiverGroup", request.receiverGroup},
    };

    return true;
}
