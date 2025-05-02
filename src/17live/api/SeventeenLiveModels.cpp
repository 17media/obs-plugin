#include "SeventeenLiveModels.hpp"

#include "json11.hpp"

using namespace json11;

namespace seventeenlive {

bool JsonToSeventeenLiveRoomInfo(const Json &json, SeventeenLiveRoomInfo &roomInfo)
{
    if (!json.is_object()) {
        return false;
    }

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

    return true;
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

} // namespace seventeenlive
