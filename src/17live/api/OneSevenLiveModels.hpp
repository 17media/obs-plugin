#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "json11.hpp"

using namespace json11;

// Define current streaming status, including not started 0, live created 1, streaming started 2
enum class OneSevenLiveStreamingStatus { NotStarted, Live, Streaming };

// Function to get Provider name by index
static QString GetProviderNameByIndex(int index) {
    switch (index) {
    case 0:
        return "DEFAULT";
    case 1:
        return "UCLOUD";
    case 2:
        return "QINIU";
    case 3:
        return "QCLOUD";
    case 4:
        return "WANSU";
    case 5:
        return "WANSU_LOW_LATENCY";
    case 6:
        return "WANSU_SPECIFIED_IP";
    case 7:
        return "SRS";
    case 8:
        return "CHT";
    case 9:
        return "AWS";
    case 10:
        return "QINIU_AUTH";
    case 11:
        return "WANSU_AUTH";
    case 12:
        return "LIVE17";
    case 13:
        return "WANSU_CDN";
    case 14:
        return "GOOGLE_CDN";
    case 15:
        return "AKAMAI_CDN";
    case 16:
        return "CLOUDFRONT_CDN";
    case 17:
        return "TENCENT";
    case 18:
        return "LIVE17_AUTH";
    default:
        return "UNKNOWN";
    }
}

struct OneSevenLiveAPIResponse {
    QString key;
    QString data;
};

struct OneSevenLiveAPIResult {
    QString result;
    QString message;
};

struct OneSevenLiveOnliveInfo {
    int premiumType;
};

struct OneSevenLiveUserInfo {
    QString userID;
    QString openID;
    QString displayName;
    QString name;
    QString bio;
    QString picture;
    QString website;
    int followerCount;
    int followingCount;
    int receivedLikeCount;
    int likeCount;
    int isFollowing;
    int isNotif;
    int isBlocked;
    qint64 followTime;
    qint64 followRequestTime;
    qint64 roomID;
    QString privacyMode;
    int ballerLevel;
    int postCount;
    int isCelebrity;
    int baller;
    int level;
    int followPrivacyMode;
    QString revenueShareIndicator;
    int clanStatus;
    QStringList badgeInfo;
    QString region;
    int hideAllPointToLeaderboard;
    int enableShop;
    QVariantMap monthlyVIPBadges;
    qint64 lastLiveTimestamp;
    qint64 lastCreateLiveTimestamp;
    QString lastLiveRegion;
    QStringList loyaltyInfo;
    bool streamerRecapEnable;
    int gloryroadMode;
    QStringList lastUsedHashtags;
    bool newbieDisplayAllGiftTabsToast;
    int avatarOnboardingPhase;
    bool isUnderaged;
    QStringList levelBadges;
    int isEmailVerified;
    QString extIDAppleTransfer;
    QString commentShadowColor;
    bool isFreePrivateMsgEnabled;
    bool isVliverOnlyModeEnabled;
    OneSevenLiveOnliveInfo onliveInfo;
};

bool JsonToOneSevenLiveUserInfo(const Json &json, OneSevenLiveUserInfo &userInfo);

struct OneSevenLiveAutoEnter {
    bool autoEnter;
    qint64 liveStreamID;
};

struct OneSevenLiveLoginData {
    OneSevenLiveUserInfo userInfo;
    QString message;
    QString result;
    QString refreshToken;
    QString jwtAccessToken;
    QString accessToken;
    int giftModuleState;
    QString word;
    QString abtestNewbieFocus;
    QString abtestNewbieGuidance;
    QString abtestNewbieGuide;
    bool showRecommend;
    OneSevenLiveAutoEnter autoEnterLive;
    int newbieEnhanceGuidanceStyle;
    bool newbieGuidanceFocusMissionEnable;
};

bool JsonToOneSevenLiveLoginData(const Json &json, OneSevenLiveLoginData &loginData);

/* struct for json data
{
  "errorCode": 7,
  "errorMessage": "token invalid",
  "errorTitle": ""
}
*/
struct OneSevenLiveError {
    int errorCode;
    QString errorMessage;
    QString errorTitle;
};

// RTMP URL information struct
struct OneSevenLiveRtmpUrl {
    int provider;
    QString streamType;
    QString url;
    QString urlLowQuality;
    QString webUrl;
    QString webUrlLowQuality;
    QString urlHighQuality;
    int weight;
    bool throttle;
};

// Pull stream URL information struct
struct OneSevenLivePullUrlsInfo {
    QList<OneSevenLiveRtmpUrl> rtmpURLs;
    qint64 seqNo;
};

// Product information struct
struct OneSevenLiveCommodityInfo {
    int type;
    int price;
    int amount;
    QString desc;
    qint64 endTimeMS;
};

// Event icon information struct
struct OneSevenLiveEventIcon {
    QString language;
    QString value;
};

