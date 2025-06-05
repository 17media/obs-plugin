#include "SeventeenLiveModels.hpp"

#include "json11.hpp"

#include <obs-module.h>

#include "plugin-support.h"

using namespace json11;
bool JsonToSeventeenLiveLoginData(const Json &json, SeventeenLiveLoginData &loginData)
{
    if (!json.is_object()) {
        return false;
    }

    // 处理用户信息
    const auto& userInfoJson = json["userInfo"];
    if (userInfoJson.is_object()) {
        // 基本用户信息
        loginData.userInfo.userID = QString::fromStdString(userInfoJson["userID"].string_value());
        loginData.userInfo.openID = QString::fromStdString(userInfoJson["openID"].string_value());
        loginData.userInfo.displayName = QString::fromStdString(userInfoJson["displayName"].string_value());
        loginData.userInfo.name = QString::fromStdString(userInfoJson["name"].string_value());
        loginData.userInfo.bio = QString::fromStdString(userInfoJson["bio"].string_value());
        loginData.userInfo.picture = QString::fromStdString(userInfoJson["picture"].string_value());
        loginData.userInfo.website = QString::fromStdString(userInfoJson["website"].string_value());
        
        // 计数信息
        loginData.userInfo.followerCount = userInfoJson["followerCount"].int_value();
        loginData.userInfo.followingCount = userInfoJson["followingCount"].int_value();
        loginData.userInfo.receivedLikeCount = userInfoJson["receivedLikeCount"].int_value();
        loginData.userInfo.likeCount = userInfoJson["likeCount"].int_value();
        
        // 关注状态
        loginData.userInfo.isFollowing = userInfoJson["isFollowing"].int_value();
        loginData.userInfo.isNotif = userInfoJson["isNotif"].int_value();
        loginData.userInfo.isBlocked = userInfoJson["isBlocked"].int_value();
        loginData.userInfo.followTime = userInfoJson["followTime"].int_value();
        loginData.userInfo.followRequestTime = userInfoJson["followRequestTime"].int_value();
        
        // 房间和隐私设置
        loginData.userInfo.roomID = userInfoJson["roomID"].int_value();
        loginData.userInfo.privacyMode = QString::fromStdString(userInfoJson["privacyMode"].string_value());
        loginData.userInfo.followPrivacyMode = userInfoJson["followPrivacyMode"].int_value();
        
        // 等级和状态信息
        loginData.userInfo.ballerLevel = userInfoJson["ballerLevel"].int_value();
        loginData.userInfo.postCount = userInfoJson["postCount"].int_value();
        loginData.userInfo.isCelebrity = userInfoJson["isCelebrity"].int_value();
        loginData.userInfo.baller = userInfoJson["baller"].int_value();
        loginData.userInfo.level = userInfoJson["level"].int_value();
        
        // 其他属性
        loginData.userInfo.revenueShareIndicator = QString::fromStdString(userInfoJson["revenueShareIndicator"].string_value());
        loginData.userInfo.clanStatus = userInfoJson["clanStatus"].int_value();
        loginData.userInfo.region = QString::fromStdString(userInfoJson["region"].string_value());
        loginData.userInfo.hideAllPointToLeaderboard = userInfoJson["hideAllPointToLeaderboard"].int_value();
        loginData.userInfo.enableShop = userInfoJson["enableShop"].int_value();
        
        // 时间戳信息
        loginData.userInfo.lastLiveTimestamp = userInfoJson["lastLiveTimestamp"].int_value();
        loginData.userInfo.lastCreateLiveTimestamp = userInfoJson["lastCreateLiveTimestamp"].int_value();
        loginData.userInfo.lastLiveRegion = QString::fromStdString(userInfoJson["lastLiveRegion"].string_value());
        
        // 布尔值属性
        loginData.userInfo.streamerRecapEnable = userInfoJson["streamerRecapEnable"].bool_value();
        loginData.userInfo.newbieDisplayAllGiftTabsToast = userInfoJson["newbieDisplayAllGiftTabsToast"].bool_value();
        loginData.userInfo.isUnderaged = userInfoJson["isUnderaged"].bool_value();
        loginData.userInfo.isFreePrivateMsgEnabled = userInfoJson["isFreePrivateMsgEnabled"].bool_value();
        loginData.userInfo.isVliverOnlyModeEnabled = userInfoJson["isVliverOnlyModeEnabled"].bool_value();
        
        // 整数属性
        loginData.userInfo.gloryroadMode = userInfoJson["gloryroadMode"].int_value();
        loginData.userInfo.avatarOnboardingPhase = userInfoJson["avatarOnboardingPhase"].int_value();
        loginData.userInfo.isEmailVerified = userInfoJson["isEmailVerified"].int_value();
        
        // 字符串属性
        loginData.userInfo.extIDAppleTransfer = QString::fromStdString(userInfoJson["extIDAppleTransfer"].string_value());
        loginData.userInfo.commentShadowColor = QString::fromStdString(userInfoJson["commentShadowColor"].string_value());
        
        // 数组属性
        if (userInfoJson["badgeInfo"].is_array()) {
            for (const auto& badge : userInfoJson["badgeInfo"].array_items()) {
                loginData.userInfo.badgeInfo.append(QString::fromStdString(badge.string_value()));
            }
        }
        
        if (userInfoJson["loyaltyInfo"].is_array()) {
            for (const auto& loyalty : userInfoJson["loyaltyInfo"].array_items()) {
                loginData.userInfo.loyaltyInfo.append(QString::fromStdString(loyalty.string_value()));
            }
        }
        
        if (userInfoJson["lastUsedHashtags"].is_array()) {
            for (const auto& hashtag : userInfoJson["lastUsedHashtags"].array_items()) {
                loginData.userInfo.lastUsedHashtags.append(QString::fromStdString(hashtag.string_value()));
            }
        }
        
        if (userInfoJson["levelBadges"].is_array()) {
            for (const auto& badge : userInfoJson["levelBadges"].array_items()) {
                loginData.userInfo.levelBadges.append(QString::fromStdString(badge.string_value()));
            }
        }
        
        // 对象属性 - monthlyVIPBadges
        // 注意：这里假设QVariantMap可以直接从JSON对象构建，实际实现可能需要调整
        if (userInfoJson["monthlyVIPBadges"].is_object()) {
            // 这里需要根据实际情况处理monthlyVIPBadges
            // 简单示例：
            // const auto& badges = userInfoJson["monthlyVIPBadges"].object_items();
            // for (const auto& pair : badges) {
            //     loginData.userInfo.monthlyVIPBadges.insert(QString::fromStdString(pair.first), QVariant::fromValue(pair.second));
            // }
        }
    }
    
    // 处理基本响应信息
    loginData.message = QString::fromStdString(json["message"].string_value());
    loginData.result = QString::fromStdString(json["result"].string_value());
    loginData.refreshToken = QString::fromStdString(json["refreshToken"].string_value());
    loginData.jwtAccessToken = QString::fromStdString(json["jwtAccessToken"].string_value());
    loginData.accessToken = QString::fromStdString(json["accessToken"].string_value());
    loginData.giftModuleState = json["giftModuleState"].int_value();
    loginData.word = QString::fromStdString(json["word"].string_value());
    
    // 处理A/B测试相关字段
    loginData.abtestNewbieFocus = QString::fromStdString(json["abtestNewbieFocus"].string_value());
    loginData.abtestNewbieGuidance = QString::fromStdString(json["abtestNewbieGuidance"].string_value());
    loginData.abtestNewbieGuide = QString::fromStdString(json["abtestNewbieGuide"].string_value());
    
    // 处理推荐和新手引导相关字段
    loginData.showRecommend = json["showRecommend"].bool_value();
    loginData.newbieEnhanceGuidanceStyle = json["newbieEnhanceGuidanceStyle"].int_value();
    loginData.newbieGuidanceFocusMissionEnable = json["newbieGuidanceFocusMissionEnable"].bool_value();
    
    // 处理自动进入直播相关字段
    const auto& autoEnterJson = json["autoEnterLive"];
    if (autoEnterJson.is_object()) {
        // 注意：JSON中字段名为"auto"，但结构体中字段名为"autoEnter"
        loginData.autoEnterLive.autoEnter = autoEnterJson["auto"].bool_value();
        loginData.autoEnterLive.liveStreamID = autoEnterJson["liveStreamID"].int_value();
    }
    
    return true;
}
bool JsonToSeventeenLiveRoomInfo(const Json &json, SeventeenLiveRoomInfo &roomInfo)
{
    if (!json.is_object()) {
        return false;
    }

    try {
        // 基本信息
        roomInfo.userID = QString::fromStdString(json["userID"].string_value());
        roomInfo.streamerType = json["streamerType"].int_value();
        roomInfo.streamType = QString::fromStdString(json["streamType"].string_value());
        roomInfo.status = json["status"].int_value();
        roomInfo.caption = QString::fromStdString(json["caption"].string_value());
        roomInfo.thumbnail = QString::fromStdString(json["thumbnail"].string_value());

        // RTMP URLs
        const auto& rtmpUrlsJson = json["rtmpUrls"];
        if (rtmpUrlsJson.is_array()) {
            for (const auto& urlJson : rtmpUrlsJson.array_items()) {
                SeventeenLiveRtmpUrl rtmpUrl;
                rtmpUrl.provider = urlJson["provider"].int_value();
                rtmpUrl.streamType = QString::fromStdString(urlJson["streamType"].string_value());
                rtmpUrl.url = QString::fromStdString(urlJson["url"].string_value());
                rtmpUrl.urlLowQuality = QString::fromStdString(urlJson["urlLowQuality"].string_value());
                rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].string_value());
                rtmpUrl.webUrlLowQuality = QString::fromStdString(urlJson["webUrlLowQuality"].string_value());
                rtmpUrl.urlHighQuality = QString::fromStdString(urlJson["urlHighQuality"].string_value());
                rtmpUrl.weight = urlJson["weight"].int_value();
                rtmpUrl.throttle = urlJson["throttle"].bool_value();
                roomInfo.rtmpUrls.append(rtmpUrl);
            }
        }

        // Pull URLs Info
        const auto& pullUrlsInfoJson = json["pullURLsInfo"];
        if (pullUrlsInfoJson.is_object()) {
            roomInfo.pullURLsInfo.seqNo = pullUrlsInfoJson["seqNo"].int_value();
            const auto& rtmpURLsJson = pullUrlsInfoJson["rtmpURLs"];
            if (rtmpURLsJson.is_array()) {
                for (const auto& urlJson : rtmpURLsJson.array_items()) {
                    SeventeenLiveRtmpUrl rtmpUrl;
                    rtmpUrl.provider = urlJson["provider"].int_value();
                    rtmpUrl.streamType = QString::fromStdString(urlJson["streamType"].string_value());
                    rtmpUrl.url = QString::fromStdString(urlJson["url"].string_value());
                    rtmpUrl.urlLowQuality = QString::fromStdString(urlJson["urlLowQuality"].string_value());
                    rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].string_value());
                    rtmpUrl.webUrlLowQuality = QString::fromStdString(urlJson["webUrlLowQuality"].string_value());
                    rtmpUrl.urlHighQuality = QString::fromStdString(urlJson["urlHighQuality"].string_value());
                    rtmpUrl.weight = urlJson["weight"].int_value();
                    rtmpUrl.throttle = urlJson["throttle"].bool_value();
                    roomInfo.pullURLsInfo.rtmpURLs.append(rtmpUrl);
                }
            }
        }

        // 直播信息
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

        // 房间设置
        roomInfo.shareLocation = json["shareLocation"].int_value();
        roomInfo.followerOnlyChat = json["followerOnlyChat"].int_value();
        roomInfo.chatAvailable = json["chatAvailable"].int_value();
        roomInfo.replayCount = json["replayCount"].int_value();
        roomInfo.replayAvailable = json["replayAvailable"].int_value();
        roomInfo.numberOfChunks = json["numberOfChunks"].int_value();
        roomInfo.canSendGift = json["canSendGift"].int_value();

        // 用户信息
        const auto& userInfoJson = json["userInfo"];
        if (userInfoJson.is_object()) {
            roomInfo.userInfo.userID = QString::fromStdString(userInfoJson["userID"].string_value());
            roomInfo.userInfo.openID = QString::fromStdString(userInfoJson["openID"].string_value());
            roomInfo.userInfo.displayName = QString::fromStdString(userInfoJson["displayName"].string_value());
            roomInfo.userInfo.gender = QString::fromStdString(userInfoJson["gender"].string_value());
            roomInfo.userInfo.isChoice = userInfoJson["isChoice"].bool_value();
            roomInfo.userInfo.isInternational = userInfoJson["isInternational"].bool_value();
            roomInfo.userInfo.adsOn = userInfoJson["adsOn"].int_value();
            roomInfo.userInfo.experience = userInfoJson["experience"].int_value();
            roomInfo.userInfo.deviceType = QString::fromStdString(userInfoJson["deviceType"].string_value());
            roomInfo.userInfo.picture = QString::fromStdString(userInfoJson["picture"].string_value());


            // 荣耀之路信息
            const auto& gloryroadInfoJson = userInfoJson["gloryroadInfo"];
            if (gloryroadInfoJson.is_object()) {
                roomInfo.userInfo.gloryroadInfo.point = gloryroadInfoJson["point"].int_value();
                roomInfo.userInfo.gloryroadInfo.level = gloryroadInfoJson["level"].int_value();
                roomInfo.userInfo.gloryroadInfo.iconURL = QString::fromStdString(gloryroadInfoJson["iconURL"].string_value());
                roomInfo.userInfo.gloryroadInfo.badgeIconURL = QString::fromStdString(gloryroadInfoJson["badgeIconURL"].string_value());
            }
        }

        // 其他设置
        roomInfo.landscape = json["landscape"].bool_value();
        roomInfo.mute = json["mute"].bool_value();
        roomInfo.birthdayState = json["birthdayState"].int_value();
        roomInfo.dayBeforeBirthday = json["dayBeforeBirthday"].int_value();
        roomInfo.achievementValue = json["achievementValue"].int_value();
        roomInfo.mediaMessageReadState = json["mediaMessageReadState"].int_value();
        roomInfo.region = QString::fromStdString(json["region"].string_value());
        roomInfo.device = QString::fromStdString(json["device"].string_value());

        // 活动列表
        const auto& eventListJson = json["eventList"];
        if (eventListJson.is_array()) {
            for (const auto& eventJson : eventListJson.array_items()) {
                SeventeenLiveEventInfo eventInfo;
                eventInfo.ID = eventJson["ID"].int_value();
                eventInfo.type = eventJson["type"].int_value();
                eventInfo.icon = QString::fromStdString(eventJson["icon"].string_value());
                eventInfo.endTime = eventJson["endTime"].int_value();
                eventInfo.showTimer = eventJson["showTimer"].int_value();
                eventInfo.name = QString::fromStdString(eventJson["name"].string_value());
                eventInfo.URL = QString::fromStdString(eventJson["URL"].string_value());
                eventInfo.pageSize = eventJson["pageSize"].int_value();
                eventInfo.webViewTitle = QString::fromStdString(eventJson["webViewTitle"].string_value());

                // 图标列表
                const auto& iconsJson = eventJson["icons"];
                if (iconsJson.is_array()) {
                    for (const auto& iconJson : iconsJson.array_items()) {
                        SeventeenLiveEventIcon icon;
                        icon.language = QString::fromStdString(iconJson["language"].string_value());
                        icon.value = QString::fromStdString(iconJson["value"].string_value());
                        eventInfo.icons.append(icon);
                    }
                }

                roomInfo.eventList.append(eventInfo);
            }
        }


        // 存档配置
        const auto& archiveConfigJson = json["archiveConfig"];
        if (archiveConfigJson.is_object()) {
            roomInfo.archiveConfig.autoRecording = archiveConfigJson["autoRecording"].bool_value();
            roomInfo.archiveConfig.autoPublish = archiveConfigJson["autoPublish"].bool_value();
            roomInfo.archiveConfig.clipPermission = archiveConfigJson["clipPermission"].int_value();
            roomInfo.archiveConfig.clipPermissionDownload = archiveConfigJson["clipPermissionDownload"].int_value();
        }

        // 存档ID和游戏跑马灯设置
        roomInfo.archiveID = QString::fromStdString(json["archiveID"].string_value());
        roomInfo.hideGameMarquee = json["hideGameMarquee"].bool_value();

        const auto& subtabsJson = json["subtabs"];
        if (subtabsJson.is_array()) {
            for (const auto& subtabJson : subtabsJson.array_items()) {
                roomInfo.subtabs.append(QString::fromStdString(subtabJson.string_value()));
            }
        }

        const auto& lastUsedHashtagsJson = json["lastUsedHashtags"];
        if (lastUsedHashtagsJson.is_array()) {
            for (const auto& hashtagJson : lastUsedHashtagsJson.array_items()) {
                SeventeenLiveHashtag hashtag;
                hashtag.text = QString::fromStdString(hashtagJson["text"].string_value());
                hashtag.isOfficial = hashtagJson["isOfficial"].bool_value();
                roomInfo.lastUsedHashtags.append(hashtag);
            }
        }
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "[obs-17live]: JsonToSeventeenLiveRoomInfo error: %s", e.what());
        return false;
    }

    return true;
}

