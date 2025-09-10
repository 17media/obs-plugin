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
#include "json11.hpp"

using namespace json11;
using namespace std;

bool JsonToOneSevenLiveLoginData(const Json &json, OneSevenLiveLoginData &loginData) {
    if (!json.is_object()) {
        return false;
    }

    // Handle user information
    const auto &userInfoJson = json["userInfo"];
    if (userInfoJson.is_object()) {
        // Basic user information
        loginData.userInfo.userID = QString::fromStdString(userInfoJson["userID"].string_value());
        loginData.userInfo.openID = QString::fromStdString(userInfoJson["openID"].string_value());
        loginData.userInfo.displayName =
            QString::fromStdString(userInfoJson["displayName"].string_value());
        loginData.userInfo.name = QString::fromStdString(userInfoJson["name"].string_value());
        loginData.userInfo.bio = QString::fromStdString(userInfoJson["bio"].string_value());
        loginData.userInfo.picture = QString::fromStdString(userInfoJson["picture"].string_value());
        loginData.userInfo.website = QString::fromStdString(userInfoJson["website"].string_value());

        // Count information
        loginData.userInfo.followerCount = userInfoJson["followerCount"].int_value();
        loginData.userInfo.followingCount = userInfoJson["followingCount"].int_value();
        loginData.userInfo.receivedLikeCount = userInfoJson["receivedLikeCount"].int_value();
        loginData.userInfo.likeCount = userInfoJson["likeCount"].int_value();

        // Follow status
        loginData.userInfo.isFollowing = userInfoJson["isFollowing"].int_value();
        loginData.userInfo.isNotif = userInfoJson["isNotif"].int_value();
        loginData.userInfo.isBlocked = userInfoJson["isBlocked"].int_value();
        loginData.userInfo.followTime = userInfoJson["followTime"].int_value();
        loginData.userInfo.followRequestTime = userInfoJson["followRequestTime"].int_value();

        // Room and privacy settings
        loginData.userInfo.roomID = userInfoJson["roomID"].int_value();
        loginData.userInfo.privacyMode =
            QString::fromStdString(userInfoJson["privacyMode"].string_value());
        loginData.userInfo.followPrivacyMode = userInfoJson["followPrivacyMode"].int_value();

        // Level and status information
        loginData.userInfo.ballerLevel = userInfoJson["ballerLevel"].int_value();
        loginData.userInfo.postCount = userInfoJson["postCount"].int_value();
        loginData.userInfo.isCelebrity = userInfoJson["isCelebrity"].int_value();
        loginData.userInfo.baller = userInfoJson["baller"].int_value();
        loginData.userInfo.level = userInfoJson["level"].int_value();

        // Other attributes
        loginData.userInfo.revenueShareIndicator =
            QString::fromStdString(userInfoJson["revenueShareIndicator"].string_value());
        loginData.userInfo.clanStatus = userInfoJson["clanStatus"].int_value();
        loginData.userInfo.region = QString::fromStdString(userInfoJson["region"].string_value());
        loginData.userInfo.hideAllPointToLeaderboard =
            userInfoJson["hideAllPointToLeaderboard"].int_value();
        loginData.userInfo.enableShop = userInfoJson["enableShop"].int_value();

        // Timestamp information
        loginData.userInfo.lastLiveTimestamp = userInfoJson["lastLiveTimestamp"].int_value();
        loginData.userInfo.lastCreateLiveTimestamp =
            userInfoJson["lastCreateLiveTimestamp"].int_value();
        loginData.userInfo.lastLiveRegion =
            QString::fromStdString(userInfoJson["lastLiveRegion"].string_value());

        // Boolean attributes
        loginData.userInfo.streamerRecapEnable = userInfoJson["streamerRecapEnable"].bool_value();
        loginData.userInfo.newbieDisplayAllGiftTabsToast =
            userInfoJson["newbieDisplayAllGiftTabsToast"].bool_value();
        loginData.userInfo.isUnderaged = userInfoJson["isUnderaged"].bool_value();
        loginData.userInfo.isFreePrivateMsgEnabled =
            userInfoJson["isFreePrivateMsgEnabled"].bool_value();
        loginData.userInfo.isVliverOnlyModeEnabled =
            userInfoJson["isVliverOnlyModeEnabled"].bool_value();

        // Integer attributes
        loginData.userInfo.gloryroadMode = userInfoJson["gloryroadMode"].int_value();
        loginData.userInfo.avatarOnboardingPhase =
            userInfoJson["avatarOnboardingPhase"].int_value();
        loginData.userInfo.isEmailVerified = userInfoJson["isEmailVerified"].int_value();

        // String attributes
        loginData.userInfo.extIDAppleTransfer =
            QString::fromStdString(userInfoJson["extIDAppleTransfer"].string_value());
        loginData.userInfo.commentShadowColor =
            QString::fromStdString(userInfoJson["commentShadowColor"].string_value());

        // Array attributes
        if (userInfoJson["badgeInfo"].is_array()) {
            for (const auto &badge : userInfoJson["badgeInfo"].array_items()) {
                loginData.userInfo.badgeInfo.append(QString::fromStdString(badge.string_value()));
            }
        }

        if (userInfoJson["loyaltyInfo"].is_array()) {
            for (const auto &loyalty : userInfoJson["loyaltyInfo"].array_items()) {
                loginData.userInfo.loyaltyInfo.append(
                    QString::fromStdString(loyalty.string_value()));
            }
        }

        if (userInfoJson["lastUsedHashtags"].is_array()) {
            for (const auto &hashtag : userInfoJson["lastUsedHashtags"].array_items()) {
                loginData.userInfo.lastUsedHashtags.append(
                    QString::fromStdString(hashtag.string_value()));
            }
        }

        if (userInfoJson["levelBadges"].is_array()) {
            for (const auto &badge : userInfoJson["levelBadges"].array_items()) {
                loginData.userInfo.levelBadges.append(QString::fromStdString(badge.string_value()));
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
    loginData.message = QString::fromStdString(json["message"].string_value());
    loginData.result = QString::fromStdString(json["result"].string_value());
    loginData.refreshToken = QString::fromStdString(json["refreshToken"].string_value());
    loginData.jwtAccessToken = QString::fromStdString(json["jwtAccessToken"].string_value());
    loginData.accessToken = QString::fromStdString(json["accessToken"].string_value());
    loginData.giftModuleState = json["giftModuleState"].int_value();
    loginData.word = QString::fromStdString(json["word"].string_value());

    // Handle A/B testing related fields
    loginData.abtestNewbieFocus = QString::fromStdString(json["abtestNewbieFocus"].string_value());
    loginData.abtestNewbieGuidance =
        QString::fromStdString(json["abtestNewbieGuidance"].string_value());
    loginData.abtestNewbieGuide = QString::fromStdString(json["abtestNewbieGuide"].string_value());

    // Handle recommendation and onboarding related fields
    loginData.showRecommend = json["showRecommend"].bool_value();
    loginData.newbieEnhanceGuidanceStyle = json["newbieEnhanceGuidanceStyle"].int_value();
    loginData.newbieGuidanceFocusMissionEnable =
        json["newbieGuidanceFocusMissionEnable"].bool_value();

    // Handle auto-enter live streaming related fields
    const auto &autoEnterJson = json["autoEnterLive"];
    if (autoEnterJson.is_object()) {
        // Note: Field name in JSON is "auto", but field name in struct is "autoEnter"
        loginData.autoEnterLive.autoEnter = autoEnterJson["auto"].bool_value();
        loginData.autoEnterLive.liveStreamID = autoEnterJson["liveStreamID"].int_value();
    }

    return true;
}

bool JsonToOneSevenLiveArmyName(const Json &json, OneSevenLiveArmyName &armyName) {
    if (!json.is_object()) {
        return false;
    }

    armyName.customName = QString::fromStdString(json["customName"].string_value());
    armyName.defaultName = QString::fromStdString(json["defaultName"].string_value());

    return true;
}

bool OneSevenLiveArmyNameToJson(const OneSevenLiveArmyName &armyName, Json &json) {
    json = Json::object{
        {"customName", armyName.customName.toStdString()},
        {"defaultName", armyName.defaultName.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveArmyRankName(const Json &json, OneSevenLiveArmyRankName &rankName) {
    if (!json.is_object()) {
        return false;
    }

    rankName.rank = json["rank"].int_value();
    rankName.rankTier = json["rankTier"].int_value();
    rankName.customName = QString::fromStdString(json["customName"].string_value());
    rankName.defaultName = QString::fromStdString(json["defaultName"].string_value());

    return true;
}

bool OneSevenLiveArmyRankNameToJson(const OneSevenLiveArmyRankName &rankName, Json &json) {
    json = Json::object{
        {"rank", rankName.rank},
        {"rankTier", rankName.rankTier},
        {"customName", rankName.customName.toStdString()},
        {"defaultName", rankName.defaultName.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveArmyNameResponse(const Json &json, OneSevenLiveArmyNameResponse &response) {
    if (!json.is_object()) {
        return false;
    }

    // Parse armyName object
    const auto &armyNameJson = json["armyName"];
    if (armyNameJson.is_object()) {
        if (!JsonToOneSevenLiveArmyName(armyNameJson, response.armyName)) {
            return false;
        }
    }

    // Parse rankName array
    const auto &rankNameArray = json["rankName"];
    if (rankNameArray.is_array()) {
        for (const auto &rankNameJson : rankNameArray.array_items()) {
            OneSevenLiveArmyRankName rankName;
            if (JsonToOneSevenLiveArmyRankName(rankNameJson, rankName)) {
                response.rankName.append(rankName);
            }
        }
    }

    return true;
}

bool OneSevenLiveArmyNameResponseToJson(const OneSevenLiveArmyNameResponse &response, Json &json) {
    // Convert armyName object
    Json armyNameJson;
    OneSevenLiveArmyNameToJson(response.armyName, armyNameJson);

    // Convert rankName array
    std::vector<Json> rankNameJsonArray;
    for (const auto &rankName : response.rankName) {
        Json rankNameJson;
        OneSevenLiveArmyRankNameToJson(rankName, rankNameJson);
        rankNameJsonArray.push_back(rankNameJson);
    }

    json = Json::object{
        {"armyName", armyNameJson},
        {"rankName", Json(rankNameJsonArray)},
    };

    return true;
}

// Convert JSON to OneSevenLiveLabelToken
bool JsonToOneSevenLiveLabelToken(const Json &json, OneSevenLiveLabelToken &labelToken) {
    if (!json.is_object()) {
        return false;
    }

    if (json["key"].is_string()) {
        labelToken.key = QString::fromStdString(json["key"].string_value());
    }

    return true;
}

// Convert OneSevenLiveLabelToken to JSON
bool OneSevenLiveLabelTokenToJson(const OneSevenLiveLabelToken &labelToken, Json &json) {
    json = Json::object{
        {"key", labelToken.key.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveGloryroadInfo(const Json &jsonData,
                                     OneSevenLiveGloryroadInfo &gloryroadInfo) {
    if (!jsonData.is_object()) {
        return false;
    }

    // int point;
    if (jsonData["point"].is_number()) {
        gloryroadInfo.point = jsonData["point"].int_value();
    }

    // int level;
    if (jsonData["level"].is_number()) {
        gloryroadInfo.point = jsonData["level"].int_value();
    }

    // QString iconURL;
    if (jsonData["iconURL"].is_string()) {
        gloryroadInfo.iconURL = QString::fromStdString(jsonData["iconURL"].string_value());
    }

    // QString badgeIconURL;
    if (jsonData["badgeIconURL"].is_string()) {
        gloryroadInfo.badgeIconURL =
            QString::fromStdString(jsonData["badgeIconURL"].string_value());
    }

    return true;
}

// Convert JSON to OneSevenLiveArmyInfoUser
bool JsonToOneSevenLiveArmyInfoUser(const Json &json, OneSevenLiveArmyInfoUser &user) {
    if (!json.is_object()) {
        return false;
    }

    if (json["userID"].is_string()) {
        user.userID = QString::fromStdString(json["userID"].string_value());
    }

    if (json["displayName"].is_string()) {
        user.displayName = QString::fromStdString(json["displayName"].string_value());
    }

    if (json["picture"].is_string()) {
        user.picture = QString::fromStdString(json["picture"].string_value());
    }

    if (json["name"].is_string()) {
        user.name = QString::fromStdString(json["name"].string_value());
    }

    if (json["level"].is_number()) {
        user.level = json["level"].int_value();
    }

    if (json["openID"].is_string()) {
        user.openID = QString::fromStdString(json["openID"].string_value());
    }

    if (json["region"].is_string()) {
        user.region = QString::fromStdString(json["region"].string_value());
    }

    if (json["gloryroadInfo"].is_object()) {
        JsonToOneSevenLiveGloryroadInfo(json["gloryroadInfo"], user.gloryroadInfo);
    }

    if (json["gloryroadMode"].is_number()) {
        user.gloryroadMode = json["gloryroadMode"].int_value();
    }

    return true;
}

bool OneSevenLiveGloryroadInfoToJson(const OneSevenLiveGloryroadInfo &gloryroadInfo,
                                     Json &jsonData) {
    jsonData = Json::object{
        {"point", gloryroadInfo.point},
        {"level", gloryroadInfo.level},
        {"iconURL", gloryroadInfo.iconURL.toStdString()},
        {"badgeIconURL", gloryroadInfo.badgeIconURL.toStdString()},
    };

    return true;
}

// Convert OneSevenLiveArmyInfoUser to JSON
bool OneSevenLiveArmyInfoUserToJson(const OneSevenLiveArmyInfoUser &user, Json &json) {
    Json gloryroadInfoJson;
    OneSevenLiveGloryroadInfoToJson(user.gloryroadInfo, gloryroadInfoJson);

    json = Json::object{
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
bool JsonToOneSevenLiveArmyInfo(const Json &json, OneSevenLiveArmyInfo &armyInfo) {
    if (!json.is_object()) {
        return false;
    }

    if (json["user"].is_object()) {
        JsonToOneSevenLiveArmyInfoUser(json["user"], armyInfo.user);
    }

    if (json["rank"].is_number()) {
        armyInfo.rank = json["rank"].int_value();
    }

    if (json["pointContribution"].is_number()) {
        armyInfo.pointContribution = json["pointContribution"].int_value();
    }

    if (json["seniority"].is_number()) {
        armyInfo.seniority = json["seniority"].int_value();
    }

    if (json["startTime"].is_number()) {
        armyInfo.startTime = json["startTime"].int_value();
    }

    if (json["endTime"].is_number()) {
        armyInfo.endTime = json["endTime"].int_value();
    }

    if (json["isOnLive"].is_bool()) {
        armyInfo.isOnLive = json["isOnLive"].bool_value();
    }

    if (json["newStatus"].is_number()) {
        armyInfo.newStatus = json["newStatus"].int_value();
    }

    if (json["periodStartTime"].is_number()) {
        armyInfo.periodStartTime = json["periodStartTime"].int_value();
    }

    return true;
}

// Convert OneSevenLiveArmyInfo to JSON
bool OneSevenLiveArmyInfoToJson(const OneSevenLiveArmyInfo &armyInfo, Json &json) {
    Json userJson;
    OneSevenLiveArmyInfoUserToJson(armyInfo.user, userJson);

    json = Json::object{
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
bool JsonToOneSevenLiveUserAttr(const Json &json, OneSevenLiveUserAttr &userAttr) {
    if (!json.is_object()) {
        return false;
    }

    if (json["level"].is_number()) {
        userAttr.level = json["level"].int_value();
    }

    if (json["sentPoint"].is_number()) {
        userAttr.sentPoint = json["sentPoint"].int_value();
    }

    if (json["checkinLevel"].is_number()) {
        userAttr.checkinLevel = json["checkinLevel"].int_value();
    }

    if (json["checkinCount"].is_number()) {
        userAttr.checkinCount = json["checkinCount"].int_value();
    }

    if (json["checkinBdgURL"].is_string()) {
        userAttr.checkinBdgURL = QString::fromStdString(json["checkinBdgURL"].string_value());
    }

    if (json["noteStatus"].is_number()) {
        userAttr.noteStatus = json["noteStatus"].int_value();
    }

    if (json["followStatus"].is_number()) {
        userAttr.followStatus = json["followStatus"].int_value();
    }

    if (json["gloryroadMode"].is_number()) {
        userAttr.gloryroadMode = json["gloryroadMode"].int_value();
    }

    if (json["gloryroadInfo"].is_object()) {
        JsonToOneSevenLiveGloryroadInfo(json["gloryroadInfo"], userAttr.gloryroadInfo);
    }

    return true;
}

// Convert OneSevenLiveUserAttr to JSON
bool OneSevenLiveUserAttrToJson(const OneSevenLiveUserAttr &userAttr, Json &json) {
    Json gloryroadInfoJson;
    OneSevenLiveGloryroadInfoToJson(userAttr.gloryroadInfo, gloryroadInfoJson);

    json = Json::object{
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
bool JsonToOneSevenLiveAnonymousInfo(const Json &json, OneSevenLiveAnonymousInfo &anonymousInfo) {
    if (!json.is_object()) {
        return false;
    }

    if (json["isInvisible"].is_bool()) {
        anonymousInfo.isInvisible = json["isInvisible"].bool_value();
    }

    if (json["pureText"].is_string()) {
        anonymousInfo.pureText = QString::fromStdString(json["pureText"].string_value());
    }

    return true;
}

// Convert OneSevenLiveAnonymousInfo to JSON
bool OneSevenLiveAnonymousInfoToJson(const OneSevenLiveAnonymousInfo &anonymousInfo, Json &json) {
    json = Json::object{
        {"isInvisible", anonymousInfo.isInvisible},
        {"pureText", anonymousInfo.pureText.toStdString()},
    };

    return true;
}

// Convert JSON to OneSevenLiveDisplayUser
bool JsonToOneSevenLiveDisplayUser(const Json &json, OneSevenLiveDisplayUser &displayUser) {
    if (!json.is_object()) {
        return false;
    }

    if (json["armyRank"].is_number()) {
        displayUser.armyRank = json["armyRank"].int_value();
    }

    if (json["badgeURL"].is_string()) {
        displayUser.badgeURL = QString::fromStdString(json["badgeURL"].string_value());
    }

    if (json["bgColor"].is_string()) {
        displayUser.bgColor = QString::fromStdString(json["bgColor"].string_value());
    }

    if (json["checkinBdgURL"].is_string()) {
        displayUser.checkinBdgURL = QString::fromStdString(json["checkinBdgURL"].string_value());
    }

    if (json["checkinLevel"].is_number()) {
        displayUser.checkinLevel = json["checkinLevel"].int_value();
    }

    if (json["circleBadgeURL"].is_string()) {
        displayUser.circleBadgeURL = QString::fromStdString(json["circleBadgeURL"].string_value());
    }

    if (json["displayName"].is_string()) {
        displayUser.displayName = QString::fromStdString(json["displayName"].string_value());
    }

    if (json["fgColor"].is_string()) {
        displayUser.fgColor = QString::fromStdString(json["fgColor"].string_value());
    }

    if (json["gloryroadInfo"].is_object()) {
        JsonToOneSevenLiveGloryroadInfo(json["gloryroadInfo"], displayUser.gloryroadInfo);
    }

    if (json["gloryroadMode"].is_number()) {
        displayUser.gloryroadMode = json["gloryroadMode"].int_value();
    }

    if (json["hasProgram"].is_bool()) {
        displayUser.hasProgram = json["hasProgram"].bool_value();
    }

    if (json["isDirty"].is_bool()) {
        displayUser.isDirty = json["isDirty"].bool_value();
    }

    if (json["isDirtyUser"].is_bool()) {
        displayUser.isDirtyUser = json["isDirtyUser"].bool_value();
    }

    if (json["isGuardian"].is_bool()) {
        displayUser.isGuardian = json["isGuardian"].bool_value();
    }

    if (json["isProducer"].is_bool()) {
        displayUser.isProducer = json["isProducer"].bool_value();
    }

    if (json["isStreamer"].is_bool()) {
        displayUser.isStreamer = json["isStreamer"].bool_value();
    }

    if (json["isVIP"].is_bool()) {
        displayUser.isVIP = json["isVIP"].bool_value();
    }

    if (json["level"].is_number()) {
        displayUser.level = json["level"].int_value();
    }

    if (json["mLevel"].is_number()) {
        displayUser.mLevel = json["mLevel"].int_value();
    }

    if (json["pfxBadgeURL"].is_string()) {
        displayUser.pfxBadgeURL = QString::fromStdString(json["pfxBadgeURL"].string_value());
    }

    if (json["picture"].is_string()) {
        displayUser.picture = QString::fromStdString(json["picture"].string_value());
    }

    if (json["producer"].is_number()) {
        displayUser.producer = json["producer"].int_value();
    }

    if (json["program"].is_number()) {
        displayUser.program = json["program"].int_value();
    }

    if (json["topRightIconURL"].is_string()) {
        displayUser.topRightIconURL =
            QString::fromStdString(json["topRightIconURL"].string_value());
    }

    if (json["userID"].is_string()) {
        displayUser.userID = QString::fromStdString(json["userID"].string_value());
    }

    if (json["vipCharmURL"].is_string()) {
        displayUser.vipCharmURL = QString::fromStdString(json["vipCharmURL"].string_value());
    }

    return true;
}

// Convert OneSevenLiveDisplayUser to JSON
bool OneSevenLiveDisplayUserToJson(const OneSevenLiveDisplayUser &displayUser, Json &json) {
    Json gloryroadInfoJson;
    OneSevenLiveGloryroadInfoToJson(displayUser.gloryroadInfo, gloryroadInfoJson);

    json = Json::object{
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

// Convert JSON to OneSevenLiveRockZoneViewer
bool JsonToOneSevenLiveRockZoneViewer(const Json &json, OneSevenLiveRockZoneViewer &viewer) {
    if (!json.is_object()) {
        return false;
    }

    if (json["type"].is_number()) {
        viewer.type = json["type"].int_value();
    }

    if (json["armyInfo"].is_object()) {
        JsonToOneSevenLiveArmyInfo(json["armyInfo"], viewer.armyInfo);
    }

    if (json["labelToken"].is_object()) {
        JsonToOneSevenLiveLabelToken(json["labelToken"], viewer.labelToken);
    }

    if (json["userAttr"].is_object()) {
        JsonToOneSevenLiveUserAttr(json["userAttr"], viewer.userAttr);
    }

    if (json["anonymousInfo"].is_object()) {
        JsonToOneSevenLiveAnonymousInfo(json["anonymousInfo"], viewer.anonymousInfo);
    }

    if (json["armyLevel"].is_number()) {
        viewer.armyLevel = json["armyLevel"].int_value();
    }

    if (json["displayUser"].is_object()) {
        JsonToOneSevenLiveDisplayUser(json["displayUser"], viewer.displayUser);
    }

    return true;
}

// Convert OneSevenLiveRockZoneViewer to JSON
bool OneSevenLiveRockZoneViewerToJson(const OneSevenLiveRockZoneViewer &viewer, Json &json) {
    Json armyInfoJson;
    OneSevenLiveArmyInfoToJson(viewer.armyInfo, armyInfoJson);

    Json labelTokenJson;
    OneSevenLiveLabelTokenToJson(viewer.labelToken, labelTokenJson);

    Json userAttrJson;
    OneSevenLiveUserAttrToJson(viewer.userAttr, userAttrJson);

    Json anonymousInfoJson;
    OneSevenLiveAnonymousInfoToJson(viewer.anonymousInfo, anonymousInfoJson);

    Json displayUserJson;
    OneSevenLiveDisplayUserToJson(viewer.displayUser, displayUserJson);

    json = Json::object{
        {"type", viewer.type},
        {"armyInfo", armyInfoJson},
        {"labelToken", labelTokenJson},
        {"userAttr", userAttrJson},
        {"anonymousInfo", anonymousInfoJson},
        {"armyLevel", viewer.armyLevel},
        {"displayUser", displayUserJson},
    };

    return true;
}

bool JsonToOneSevenLiveRockViewers(const Json &json, QList<OneSevenLiveRockZoneViewer> &viewers) {
    if (!json.is_array()) {
        return false;
    }

    for (const auto &itemJson : json.array_items()) {
        OneSevenLiveRockZoneViewer viewer;
        JsonToOneSevenLiveRockZoneViewer(itemJson, viewer);
        viewers.append(viewer);
    }

    return true;
}

bool JsonToOneSevenLiveRoomInfo(const Json &json, OneSevenLiveRoomInfo &roomInfo) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Basic information
        roomInfo.userID = QString::fromStdString(json["userID"].string_value());
        roomInfo.streamerType = json["streamerType"].int_value();
        roomInfo.streamType = QString::fromStdString(json["streamType"].string_value());
        roomInfo.status = json["status"].int_value();
        roomInfo.caption = QString::fromStdString(json["caption"].string_value());
        roomInfo.thumbnail = QString::fromStdString(json["thumbnail"].string_value());

        // RTMP URLs
        const auto &rtmpUrlsJson = json["rtmpUrls"];
        if (rtmpUrlsJson.is_array()) {
            for (const auto &urlJson : rtmpUrlsJson.array_items()) {
                OneSevenLiveRtmpUrl rtmpUrl;
                rtmpUrl.provider = urlJson["provider"].int_value();
                rtmpUrl.streamType = QString::fromStdString(urlJson["streamType"].string_value());
                rtmpUrl.url = QString::fromStdString(urlJson["url"].string_value());
                rtmpUrl.urlLowQuality =
                    QString::fromStdString(urlJson["urlLowQuality"].string_value());
                rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].string_value());
                rtmpUrl.webUrlLowQuality =
                    QString::fromStdString(urlJson["webUrlLowQuality"].string_value());
                rtmpUrl.urlHighQuality =
                    QString::fromStdString(urlJson["urlHighQuality"].string_value());
                rtmpUrl.weight = urlJson["weight"].int_value();
                rtmpUrl.throttle = urlJson["throttle"].bool_value();
                roomInfo.rtmpUrls.append(rtmpUrl);
            }
        }

        // Pull URLs Info
        const auto &pullUrlsInfoJson = json["pullURLsInfo"];
        if (pullUrlsInfoJson.is_object()) {
            roomInfo.pullURLsInfo.seqNo = pullUrlsInfoJson["seqNo"].int_value();
            const auto &rtmpURLsJson = pullUrlsInfoJson["rtmpURLs"];
            if (rtmpURLsJson.is_array()) {
                for (const auto &urlJson : rtmpURLsJson.array_items()) {
                    OneSevenLiveRtmpUrl rtmpUrl;
                    rtmpUrl.provider = urlJson["provider"].int_value();
                    rtmpUrl.streamType =
                        QString::fromStdString(urlJson["streamType"].string_value());
                    rtmpUrl.url = QString::fromStdString(urlJson["url"].string_value());
                    rtmpUrl.urlLowQuality =
                        QString::fromStdString(urlJson["urlLowQuality"].string_value());
                    rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].string_value());
                    rtmpUrl.webUrlLowQuality =
                        QString::fromStdString(urlJson["webUrlLowQuality"].string_value());
                    rtmpUrl.urlHighQuality =
                        QString::fromStdString(urlJson["urlHighQuality"].string_value());
                    rtmpUrl.weight = urlJson["weight"].int_value();
                    rtmpUrl.throttle = urlJson["throttle"].bool_value();
                    roomInfo.pullURLsInfo.rtmpURLs.append(rtmpUrl);
                }
            }
        }

        // Live streaming information
        roomInfo.allowCallin = json["allowCallin"].int_value();
        roomInfo.restreamerOpenID = QString::fromStdString(json["restreamerOpenID"].string_value());
        roomInfo.streamID = QString::fromStdString(json["streamID"].string_value());
        roomInfo.liveStreamID = json["liveStreamID"].int_value();
        roomInfo.endTime = json["endTime"].int_value();
        roomInfo.beginTime = json["beginTime"].int_value();
        roomInfo.receivedLikeCount = json["receivedLikeCount"].int_value();
        roomInfo.duration = json["duration"].int_value();
        roomInfo.viewerCount = json["viewerCount"].int_value();
        roomInfo.totalViewTime = json["totalViewTime"].int_value();
        roomInfo.liveViewerCount = json["liveViewerCount"].int_value();
        roomInfo.audioOnly = json["audioOnly"].int_value();
        roomInfo.locationName = QString::fromStdString(json["locationName"].string_value());
        roomInfo.coverPhoto = QString::fromStdString(json["coverPhoto"].string_value());
        roomInfo.latitude = json["latitude"].number_value();
        roomInfo.longitude = json["longitude"].number_value();

        // Room settings
        roomInfo.shareLocation = json["shareLocation"].int_value();
        roomInfo.followerOnlyChat = json["followerOnlyChat"].int_value();
        roomInfo.chatAvailable = json["chatAvailable"].int_value();
        roomInfo.replayCount = json["replayCount"].int_value();
        roomInfo.replayAvailable = json["replayAvailable"].int_value();
        roomInfo.numberOfChunks = json["numberOfChunks"].int_value();
        roomInfo.canSendGift = json["canSendGift"].int_value();

        // User information
        const auto &userInfoJson = json["userInfo"];
        if (userInfoJson.is_object()) {
            roomInfo.userInfo.userID =
                QString::fromStdString(userInfoJson["userID"].string_value());
            roomInfo.userInfo.openID =
                QString::fromStdString(userInfoJson["openID"].string_value());
            roomInfo.userInfo.displayName =
                QString::fromStdString(userInfoJson["displayName"].string_value());
            roomInfo.userInfo.gender =
                QString::fromStdString(userInfoJson["gender"].string_value());
            roomInfo.userInfo.isChoice = userInfoJson["isChoice"].bool_value();
            roomInfo.userInfo.isInternational = userInfoJson["isInternational"].bool_value();
            roomInfo.userInfo.adsOn = userInfoJson["adsOn"].int_value();
            roomInfo.userInfo.experience = userInfoJson["experience"].int_value();
            roomInfo.userInfo.deviceType =
                QString::fromStdString(userInfoJson["deviceType"].string_value());
            roomInfo.userInfo.picture =
                QString::fromStdString(userInfoJson["picture"].string_value());

            // Glory Road information
            const auto &gloryroadInfoJson = userInfoJson["gloryroadInfo"];
            if (gloryroadInfoJson.is_object()) {
                roomInfo.userInfo.gloryroadInfo.point = gloryroadInfoJson["point"].int_value();
                roomInfo.userInfo.gloryroadInfo.level = gloryroadInfoJson["level"].int_value();
                roomInfo.userInfo.gloryroadInfo.iconURL =
                    QString::fromStdString(gloryroadInfoJson["iconURL"].string_value());
                roomInfo.userInfo.gloryroadInfo.badgeIconURL =
                    QString::fromStdString(gloryroadInfoJson["badgeIconURL"].string_value());
            }
        }

        // Other settings
        roomInfo.landscape = json["landscape"].bool_value();
        roomInfo.mute = json["mute"].bool_value();
        roomInfo.birthdayState = json["birthdayState"].int_value();
        roomInfo.dayBeforeBirthday = json["dayBeforeBirthday"].int_value();
        roomInfo.achievementValue = json["achievementValue"].int_value();
        roomInfo.mediaMessageReadState = json["mediaMessageReadState"].int_value();
        roomInfo.region = QString::fromStdString(json["region"].string_value());
        roomInfo.device = QString::fromStdString(json["device"].string_value());

        // Event list
        const auto &eventListJson = json["eventList"];
        if (eventListJson.is_array()) {
            for (const auto &eventJson : eventListJson.array_items()) {
                OneSevenLiveEventInfo eventInfo;
                eventInfo.ID = eventJson["ID"].int_value();
                eventInfo.type = eventJson["type"].int_value();
                eventInfo.icon = QString::fromStdString(eventJson["icon"].string_value());
                eventInfo.endTime = eventJson["endTime"].int_value();
                eventInfo.showTimer = eventJson["showTimer"].int_value();
                eventInfo.name = QString::fromStdString(eventJson["name"].string_value());
                eventInfo.URL = QString::fromStdString(eventJson["URL"].string_value());
                eventInfo.pageSize = eventJson["pageSize"].int_value();
                eventInfo.webViewTitle =
                    QString::fromStdString(eventJson["webViewTitle"].string_value());

                // Icon list
                const auto &iconsJson = eventJson["icons"];
                if (iconsJson.is_array()) {
                    for (const auto &iconJson : iconsJson.array_items()) {
                        OneSevenLiveEventIcon icon;
                        icon.language = QString::fromStdString(iconJson["language"].string_value());
                        icon.value = QString::fromStdString(iconJson["value"].string_value());
                        eventInfo.icons.append(icon);
                    }
                }

                roomInfo.eventList.append(eventInfo);
            }
        }

        // Archive configuration
        const auto &archiveConfigJson = json["archiveConfig"];
        if (archiveConfigJson.is_object()) {
            roomInfo.archiveConfig.autoRecording = archiveConfigJson["autoRecording"].bool_value();
            roomInfo.archiveConfig.autoPublish = archiveConfigJson["autoPublish"].bool_value();
            roomInfo.archiveConfig.clipPermission = archiveConfigJson["clipPermission"].int_value();
            roomInfo.archiveConfig.clipPermissionDownload =
                archiveConfigJson["clipPermissionDownload"].int_value();
        }

        // Archive ID and game marquee settings
        roomInfo.archiveID = QString::fromStdString(json["archiveID"].string_value());
        roomInfo.hideGameMarquee = json["hideGameMarquee"].bool_value();

        const auto &subtabsJson = json["subtabs"];
        if (subtabsJson.is_array()) {
            for (const auto &subtabJson : subtabsJson.array_items()) {
                roomInfo.subtabs.append(QString::fromStdString(subtabJson.string_value()));
            }
        }

        const auto &lastUsedHashtagsJson = json["lastUsedHashtags"];
        if (lastUsedHashtagsJson.is_array()) {
            for (const auto &hashtagJson : lastUsedHashtagsJson.array_items()) {
                OneSevenLiveHashtag hashtag;
                hashtag.text = QString::fromStdString(hashtagJson["text"].string_value());
                hashtag.isOfficial = hashtagJson["isOfficial"].bool_value();
                roomInfo.lastUsedHashtags.append(hashtag);
            }
        }
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToOneSevenLiveRoomInfo error: %s", e.what());
        return false;
    }

    return true;
}

bool OneSevenLiveRoomInfoToJson(const OneSevenLiveRoomInfo &roomInfo, Json &json) {
    try {
        Json::object jsonObject;

        // Basic information
        jsonObject["userID"] = roomInfo.userID.toStdString();
        jsonObject["streamerType"] = roomInfo.streamerType;
        jsonObject["streamType"] = roomInfo.streamType.toStdString();
        jsonObject["status"] = roomInfo.status;
        jsonObject["caption"] = roomInfo.caption.toStdString();
        jsonObject["thumbnail"] = roomInfo.thumbnail.toStdString();

        // RTMP URLs
        Json::array rtmpUrlsArray;
        for (const auto &rtmpUrl : roomInfo.rtmpUrls) {
            Json::object rtmpUrlJson;
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
        Json::object pullUrlsInfoObject;
        pullUrlsInfoObject["seqNo"] = static_cast<int>(roomInfo.pullURLsInfo.seqNo);
        Json::array pullRtmpUrlsArray;
        for (const auto &rtmpUrl : roomInfo.pullURLsInfo.rtmpURLs) {
            Json::object rtmpUrlJson;
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
        Json::object userInfoObject;
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
        Json::object gloryroadInfoObject;
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
        Json::array eventListArray;
        for (const auto &eventInfo : roomInfo.eventList) {
            Json::object eventJson;
            eventJson["ID"] = static_cast<int>(eventInfo.ID);
            eventJson["type"] = eventInfo.type;
            eventJson["icon"] = eventInfo.icon.toStdString();
            eventJson["endTime"] = static_cast<int>(eventInfo.endTime);
            eventJson["showTimer"] = eventInfo.showTimer;
            eventJson["name"] = eventInfo.name.toStdString();
            eventJson["URL"] = eventInfo.URL.toStdString();
            eventJson["pageSize"] = eventInfo.pageSize;
            eventJson["webViewTitle"] = eventInfo.webViewTitle.toStdString();

            Json::array iconsArray;
            for (const auto &icon : eventInfo.icons) {
                Json::object iconJson;
                iconJson["language"] = icon.language.toStdString();
                iconJson["value"] = icon.value.toStdString();
                iconsArray.push_back(iconJson);
            }
            eventJson["icons"] = iconsArray;
            eventListArray.push_back(eventJson);
        }
        jsonObject["eventList"] = eventListArray;

        // Archive configuration
        Json::object archiveConfigObject;
        archiveConfigObject["autoRecording"] = roomInfo.archiveConfig.autoRecording;
        archiveConfigObject["autoPublish"] = roomInfo.archiveConfig.autoPublish;
        archiveConfigObject["clipPermission"] = roomInfo.archiveConfig.clipPermission;
        archiveConfigObject["clipPermissionDownload"] =
            roomInfo.archiveConfig.clipPermissionDownload;
        jsonObject["archiveConfig"] = archiveConfigObject;

        // Archive ID and game marquee settings
        jsonObject["archiveID"] = roomInfo.archiveID.toStdString();
        jsonObject["hideGameMarquee"] = roomInfo.hideGameMarquee;

        Json::array subtabsArray;
        for (const auto &subtab : roomInfo.subtabs) {
            subtabsArray.push_back(subtab.toStdString());
        }
        jsonObject["subtabs"] = subtabsArray;

        Json::array lastUsedHashtagsArray;
        for (const auto &hashtag : roomInfo.lastUsedHashtags) {
            Json::object hashtagJson;
            hashtagJson["text"] = hashtag.text.toStdString();
            hashtagJson["isOfficial"] = hashtag.isOfficial;
            lastUsedHashtagsArray.push_back(hashtagJson);
        }
        jsonObject["lastUsedHashtags"] = lastUsedHashtagsArray;

        json = Json(jsonObject);
        return true;

    } catch (const std::exception &e) {
        // You can add logging here, for example using obs_log
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveRoomInfoToJson error: %s", e.what());
        return false;
    }
}

bool OneSevenLiveRtmpRequestToJson(const OneSevenLiveRtmpRequest &request, Json &json) {
    // Create archive configuration JSON object
    Json archiveConfig = Json::object{{"autoRecording", request.archiveConfig.autoRecording},
                                      {"autoPublish", request.archiveConfig.autoPublish},
                                      {"clipPermission", request.archiveConfig.clipPermission}};

    Json armyOnly = Json::object{{"enable", request.armyOnly.enable},
                                 {"requiredArmyRank", request.armyOnly.requiredArmyRank},
                                 {"showOnHotPage", request.armyOnly.showOnHotPage},
                                 {"armyOnlyPN", request.armyOnly.armyOnlyPN}};

    // Create virtual streamer information JSON object
    Json vliverInfo = Json::object{{"vliverModel", request.vliverInfo.vliverModel}};

    // Convert hashtags to Json array
    std::vector<Json> hashtagsArray;
    for (const QString &tag : request.hashtags) {
        hashtagsArray.push_back(Json(tag.toStdString()));
    }

    // Create main JSON object
    Json eventID = Json(static_cast<int>(request.eventID));
    json = Json::object{{"userID", request.userID.toStdString()},
                        {"caption", request.caption.toStdString()},
                        {"device", request.device.toStdString()},
                        {"eventID", eventID},
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

bool JsonToOneSevenLiveRtmpRequest(const Json &json, OneSevenLiveRtmpRequest &request) {
    if (!json.is_object()) {
        return false;
    }
    // Basic information
    request.userID = QString::fromStdString(json["userID"].string_value());
    request.caption = QString::fromStdString(json["caption"].string_value());
    request.device = QString::fromStdString(json["device"].string_value());
    request.eventID = json["eventID"].int_value();
    // hashtags
    const auto &hashtagsJson = json["hashtags"];
    if (hashtagsJson.is_array()) {
        for (const auto &tagJson : hashtagsJson.array_items()) {
            request.hashtags.append(QString::fromStdString(tagJson.string_value()));
        }
    }
    request.landscape = json["landscape"].bool_value();
    request.streamerType = json["streamerType"].int_value();
    request.subtabID = QString::fromStdString(json["subtabID"].string_value());
    // archiveConfig
    const auto &archiveConfigJson = json["archiveConfig"];
    if (archiveConfigJson.is_object()) {
        request.archiveConfig.autoRecording = archiveConfigJson["autoRecording"].bool_value();
        request.archiveConfig.autoPublish = archiveConfigJson["autoPublish"].bool_value();
        request.archiveConfig.clipPermission = archiveConfigJson["clipPermission"].int_value();
    }
    // vliverInfo
    const auto &vliverInfoJson = json["vliverInfo"];
    if (vliverInfoJson.is_object()) {
        request.vliverInfo.vliverModel = vliverInfoJson["vliverModel"].int_value();
    }
    return true;
}

bool OneSevenLiveStreamInfoToJson(const OneSevenLiveStreamInfo &streamInfo, Json &json) {
    Json jsonRequest;
    if (!OneSevenLiveRtmpRequestToJson(streamInfo.request, jsonRequest)) {
        return false;
    }
    json = Json::object{{"request", jsonRequest},
                        {"categoryName", streamInfo.categoryName.toStdString()},
                        {"createdAt", streamInfo.createdAt.toString(Qt::ISODate).toStdString()},
                        {"streamUuid", streamInfo.streamUuid.toStdString()}};
    return true;
}

bool JsonToOneSevenLiveStreamInfo(const Json &json, OneSevenLiveStreamInfo &streamInfo) {
    if (!json.is_object()) {
        return false;
    }
    // Basic information
    streamInfo.categoryName = QString::fromStdString(json["categoryName"].string_value());
    streamInfo.createdAt = QDateTime::fromString(
        QString::fromStdString(json["createdAt"].string_value()), Qt::ISODate);
    streamInfo.streamUuid = QString::fromStdString(json["streamUuid"].string_value());
    // Handle request object
    if (!JsonToOneSevenLiveRtmpRequest(json["request"], streamInfo.request)) {
        return false;
    }
    return true;
}

bool JsonToOneSevenLiveRtmpResponse(const Json &json, OneSevenLiveRtmpResponse &response) {
    if (!json.is_object()) {
        return false;
    }

    // Basic information
    response.liveStreamID = QString::fromStdString(json["liveStreamID"].string_value());
    response.streamID = QString::fromStdString(json["streamID"].string_value());
    response.rtmpURL = QString::fromStdString(json["rtmpURL"].string_value());
    response.rtmpProvider = QString::fromStdString(json["rtmpProvider"].string_value());
    response.messageProvider = json["messageProvider"].int_value();

    // firstStreamInfo is empty object, assign directly
    response.firstStreamInfo = json["firstStreamInfo"];

    // Handle rtmpURLs array
    const auto &rtmpUrlsJson = json["rtmpURLs"];
    if (rtmpUrlsJson.is_array()) {
        for (const auto &urlJson : rtmpUrlsJson.array_items()) {
            OneSevenLiveRtmpUrl rtmpUrl;
            rtmpUrl.provider = urlJson["provider"].int_value();
            rtmpUrl.streamType = QString::fromStdString(urlJson["streamType"].string_value());
            rtmpUrl.url = QString::fromStdString(urlJson["url"].string_value());
            rtmpUrl.urlLowQuality = QString::fromStdString(urlJson["urlLowQuality"].string_value());
            rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].string_value());
            rtmpUrl.webUrlLowQuality =
                QString::fromStdString(urlJson["webUrlLowQuality"].string_value());
            rtmpUrl.urlHighQuality =
                QString::fromStdString(urlJson["urlHighQuality"].string_value());
            rtmpUrl.weight = urlJson["weight"].int_value();
            rtmpUrl.throttle = urlJson["throttle"].bool_value();
            response.rtmpURLs.append(rtmpUrl);
        }
    }

    // Handle achievement value status
    const auto &achievementValueStateJson = json["achievementValueState"];
    if (achievementValueStateJson.is_object()) {
        response.achievementValueState.isValueCarryOver =
            achievementValueStateJson["isValueCarryOver"].bool_value();
        response.achievementValueState.initSeconds =
            achievementValueStateJson["initSeconds"].int_value();
    }

    // Subtitle enable status
    response.subtitleEnabled = json["subtitleEnabled"].bool_value();

    // Handle WHIP information
    const auto &whipJson = json["WHIP"];
    if (whipJson.is_object()) {
        response.whipInfo.server = QString::fromStdString(whipJson["server"].string_value());
        response.whipInfo.token = QString::fromStdString(whipJson["token"].string_value());
    }

    return true;
}

bool OneSevenLiveCloseLiveRequestToJson(const OneSevenLiveCloseLiveRequest &request, Json &json) {
    json = Json::object{{"reason", request.reason.toStdString()},
                        {"userID", request.userID.toStdString()}};

    return true;
}

bool JsonToOneSevenLiveConfigStreamer(const Json &json, OneSevenLiveConfigStreamer &response) {
    if (!json.is_object()) {
        return false;
    }

    // Parse event section
    if (json["event"].is_object()) {
        const auto &eventJson = json["event"];

        // Parse events array
        if (eventJson["events"].is_array()) {
            const auto &eventsArray = eventJson["events"].array_items();
            for (const auto &eventItem : eventsArray) {
                OneSevenLiveEventItem item;
                item.ID = eventItem["ID"].int_value();
                item.name = QString::fromStdString(eventItem["name"].string_value());
                item.bannerURL = QString::fromStdString(eventItem["bannerURL"].string_value());
                item.descriptionURL =
                    QString::fromStdString(eventItem["descriptionURL"].string_value());
                item.endTime = eventItem["endTime"].int_value();

                // Parse tagIDs array
                if (eventItem["tagIDs"].is_array()) {
                    const auto &tagIDsArray = eventItem["tagIDs"].array_items();
                    for (const auto &tagID : tagIDsArray) {
                        item.tagIDs.append(QString::fromStdString(tagID.string_value()));
                    }
                }

                response.event.events.append(item);
            }
        }

        response.event.notEligibleForAllEvents = eventJson["notEligibleForAllEvents"].bool_value();
        response.event.promotionIndex = eventJson["promotionIndex"].int_value();
        response.event.instructionURL =
            QString::fromStdString(eventJson["instructionURL"].string_value());

        // Parse tags array
        if (eventJson["tags"].is_array()) {
            const auto &tagsArray = eventJson["tags"].array_items();
            for (const auto &tagItem : tagsArray) {
                OneSevenLiveEventTag tag;
                tag.ID = QString::fromStdString(tagItem["ID"].string_value());
                tag.name = QString::fromStdString(tagItem["name"].string_value());
                response.event.tags.append(tag);
            }
        }
    }

    // Parse customEvent section
    if (json["customEvent"].is_object()) {
        const auto &customEventJson = json["customEvent"];
        response.customEvent.endTime = customEventJson["endTime"].int_value();
        response.customEvent.status = customEventJson["status"].int_value();
    }

    // Parse boxGacha section
    if (json["boxGacha"].is_object()) {
        const auto &boxGachaJson = json["boxGacha"];
        response.boxGacha.previousSettingStatus =
            boxGachaJson["previousSettingStatus"].bool_value();
        response.boxGacha.availableEventID =
            QString::fromStdString(boxGachaJson["availableEventID"].string_value());
    }

    // Parse subtabs array
    if (json["subtabs"].is_array()) {
        const auto &subtabsArray = json["subtabs"].array_items();
        for (const auto &subtabItem : subtabsArray) {
            OneSevenLiveSubtab subtab;
            subtab.displayName = QString::fromStdString(subtabItem["displayName"].string_value());
            subtab.ID = QString::fromStdString(subtabItem["ID"].string_value());
            response.subtabs.append(subtab);
        }
    }

    if (json["lastStreamState"].is_object()) {
        const auto &lastStreamStateJson = json["lastStreamState"];
        OneSevenLiveStreamState lastStreamState;
        if (json["lastStreamState"]["vliverInfo"].is_object()) {
            OneSevenLiveVliverInfo vliverInfo;
            vliverInfo.vliverModel = lastStreamStateJson["vliverInfo"]["vliverModel"].int_value();
            lastStreamState.vliverInfo = vliverInfo;
        }
        response.lastStreamState = lastStreamState;
    }

    response.hashtagSelectLimit = json["hashtagSelectLimit"].int_value();
    response.armyOnly = json["armyOnly"].int_value();

    // Archive configuration
    const auto &archiveConfigJson = json["archiveConfig"];
    if (archiveConfigJson.is_object()) {
        response.archiveConfig.autoRecording = archiveConfigJson["autoRecording"].bool_value();
        response.archiveConfig.autoPublish = archiveConfigJson["autoPublish"].bool_value();
        response.archiveConfig.clipPermission = archiveConfigJson["clipPermission"].int_value();
        response.archiveConfig.clipPermissionDownload =
            archiveConfigJson["clipPermissionDownload"].int_value();
    }

    return true;
}

bool OneSevenLiveConfigStreamerToJson(const OneSevenLiveConfigStreamer &response, Json &json) {
    // Create event section
    std::vector<Json> eventsArray;
    for (const auto &event : response.event.events) {
        // Create tagIDs array
        std::vector<Json> tagIDsArray;
        for (const QString &tagID : event.tagIDs) {
            tagIDsArray.push_back(Json(tagID.toStdString()));
        }

        // Create single event object
        Json eventJson = Json::object{{"ID", static_cast<int>(event.ID)},
                                      {"name", event.name.toStdString()},
                                      {"bannerURL", event.bannerURL.toStdString()},
                                      {"descriptionURL", event.descriptionURL.toStdString()},
                                      {"tagIDs", tagIDsArray},
                                      {"endTime", static_cast<int>(event.endTime)}};
        eventsArray.push_back(eventJson);
    }

    // Create tags array
    std::vector<Json> tagsArray;
    for (const auto &tag : response.event.tags) {
        Json tagJson = Json::object{{"ID", tag.ID.toStdString()}, {"name", tag.name.toStdString()}};
        tagsArray.push_back(tagJson);
    }

    // Create event object
    Json eventJson =
        Json::object{{"events", eventsArray},
                     {"notEligibleForAllEvents", response.event.notEligibleForAllEvents},
                     {"promotionIndex", response.event.promotionIndex},
                     {"tags", tagsArray},
                     {"instructionURL", response.event.instructionURL.toStdString()}};

    // Create customEvent object
    Json customEventJson = Json::object{{"endTime", static_cast<int>(response.customEvent.endTime)},
                                        {"status", response.customEvent.status}};

    // Create boxGacha object
    Json boxGachaJson =
        Json::object{{"previousSettingStatus", response.boxGacha.previousSettingStatus},
                     {"availableEventID", response.boxGacha.availableEventID.toStdString()}};

    // Create subtabs array
    std::vector<Json> subtabsArray;
    for (const auto &subtab : response.subtabs) {
        Json subtabJson = Json::object{{"displayName", subtab.displayName.toStdString()},
                                       {"ID", subtab.ID.toStdString()}};
        subtabsArray.push_back(subtabJson);
    }

    Json lastStreamStateJson = Json::object{
        {"vliverInfo",
         Json::object{{"vliverModel", response.lastStreamState.vliverInfo.vliverModel}}}};

    // Archive configuration
    Json::object archiveConfigObject;
    archiveConfigObject["autoRecording"] = response.archiveConfig.autoRecording;
    archiveConfigObject["autoPublish"] = response.archiveConfig.autoPublish;
    archiveConfigObject["clipPermission"] = response.archiveConfig.clipPermission;
    archiveConfigObject["clipPermissionDownload"] = response.archiveConfig.clipPermissionDownload;

    // Create main JSON object
    json = Json::object{{"event", eventJson},
                        {"customEvent", customEventJson},
                        {"boxGacha", boxGachaJson},
                        {"subtabs", subtabsArray},
                        {"lastStreamState", lastStreamStateJson},
                        {"hashtagSelectLimit", response.hashtagSelectLimit},
                        {"armyOnly", response.armyOnly},
                        {"archiveConfig", archiveConfigObject}};

    return true;
}

bool JsonToOneSevenLiveAblyTokenResponse(const Json &json,
                                         OneSevenLiveAblyTokenResponse &response) {
    if (!json.is_object()) {
        return false;
    }

    // Parse provider field
    if (json["provider"].is_number()) {
        response.provider = json["provider"].int_value();
    } else {
        return false;
    }

    // Parse token field
    if (json["token"].is_string()) {
        response.token = QString::fromStdString(json["token"].string_value());
    } else {
        return false;
    }

    // Parse channels array
    if (json["channels"].is_array()) {
        const auto &channelsArray = json["channels"].array_items();
        response.channels.clear();
        for (const auto &channel : channelsArray) {
            if (channel.is_string()) {
                response.channels.append(QString::fromStdString(channel.string_value()));
            }
        }
    } else {
        return false;
    }

    return true;
}

bool OneSevenLiveAblyTokenResponseToJson(const OneSevenLiveAblyTokenResponse &response,
                                         Json &json) {
    // Create channels array
    std::vector<Json> channelsArray;
    for (const QString &channel : response.channels) {
        channelsArray.push_back(Json(channel.toStdString()));
    }

    // Create main JSON object
    json = Json::object{{"provider", response.provider},
                        {"token", response.token.toStdString()},
                        {"channels", channelsArray}};

    return true;
}

bool JsonToOneSevenLiveUserInfo(const Json &json, OneSevenLiveUserInfo &userInfo) {
    if (!json.is_object()) {
        return false;
    }

    try {
        OneSevenLiveOnliveInfo onliveInfo;
        if (json["onliveInfo"].is_object()) {
            onliveInfo.premiumType = json["onliveInfo"]["premiumType"].int_value();
        }
        userInfo.onliveInfo = onliveInfo;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "Error parsing JSON: %s", e.what());
        return false;
    }

    return true;
}

bool JsonToOneSevenLiveConfig(const Json &json, OneSevenLiveConfig &config) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Handle addOns object
        const auto &addOnsJson = json["addOns"];
        if (addOnsJson.is_object()) {
            // Handle features object
            const auto &featuresJson = addOnsJson["features"];
            if (featuresJson.is_object()) {
                // Iterate through all key-value pairs in features object
                for (const auto &item : featuresJson.object_items()) {
                    const std::string &key = item.first;
                    const int value = item.second.int_value();
                    config.addOns.features[QString::fromStdString(key)] = value;
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

bool OneSevenLiveConfigToJson(const OneSevenLiveConfig &config, Json &json) {
    try {
        // Create features object
        Json::object featuresObject;
        for (auto it = config.addOns.features.constBegin(); it != config.addOns.features.constEnd();
             ++it) {
            featuresObject[it.key().toStdString()] = it.value();
        }

        // Create addOns object
        Json::object addOnsObject;
        addOnsObject["features"] = featuresObject;

        // Create main JSON object
        Json::object jsonObject;
        jsonObject["addOns"] = addOnsObject;

        json = Json(jsonObject);
        return true;
    } catch (const std::exception &e) {
        // Log error message
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveConfigToJson error: %s", e.what());
        return false;
    }
}

bool JsonToOneSevenLiveArmySubscriptionLevels(const Json &json,
                                              OneSevenLiveArmySubscriptionLevels &levels) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Handle subscriptionLevels array
        const auto &subscriptionLevelsJson = json["subscriptionLevels"];
        if (subscriptionLevelsJson.is_array()) {
            levels.subscriptionLevels.clear();

            for (const auto &levelJson : subscriptionLevelsJson.array_items()) {
                OneSevenLiveArmySubscriptionLevel level;

                // Parse basic fields
                level.rank = levelJson["rank"].int_value();
                level.subscribersAmount = levelJson["subscribersAmount"].int_value();

                // Parse i18nToken object
                const auto &i18nTokenJson = levelJson["i18nToken"];
                if (i18nTokenJson.is_object()) {
                    level.i18nToken.key =
                        QString::fromStdString(i18nTokenJson["key"].string_value());

                    // Parse params array
                    const auto &paramsJson = i18nTokenJson["params"];
                    if (paramsJson.is_array()) {
                        for (const auto &paramJson : paramsJson.array_items()) {
                            OneSevenLiveI18nTokenParam param;
                            param.value = QString::fromStdString(paramJson["value"].string_value());
                            level.i18nToken.params.append(param);
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
                                              Json &json) {
    try {
        // Create subscriptionLevels array
        std::vector<Json> subscriptionLevelsArray;

        for (const auto &level : levels.subscriptionLevels) {
            // Create params array
            std::vector<Json> paramsArray;
            for (const auto &param : level.i18nToken.params) {
                Json paramJson = Json::object{{"value", param.value.toStdString()}};
                paramsArray.push_back(paramJson);
            }

            // Create i18nToken object
            Json i18nTokenJson;
            if (level.i18nToken.params.isEmpty()) {
                i18nTokenJson = Json::object{{"key", level.i18nToken.key.toStdString()}};
            } else {
                i18nTokenJson = Json::object{{"key", level.i18nToken.key.toStdString()},
                                             {"params", paramsArray}};
            }

            // Create level object
            Json levelJson = Json::object{{"rank", level.rank},
                                          {"subscribersAmount", level.subscribersAmount},
                                          {"i18nToken", i18nTokenJson}};

            subscriptionLevelsArray.push_back(levelJson);
        }

        // Create main JSON object
        json = Json::object{{"subscriptionLevels", subscriptionLevelsArray}};

        return true;
    } catch (const std::exception &e) {
        // Log error message
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveArmySubscriptionLevelsToJson error: %s",
                e.what());
        return false;
    }
}

bool JsonToOneSevenLiveGiftTabsResponse(const Json &json, OneSevenLiveGiftTabsResponse &response) {
    try {
        // Parse giftLastUpdate
        response.giftLastUpdate = json["giftLastUpdate"].int_value();

        // Parse tabs array
        if (json["tabs"].is_array()) {
            const auto &tabsArray = json["tabs"].array_items();
            for (const auto &tabItem : tabsArray) {
                OneSevenLiveGiftTab tab;
                tab.id = QString::fromStdString(tabItem["id"].string_value());
                tab.type = tabItem["type"].int_value();
                tab.name = QString::fromStdString(tabItem["name"].string_value());

                // Parse gifts array
                if (tabItem["gifts"].is_array()) {
                    const auto &giftsArray = tabItem["gifts"].array_items();
                    for (const auto &giftItem : giftsArray) {
                        OneSevenLiveGift gift;
                        gift.giftID = QString::fromStdString(giftItem["giftID"].string_value());
                        gift.isHidden = giftItem["isHidden"].int_value();
                        gift.regionMode = giftItem["regionMode"].int_value();
                        gift.name = QString::fromStdString(giftItem["name"].string_value());
                        gift.point = giftItem["point"].int_value();
                        gift.leaderboardIcon =
                            QString::fromStdString(giftItem["leaderboardIcon"].string_value());
                        gift.vffURL = QString::fromStdString(giftItem["vffURL"].string_value());
                        gift.vffMD5 = QString::fromStdString(giftItem["vffMD5"].string_value());
                        gift.vffJson = QString::fromStdString(giftItem["vffJson"].string_value());

                        // Parse regions array
                        if (giftItem["regions"].is_array()) {
                            const auto &regionsArray = giftItem["regions"].array_items();
                            for (const auto &region : regionsArray) {
                                gift.regions.append(QString::fromStdString(region.string_value()));
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

bool OneSevenLiveGiftTabsResponseToJson(const OneSevenLiveGiftTabsResponse &response, Json &json) {
    try {
        // Create tabs array
        std::vector<Json> tabsArray;
        for (const auto &tab : response.tabs) {
            // Create gifts array
            std::vector<Json> giftsArray;
            for (const auto &gift : tab.gifts) {
                // Create regions array
                std::vector<Json> regionsArray;
                for (const auto &region : gift.regions) {
                    regionsArray.push_back(Json(region.toStdString()));
                }

                Json giftJson =
                    Json::object{{"giftID", gift.giftID.toStdString()},
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

            Json tabJson = Json::object{{"id", tab.id.toStdString()},
                                        {"type", tab.type},
                                        {"name", tab.name.toStdString()},
                                        {"gifts", giftsArray}};
            tabsArray.push_back(tabJson);
        }

        // Create main JSON object
        json = Json::object{{"giftLastUpdate", static_cast<int>(response.giftLastUpdate)},
                            {"tabs", tabsArray}};

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveGiftTabsResponseToJson error: %s", e.what());
        return false;
    }
}

bool JsonToOneSevenLiveGiftsResponse(const Json &json, OneSevenLiveGiftsResponse &response) {
    try {
        // Parse lastUpdate
        response.lastUpdate = json["lastUpdate"].int_value();

        // Parse gifts array
        if (json["gifts"].is_array()) {
            const auto &giftsArray = json["gifts"].array_items();
            for (const auto &giftItem : giftsArray) {
                OneSevenLiveGift gift;
                gift.giftID = QString::fromStdString(giftItem["giftID"].string_value());
                gift.isHidden = giftItem["isHidden"].int_value();
                gift.regionMode = giftItem["regionMode"].int_value();
                gift.name = QString::fromStdString(giftItem["name"].string_value());
                gift.point = giftItem["point"].int_value();
                gift.leaderboardIcon =
                    QString::fromStdString(giftItem["leaderboardIcon"].string_value());
                gift.vffURL = QString::fromStdString(giftItem["vffURL"].string_value());
                gift.vffMD5 = QString::fromStdString(giftItem["vffMD5"].string_value());
                gift.vffJson = QString::fromStdString(giftItem["vffJson"].string_value());

                // Parse regions array
                if (giftItem["regions"].is_array()) {
                    const auto &regionsArray = giftItem["regions"].array_items();
                    for (const auto &region : regionsArray) {
                        gift.regions.append(QString::fromStdString(region.string_value()));
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

bool OneSevenLiveGiftsResponseToJson(const OneSevenLiveGiftsResponse &response, Json &json) {
    try {
        // Create gifts array
        std::vector<Json> giftsArray;
        for (const auto &gift : response.gifts) {
            // Create regions array
            std::vector<Json> regionsArray;
            for (const auto &region : gift.regions) {
                regionsArray.push_back(Json(region.toStdString()));
            }

            Json giftJson = Json::object{{"giftID", gift.giftID.toStdString()},
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
        json = Json::object{{"lastUpdate", static_cast<int>(response.lastUpdate)},
                            {"gifts", giftsArray}};

        return true;
    } catch (const std::exception &e) {
        obs_log(LOG_ERROR, "[obs-17live]: OneSevenLiveGiftsResponseToJson error: %s", e.what());
        return false;
    }
}

bool OneSevenLiveCustomEventToJson(const OneSevenLiveCustomEvent &request, Json &json) {
    json = Json::object{
        {"eventName", request.eventName.toStdString()},
        {"description", request.description.toStdString()},
        {"endTime", static_cast<int>(request.endTime)},
        {"dailyGoalPoints", static_cast<int>(request.dailyGoalPoints)},
        {"goalPoints", static_cast<int>(request.goalPoints)},
        {"userID", request.userID.toStdString()},
    };

    // Add gift ID array
    std::vector<Json> giftIDsJson;
    for (const auto &giftID : request.giftIDs) {
        giftIDsJson.push_back(Json(giftID.toStdString()));
    }
    auto jsonObj = json.object_items();
    jsonObj["giftIDs"] = Json(giftIDsJson);
    json = Json(jsonObj);

    return true;
}

bool OneSevenLiveChangeCustomEventStatusRequestToJson(
    const OneSevenLiveCustomEventStatusRequest &request, Json &json) {
    json = Json::object{
        {"status", request.status},
        {"userID", request.userID.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLiveCustomEvent(const Json &json, OneSevenLiveCustomEvent &response) {
    response.eventID = QString::fromStdString(json["eventID"].string_value());
    response.userID = QString::fromStdString(json["userID"].string_value());
    response.status = json["status"].int_value();
    response.eventName = QString::fromStdString(json["eventName"].string_value());
    response.description = QString::fromStdString(json["description"].string_value());
    response.startTime = json["startTime"].int_value();
    response.endTime = json["endTime"].int_value();
    response.realEndTime = json["realEndTime"].int_value();
    response.isAchieved = json["isAchieved"].bool_value();
    response.goalPoints = json["goalPoints"].int_value();
    response.dailyGoalPoints = json["dailyGoalPoints"].int_value();
    response.displayStatus = QString::fromStdString(json["displayStatus"].string_value());
    response.currentGoalPoints = json["currentGoalPoints"].int_value();
    response.currentDailyGoalPoints = json["currentDailyGoalPoints"].int_value();

    // Process giftIDs array
    auto giftIDsJson = json["giftIDs"].array_items();
    for (const auto &giftIDJson : giftIDsJson) {
        response.giftIDs.append(QString::fromStdString(giftIDJson.string_value()));
    }

    // Process gifts array
    auto giftsJson = json["gifts"].array_items();
    for (const auto &giftJson : giftsJson) {
        OneSevenLiveGift gift;
        gift.giftID = QString::fromStdString(giftJson["giftID"].string_value());
        gift.name = QString::fromStdString(giftJson["name"].string_value());
        gift.point = giftJson["point"].int_value();
        gift.isHidden = giftJson["isHidden"].int_value();
        gift.regionMode = giftJson["regionMode"].int_value();
        gift.leaderboardIcon = QString::fromStdString(giftJson["leaderboardIcon"].string_value());
        gift.vffURL = QString::fromStdString(giftJson["vffURL"].string_value());
        gift.vffMD5 = QString::fromStdString(giftJson["vffMD5"].string_value());
        gift.vffJson = QString::fromStdString(giftJson["vffJson"].string_value());

        // Process regions array
        auto regionsJson = giftJson["regions"].array_items();
        for (const auto &regionJson : regionsJson) {
            gift.regions.append(QString::fromStdString(regionJson.string_value()));
        }

        response.gifts.append(gift);
    }

    // Process rewards array
    auto rewardsJson = json["rewards"].array_items();
    for (const auto &rewardJson : rewardsJson) {
        response.rewards.append(rewardJson);
    }

    return true;
}

bool OneSevenLivePokeRequestToJson(const OneSevenLivePokeRequest &request, Json &json) {
    json = Json::object{
        {"isPokeBack", request.isPokeBack},
        {"srcID", request.srcID.toStdString()},
        {"userID", request.userID.toStdString()},
    };

    return true;
}

bool JsonToOneSevenLivePokeResponse(const Json &json, OneSevenLivePokeResponse &response) {
    response.pokeAnimationID = QString::fromStdString(json["pokeAnimationID"].string_value());
    return true;
}

bool OneSevenLivePokeAllRequestToJson(const OneSevenLivePokeAllRequest &request, Json &json) {
    json = Json::object{
        {"liveStreamID", request.liveStreamID.toStdString()},
        {"receiverGroup", request.receiverGroup},
    };

    return true;
}