// Event information struct
struct OneSevenLiveEventInfo {
    qint64 ID;
    int type;
    QString icon;
    qint64 endTime;
    int showTimer;
    QString name;
    QString URL;
    int pageSize;
    QString webViewTitle;
    QList<OneSevenLiveEventIcon> icons;
    QList<OneSevenLiveEventIcon> webViewTitles;
};

// Glory road information struct
struct OneSevenLiveGloryroadInfo {
    int point;
    int level;
    QString iconURL;
    QString badgeIconURL;
};

// Guild information struct
struct OneSevenLiveClanInfo {
    int joinCount;
};

// League information struct
struct OneSevenLiveLeagueInfo {
    bool shouldShowEntrance;
};

// User information struct
struct OneSevenLiveStreamUserInfo : public OneSevenLiveUserInfo {
    QString gender;
    bool isChoice;
    bool isInternational;
    int adsOn;
    qint64 subscribeExpireTime;
    int experience;
    QString version;
    QString deviceType;
    QString createClanID;
    OneSevenLiveClanInfo clanInfo;
    int chatMuteDuration;
    QString language;
    QString registerRegion;
    int vipGroupType;
    int followReminder;
    OneSevenLiveLeagueInfo leagueInfo;
    bool hasVipPurchase;
    bool disableMakeLiveHotToast;
    OneSevenLiveGloryroadInfo gloryroadInfo;
};

struct OneSevenLiveArchiveConfig {
    bool autoRecording;
    bool autoPublish;
    int clipPermission;
    int clipPermissionDownload;  // New field
};

// hashtag struct
struct OneSevenLiveHashtag {
    QString text;
    bool isOfficial;
};

// Main room information struct
struct OneSevenLiveRoomInfo {
    QString userID;
    int streamerType;
    QString streamType;
    int status;
    QString caption;
    QString thumbnail;
    QList<OneSevenLiveRtmpUrl> rtmpUrls;
    OneSevenLivePullUrlsInfo pullURLsInfo;
    int allowCallin;
    QString restreamerOpenID;
    QString streamID;
    qint64 liveStreamID;
    qint64 endTime;
    qint64 beginTime;
    qint64 receivedLikeCount;
    int duration;
    int viewerCount;
    qint64 totalViewTime;
    int liveViewerCount;
    int audioOnly;
    QString locationName;
    QString coverPhoto;
    double latitude;
    double longitude;
    int shareLocation;
    int followerOnlyChat;
    int chatAvailable;
    int replayCount;
    int replayAvailable;
    int numberOfChunks;
    int canSendGift;
    OneSevenLiveStreamUserInfo userInfo;
    bool landscape;
    bool mute;
    int birthdayState;
    int dayBeforeBirthday;
    int achievementValue;
    int mediaMessageReadState;
    QString region;
    int specialTag;
    QString guardianUserID;
    QString guardianPicture;
    QString campaignIcon;
    QString campaignURL;
    qint64 campaignEndTime;
    int campaignShowTimer;
    int campaignSize;
    QString campaignTitle;
    int commodityState;
    OneSevenLiveCommodityInfo commodityInfo;
    bool canSellCommodity;
    int gridStyle;
    QString device;
    QList<OneSevenLiveEventInfo> eventList;
    OneSevenLiveArchiveConfig archiveConfig;  // Add archive configuration
    QString archiveID;                        // Add archive ID
    bool hideGameMarquee;                     // Add game marquee hide flag
    QStringList subtabs;
    QList<OneSevenLiveHashtag> lastUsedHashtags;
};

// Convert Json to OneSevenLiveRoomInfo struct
bool JsonToOneSevenLiveRoomInfo(const Json &json, OneSevenLiveRoomInfo &roomInfo);
bool OneSevenLiveRoomInfoToJson(const OneSevenLiveRoomInfo &roomInfo, Json &json);

// Virtual streamer information struct
struct OneSevenLiveVliverInfo {
    int vliverModel;
};

// Army settings
struct OneSevenLiveArmy {
    bool armyOnlyPN;
    bool enable;
    int requiredArmyRank;
    bool showOnHotPage;
};

// RTMP request struct
struct OneSevenLiveRtmpRequest {
    QString userID;
    QString caption;
    QString device;
    qint64 eventID;
    QStringList hashtags;
    bool landscape;
    int streamerType;
    QString subtabID;
    OneSevenLiveArchiveConfig archiveConfig;
    OneSevenLiveVliverInfo vliverInfo;
    OneSevenLiveArmy armyOnly;
};

bool OneSevenLiveRtmpRequestToJson(const OneSevenLiveRtmpRequest &request, Json &json);
bool JsonToOneSevenLiveRtmpRequest(const Json &json, OneSevenLiveRtmpRequest &request);

struct OneSevenLiveStreamInfo {
    OneSevenLiveRtmpRequest request;
    QString categoryName;
    QDateTime createdAt;
    QString streamUuid;
};