bool SeventeenLiveRoomInfoToJson(const SeventeenLiveRoomInfo &roomInfo, Json &json) {
    try {
        Json::object jsonObject;

        // 基本信息
        jsonObject["userID"] = roomInfo.userID.toStdString();
        jsonObject["streamerType"] = roomInfo.streamerType;
        jsonObject["streamType"] = roomInfo.streamType.toStdString();
        jsonObject["status"] = roomInfo.status;
        jsonObject["caption"] = roomInfo.caption.toStdString();
        jsonObject["thumbnail"] = roomInfo.thumbnail.toStdString();

        // RTMP URLs
        Json::array rtmpUrlsArray;
        for (const auto& rtmpUrl : roomInfo.rtmpUrls) {
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
        for (const auto& rtmpUrl : roomInfo.pullURLsInfo.rtmpURLs) {
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

        // 直播信息
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

        // 房间设置
        jsonObject["shareLocation"] = roomInfo.shareLocation;
        jsonObject["followerOnlyChat"] = roomInfo.followerOnlyChat;
        jsonObject["chatAvailable"] = roomInfo.chatAvailable;
        jsonObject["replayCount"] = roomInfo.replayCount;
        jsonObject["replayAvailable"] = roomInfo.replayAvailable;
        jsonObject["numberOfChunks"] = roomInfo.numberOfChunks;
        jsonObject["canSendGift"] = roomInfo.canSendGift;

        // 用户信息
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

        // 荣耀之路信息
        Json::object gloryroadInfoObject;
        gloryroadInfoObject["point"] = roomInfo.userInfo.gloryroadInfo.point;
        gloryroadInfoObject["level"] = roomInfo.userInfo.gloryroadInfo.level;
        gloryroadInfoObject["iconURL"] = roomInfo.userInfo.gloryroadInfo.iconURL.toStdString();
        gloryroadInfoObject["badgeIconURL"] = roomInfo.userInfo.gloryroadInfo.badgeIconURL.toStdString();
        userInfoObject["gloryroadInfo"] = gloryroadInfoObject;
        jsonObject["userInfo"] = userInfoObject;

        // 其他设置
        jsonObject["landscape"] = roomInfo.landscape;
        jsonObject["mute"] = roomInfo.mute;
        jsonObject["birthdayState"] = roomInfo.birthdayState;
        jsonObject["dayBeforeBirthday"] = roomInfo.dayBeforeBirthday;
        jsonObject["achievementValue"] = roomInfo.achievementValue;
        jsonObject["mediaMessageReadState"] = roomInfo.mediaMessageReadState;
        jsonObject["region"] = roomInfo.region.toStdString();
        jsonObject["device"] = roomInfo.device.toStdString();

        // 活动列表
        Json::array eventListArray;
        for (const auto& eventInfo : roomInfo.eventList) {
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
            for (const auto& icon : eventInfo.icons) {
                Json::object iconJson;
                iconJson["language"] = icon.language.toStdString();
                iconJson["value"] = icon.value.toStdString();
                iconsArray.push_back(iconJson);
            }
            eventJson["icons"] = iconsArray;
            eventListArray.push_back(eventJson);
        }
        jsonObject["eventList"] = eventListArray;

        // 存档配置
        Json::object archiveConfigObject;
        archiveConfigObject["autoRecording"] = roomInfo.archiveConfig.autoRecording;
        archiveConfigObject["autoPublish"] = roomInfo.archiveConfig.autoPublish;
        archiveConfigObject["clipPermission"] = roomInfo.archiveConfig.clipPermission;
        archiveConfigObject["clipPermissionDownload"] = roomInfo.archiveConfig.clipPermissionDownload;
        jsonObject["archiveConfig"] = archiveConfigObject;

        // 存档ID和游戏跑马灯设置
        jsonObject["archiveID"] = roomInfo.archiveID.toStdString();
        jsonObject["hideGameMarquee"] = roomInfo.hideGameMarquee;

        Json::array subtabsArray;
        for (const auto& subtab : roomInfo.subtabs) {
            subtabsArray.push_back(subtab.toStdString());
        }
        jsonObject["subtabs"] = subtabsArray;

        Json::array lastUsedHashtagsArray;
        for (const auto& hashtag : roomInfo.lastUsedHashtags) {
            Json::object hashtagJson;
            hashtagJson["text"] = hashtag.text.toStdString();
            hashtagJson["isOfficial"] = hashtag.isOfficial;
            lastUsedHashtagsArray.push_back(hashtagJson);
        }
        jsonObject["lastUsedHashtags"] = lastUsedHashtagsArray;

        json = Json(jsonObject);
        return true;

    } catch (const std::exception& e) {
        // 您可以在这里添加日志记录，例如使用 obs_log
        obs_log(LOG_ERROR, "[obs-17live]: SeventeenLiveRoomInfoToJson error: %s", e.what());
        return false;
    }
}

bool SeventeenLiveRtmpRequestToJson(const SeventeenLiveRtmpRequest &request, Json &json)
{
    // 创建存档配置的JSON对象
    Json archiveConfig = Json::object{
        {"autoRecording", request.archiveConfig.autoRecording},
        {"autoPublish", request.archiveConfig.autoPublish},
        {"clipPermission", request.archiveConfig.clipPermission}
    };

    // 创建虚拟主播信息的JSON对象
    Json vliverInfo = Json::object{
        {"vliverModel", request.vliverInfo.vliverModel}
    };

    // 将hashtags转换为Json数组
    std::vector<Json> hashtagsArray;
    for (const QString &tag : request.hashtags) {
        hashtagsArray.push_back(Json(tag.toStdString()));
    }

    // 创建主JSON对象
    Json eventID = Json(static_cast<int>(request.eventID));
    json = Json::object{
        {"userID", request.userID.toStdString()},
        {"caption", request.caption.toStdString()},
        {"device", request.device.toStdString()},
        {"eventID", eventID},
        {"hashtags", hashtagsArray},
        {"landscape", request.landscape},
        {"streamerType", request.streamerType},
        {"subtabID", request.subtabID.toStdString()},
        {"archiveConfig", archiveConfig},
        {"vliverInfo", vliverInfo}
    };

    return true;
}

bool JsonToSeventeenLiveRtmpRequest(const Json &json, SeventeenLiveRtmpRequest &request)
{
    if (!json.is_object()) {
        return false;
    }
    // 基本信息
    request.userID = QString::fromStdString(json["userID"].string_value());
    request.caption = QString::fromStdString(json["caption"].string_value());
    request.device = QString::fromStdString(json["device"].string_value());
    request.eventID = json["eventID"].int_value();
    // hashtags
    const auto& hashtagsJson = json["hashtags"];
    if (hashtagsJson.is_array()) {
        for (const auto& tagJson : hashtagsJson.array_items()) {
            request.hashtags.append(QString::fromStdString(tagJson.string_value()));
        }
    }
    request.landscape = json["landscape"].bool_value();
    request.streamerType = json["streamerType"].int_value();
    request.subtabID = QString::fromStdString(json["subtabID"].string_value());
    // archiveConfig
    const auto& archiveConfigJson = json["archiveConfig"];
    if (archiveConfigJson.is_object()) {
        request.archiveConfig.autoRecording = archiveConfigJson["autoRecording"].bool_value();
        request.archiveConfig.autoPublish = archiveConfigJson["autoPublish"].bool_value();
        request.archiveConfig.clipPermission = archiveConfigJson["clipPermission"].int_value();
    }
    // vliverInfo
    const auto& vliverInfoJson = json["vliverInfo"];
    if (vliverInfoJson.is_object()) {
        request.vliverInfo.vliverModel = vliverInfoJson["vliverModel"].int_value();
    }
    return true;
}

bool SeventeenLiveStreamInfoToJson(const SeventeenLiveStreamInfo &streamInfo, Json &json)
{
    Json jsonRequest;
    if (!SeventeenLiveRtmpRequestToJson(streamInfo.request, jsonRequest)) {
        return false;
    }
    json = Json::object{
        {"request", jsonRequest},
        {"categoryName", streamInfo.categoryName.toStdString()},
        {"createdAt", streamInfo.createdAt.toString(Qt::ISODate).toStdString()},
        {"streamUuid", streamInfo.streamUuid.toStdString()}
    };
    return true;
}

bool JsonToSeventeenLiveStreamInfo(const Json &json, SeventeenLiveStreamInfo &streamInfo)
{
    if (!json.is_object()) {
        return false;
    }
    // 基本信息
    streamInfo.categoryName = QString::fromStdString(json["categoryName"].string_value());
    streamInfo.createdAt = QDateTime::fromString(QString::fromStdString(json["createdAt"].string_value()), Qt::ISODate);
    streamInfo.streamUuid = QString::fromStdString(json["streamUuid"].string_value());
    // 处理 request 对象
    if (!JsonToSeventeenLiveRtmpRequest(json["request"], streamInfo.request)) {
        return false;
    }
    return true;
}

bool JsonToSeventeenLiveRtmpResponse(const Json &json, SeventeenLiveRtmpResponse &response)
{
    if (!json.is_object()) {
        return false;
    }

    // 基本信息
    response.liveStreamID = QString::fromStdString(json["liveStreamID"].string_value());
    response.streamID = QString::fromStdString(json["streamID"].string_value());
    response.rtmpURL = QString::fromStdString(json["rtmpURL"].string_value());
    response.rtmpProvider = QString::fromStdString(json["rtmpProvider"].string_value());
    response.messageProvider = json["messageProvider"].int_value();
    
    // firstStreamInfo 是空对象，直接赋值
    response.firstStreamInfo = json["firstStreamInfo"];

    // 处理 rtmpURLs 数组
    const auto& rtmpUrlsJson = json["rtmpURLs"];
    if (rtmpUrlsJson.is_array()) {
        for (const auto& urlJson : rtmpUrlsJson.array_items()) {
            SeventeenLiveRtmpUrl rtmpUrl;
            rtmpUrl.provider = urlJson["provider"].int_value();
            rtmpUrl.streamType = QString::fromStdString(urlJson["streamType"].string_value());
            rtmpUrl.url = QString::fromStdString(urlJson["url"].string_value());
            rtmpUrl.urlLowQuality = QString::fromStdString(urlJson["urlLowQuality"].string_value());
            rtmpUrl.webUrl = QString::fromStdString(urlJson["webUrl"].string_value());
            rtmpUrl.webUrlLowQuality = QString::fromStdString(urlJson["webUrlLowQuality"].string_value());
            rtmpUrl.urlHighQuality = QString::fromStdString(urlJson["urlHighQuality"].string_value());
            rtmpUrl.weight = urlJson["weight"].int_value();
            rtmpUrl.throttle = urlJson["throttle"].bool_value();
            response.rtmpURLs.append(rtmpUrl);
        }
    }

    // 处理成就值状态
    const auto& achievementValueStateJson = json["achievementValueState"];
    if (achievementValueStateJson.is_object()) {
        response.achievementValueState.isValueCarryOver = achievementValueStateJson["isValueCarryOver"].bool_value();
        response.achievementValueState.initSeconds = achievementValueStateJson["initSeconds"].int_value();
    }

    // 字幕启用状态
    response.subtitleEnabled = json["subtitleEnabled"].bool_value();

    return true;
}

bool SeventeenLiveCloseLiveRequestToJson(const SeventeenLiveCloseLiveRequest &request, Json &json)
{
    json = Json::object{
        {"reason", request.reason.toStdString()},
        {"userID", request.userID.toStdString()}
    };

    return true;
}

bool JsonToSeventeenLiveConfigStreamer(const Json &json, SeventeenLiveConfigStreamer &response) {
    if (!json.is_object()) {
      return false;
    }
  
    // 解析event部分
    if (json["event"].is_object()) {
      const auto &eventJson = json["event"];
      
      // 解析events数组
      if (eventJson["events"].is_array()) {
        const auto &eventsArray = eventJson["events"].array_items();
        for (const auto &eventItem : eventsArray) {
          SeventeenLiveEventItem item;
          item.ID = eventItem["ID"].int_value();
          item.name = QString::fromStdString(eventItem["name"].string_value());
          item.bannerURL = QString::fromStdString(eventItem["bannerURL"].string_value());
          item.descriptionURL = QString::fromStdString(eventItem["descriptionURL"].string_value());
          item.endTime = eventItem["endTime"].int_value();
          
          // 解析tagIDs数组
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
      response.event.instructionURL = QString::fromStdString(eventJson["instructionURL"].string_value());
      
      // 解析tags数组
      if (eventJson["tags"].is_array()) {
        const auto &tagsArray = eventJson["tags"].array_items();
        for (const auto &tagItem : tagsArray) {
          SeventeenLiveEventTag tag;
          tag.ID = QString::fromStdString(tagItem["ID"].string_value());
          tag.name = QString::fromStdString(tagItem["name"].string_value());
          response.event.tags.append(tag);
        }
      }
    }
    
    // 解析customEvent部分
    if (json["customEvent"].is_object()) {
      const auto &customEventJson = json["customEvent"];
      response.customEvent.endTime = customEventJson["endTime"].int_value();
      response.customEvent.status = customEventJson["status"].int_value();
    }
    
    // 解析boxGacha部分
    if (json["boxGacha"].is_object()) {
      const auto &boxGachaJson = json["boxGacha"];
      response.boxGacha.previousSettingStatus = boxGachaJson["previousSettingStatus"].bool_value();
      response.boxGacha.availableEventID = QString::fromStdString(boxGachaJson["availableEventID"].string_value());
    }
    
    // 解析subtabs数组
    if (json["subtabs"].is_array()) {
      const auto &subtabsArray = json["subtabs"].array_items();
      for (const auto &subtabItem : subtabsArray) {
        SeventeenLiveSubtab subtab;
        subtab.displayName = QString::fromStdString(subtabItem["displayName"].string_value());
        subtab.ID = QString::fromStdString(subtabItem["ID"].string_value());
        response.subtabs.append(subtab);
      }
    }

    if (json["lastStreamState"].is_object()) {
        const auto &lastStreamStateJson = json["lastStreamState"];
        SeventeenLiveStreamState lastStreamState;
        if (json["lastStreamState"]["vliverInfo"].is_object()) {
            SeventeenLiveVliverInfo vliverInfo;
            vliverInfo.vliverModel = lastStreamStateJson["vliverInfo"]["vliverModel"].int_value();
            lastStreamState.vliverInfo = vliverInfo;
        }
        response.lastStreamState = lastStreamState;
    }

    response.hashtagSelectLimit = json["hashtagSelectLimit"].int_value();
    
    return true;
}

bool SeventeenLiveConfigStreamerToJson(const SeventeenLiveConfigStreamer &response, Json &json)
{
    // 创建 event 部分
    std::vector<Json> eventsArray;
    for (const auto &event : response.event.events) {
        // 创建 tagIDs 数组
        std::vector<Json> tagIDsArray;
        for (const QString &tagID : event.tagIDs) {
            tagIDsArray.push_back(Json(tagID.toStdString()));
        }

        // 创建单个事件对象
        Json eventJson = Json::object{
            {"ID", static_cast<int>(event.ID)},
            {"name", event.name.toStdString()},
            {"bannerURL", event.bannerURL.toStdString()},
            {"descriptionURL", event.descriptionURL.toStdString()},
            {"tagIDs", tagIDsArray},
            {"endTime", static_cast<int>(event.endTime)}
        };
        eventsArray.push_back(eventJson);
    }

    // 创建 tags 数组
    std::vector<Json> tagsArray;
    for (const auto &tag : response.event.tags) {
        Json tagJson = Json::object{
            {"ID", tag.ID.toStdString()},
            {"name", tag.name.toStdString()}
        };
        tagsArray.push_back(tagJson);
    }

    // 创建 event 对象
    Json eventJson = Json::object{
        {"events", eventsArray},
        {"notEligibleForAllEvents", response.event.notEligibleForAllEvents},
        {"promotionIndex", response.event.promotionIndex},
        {"tags", tagsArray},
        {"instructionURL", response.event.instructionURL.toStdString()}
    };

    // 创建 customEvent 对象
    Json customEventJson = Json::object{
        {"endTime", static_cast<int>(response.customEvent.endTime)},
        {"status", response.customEvent.status}
    };

    // 创建 boxGacha 对象
    Json boxGachaJson = Json::object{
        {"previousSettingStatus", response.boxGacha.previousSettingStatus},
        {"availableEventID", response.boxGacha.availableEventID.toStdString()}
    };

    // 创建 subtabs 数组
    std::vector<Json> subtabsArray;
    for (const auto &subtab : response.subtabs) {
        Json subtabJson = Json::object{
            {"displayName", subtab.displayName.toStdString()},
            {"ID", subtab.ID.toStdString()}
        };
        subtabsArray.push_back(subtabJson);
    }

    Json lastStreamStateJson = Json::object{
        {"vliverInfo", Json::object{
            {"vliverModel", response.lastStreamState.vliverInfo.vliverModel}
        }}
    };

    // 创建主 JSON 对象
    json = Json::object{
        {"event", eventJson},
        {"customEvent", customEventJson},
        {"boxGacha", boxGachaJson},
        {"subtabs", subtabsArray},
        {"lastStreamState", lastStreamStateJson},
        {"hashtagSelectLimit", response.hashtagSelectLimit}
    };

    return true;
}

bool JsonToSeventeenLiveAblyTokenResponse(const Json &json, SeventeenLiveAblyTokenResponse &response) {
    if (!json.is_object()) {
        return false;
    }

    // 解析 provider 字段
    if (json["provider"].is_number()) {
        response.provider = json["provider"].int_value();
    } else {
        return false;
    }

    // 解析 token 字段
    if (json["token"].is_string()) {
        response.token = QString::fromStdString(json["token"].string_value());
    } else {
        return false;
    }

    // 解析 channels 数组
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

bool SeventeenLiveAblyTokenResponseToJson(const SeventeenLiveAblyTokenResponse &response, Json &json) {
    // 创建 channels 数组
    std::vector<Json> channelsArray;
    for (const QString &channel : response.channels) {
        channelsArray.push_back(Json(channel.toStdString()));
    }

    // 创建主 JSON 对象
    json = Json::object{
        {"provider", response.provider},
        {"token", response.token.toStdString()},
        {"channels", channelsArray}
    };

    return true;
}