bool OneSevenLiveStreamInfoToJson(const OneSevenLiveStreamInfo &streamInfo, Json &json);
bool JsonToOneSevenLiveStreamInfo(const Json &json, OneSevenLiveStreamInfo &streamInfo);

// Achievement value status struct
struct OneSevenLiveAchievementValueState {
    bool isValueCarryOver;
    int initSeconds;
};

// RTMP response struct
struct OneSevenLiveRtmpResponse {
    QString liveStreamID;
    QString streamID;
    QString rtmpURL;
    QString rtmpProvider;
    int messageProvider;
    Json firstStreamInfo;                 // Use Json type because it's an empty object
    QList<OneSevenLiveRtmpUrl> rtmpURLs;  // Reuse existing OneSevenLiveRtmpUrl struct
    OneSevenLiveAchievementValueState achievementValueState;
    bool subtitleEnabled;
};

bool JsonToOneSevenLiveRtmpResponse(const Json &json, OneSevenLiveRtmpResponse &response);

// Close live request struct
struct OneSevenLiveCloseLiveRequest {
    QString userID;
    QString reason;
};

bool OneSevenLiveCloseLiveRequestToJson(const OneSevenLiveCloseLiveRequest &request, Json &json);

// Event tag struct
struct OneSevenLiveEventTag {
    QString ID;
    QString name;
};

// Ably Token response struct
struct OneSevenLiveAblyTokenResponse {
    int provider;
    QString token;
    QStringList channels;
};

bool JsonToOneSevenLiveAblyTokenResponse(const Json &json, OneSevenLiveAblyTokenResponse &response);
bool OneSevenLiveAblyTokenResponseToJson(const OneSevenLiveAblyTokenResponse &response, Json &json);

// Event item struct
struct OneSevenLiveEventItem {
    qint64 ID;
    QString name;
    QString bannerURL;
    QString descriptionURL;
    QStringList tagIDs;
    qint64 endTime;
};

// Event list struct
struct OneSevenLiveEventList {
    QList<OneSevenLiveEventItem> events;
    bool notEligibleForAllEvents;
    int promotionIndex;
    QList<OneSevenLiveEventTag> tags;
    QString instructionURL;
};

// Custom event struct
struct OneSevenLiveCustomEvent {
    qint64 endTime;
    int status;
};

// Box gacha struct
struct OneSevenLiveBoxGacha {
    bool previousSettingStatus;
    QString availableEventID;
};

// Subtab struct
struct OneSevenLiveSubtab {
    QString displayName;
    QString ID;
};

struct OneSevenLiveStreamState {
    OneSevenLiveVliverInfo vliverInfo;
};

// Configure streamer response struct
struct OneSevenLiveConfigStreamer {
    OneSevenLiveEventList event;
    OneSevenLiveCustomEvent customEvent;
    OneSevenLiveBoxGacha boxGacha;
    QList<OneSevenLiveSubtab> subtabs;
    OneSevenLiveStreamState lastStreamState;
    int hashtagSelectLimit;
    int armyOnly;
    OneSevenLiveArchiveConfig archiveConfig;
};

// Function declaration to parse JSON to OneSevenLiveConfigStreamerResponse struct
bool JsonToOneSevenLiveConfigStreamer(const Json &json, OneSevenLiveConfigStreamer &response);
bool OneSevenLiveConfigStreamerToJson(const OneSevenLiveConfigStreamer &response, Json &json);

// Add-ons struct
struct OneSevenLiveAddOns {
    QMap<QString, int> features;
};

// Configuration struct for handling the following json data:
// {
//   "addOns": {
//     "features": {
//       "158": 1,
//       "159": 0
//     }
//   }
// }
struct OneSevenLiveConfig {
    OneSevenLiveAddOns addOns;
};

// Function declaration to parse JSON to OneSevenLiveConfig struct
bool JsonToOneSevenLiveConfig(const Json &json, OneSevenLiveConfig &config);
bool OneSevenLiveConfigToJson(const OneSevenLiveConfig &config, Json &json);

// Internationalization token parameter struct
struct OneSevenLiveI18nTokenParam {
    QString value;
};

// Internationalization token struct
struct OneSevenLiveI18nToken {
    QString key;
    QList<OneSevenLiveI18nTokenParam> params;
};

// Army subscription level struct
struct OneSevenLiveArmySubscriptionLevel {
    int rank;                         // Level ranking
    int subscribersAmount;            // Number of subscribers
    OneSevenLiveI18nToken i18nToken;  // Internationalization token
};

// Army subscription levels list struct
struct OneSevenLiveArmySubscriptionLevels {
    QList<OneSevenLiveArmySubscriptionLevel> subscriptionLevels;
};

// JSON conversion function declarations
bool JsonToOneSevenLiveArmySubscriptionLevels(const Json &json,
                                              OneSevenLiveArmySubscriptionLevels &levels);
bool OneSevenLiveArmySubscriptionLevelsToJson(const OneSevenLiveArmySubscriptionLevels &levels,
                                              Json &json);
