#pragma once

// Qt includes
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Third-party includes
#include <nlohmann/json.hpp>

using Json = nlohmann::json;

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

bool JsonToOneSevenLiveUserInfo(const nlohmann::json &json, OneSevenLiveUserInfo &userInfo);

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

bool JsonToOneSevenLiveLoginData(const nlohmann::json &json, OneSevenLiveLoginData &loginData);

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

bool JsonToOneSevenLiveRtmpUrl(const nlohmann::json &json, OneSevenLiveRtmpUrl &rtmpUrl);

// Pull stream URL information struct
struct OneSevenLivePullUrlsInfo {
    QList<OneSevenLiveRtmpUrl> rtmpURLs;
    qint64 seqNo;
};

bool JsonToOneSevenLiveRtmpUrls(const nlohmann::json &json, QList<OneSevenLiveRtmpUrl> &rtmpUrls);
bool JsonToOneSevenLivePullUrlsInfo(const nlohmann::json &pullUrlsInfoJson,
                                    OneSevenLivePullUrlsInfo &pullUrlsInfo);

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

bool JsonToOneSevenLiveGloryroadInfo(const nlohmann::json &jsonData,
                                     OneSevenLiveGloryroadInfo &gloryroadInfo);
bool OneSevenLiveGloryroadInfoToJson(const OneSevenLiveGloryroadInfo &gloryroadInfo,
                                     nlohmann::json &jsonData);

// League information struct
struct OneSevenLiveLeagueInfo {
    bool shouldShowEntrance;
};

struct OneSevenLiveUserArmyInfo {
    int joinCount;
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
    OneSevenLiveUserArmyInfo clanInfo;
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

bool JsonToOneSevenLiveArchiveConfig(const nlohmann::json &json,
                                     OneSevenLiveArchiveConfig &archiveConfig);

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
    bool enableOBSGroupCall;                  // Add OBS group call enable flag
    QStringList subtabs;
    QList<OneSevenLiveHashtag> lastUsedHashtags;
};

// Convert Json to OneSevenLiveRoomInfo struct
bool JsonToOneSevenLiveRoomInfo(const nlohmann::json &json, OneSevenLiveRoomInfo &roomInfo);
bool OneSevenLiveRoomInfoToJson(const OneSevenLiveRoomInfo &roomInfo, nlohmann::json &json);

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
    bool enableOBSGroupCall;
};

bool OneSevenLiveRtmpRequestToJson(const OneSevenLiveRtmpRequest &request, nlohmann::json &json);
bool JsonToOneSevenLiveRtmpRequest(const nlohmann::json &json, OneSevenLiveRtmpRequest &request);

struct OneSevenLiveStreamInfo {
    OneSevenLiveRtmpRequest request;
    QString categoryName;
    QDateTime createdAt;
    QString streamUuid;
};

bool OneSevenLiveStreamInfoToJson(const OneSevenLiveStreamInfo &streamInfo, nlohmann::json &json);
bool JsonToOneSevenLiveStreamInfo(const nlohmann::json &json, OneSevenLiveStreamInfo &streamInfo);

// Achievement value status struct
struct OneSevenLiveAchievementValueState {
    bool isValueCarryOver;
    int initSeconds;
};

// WHIP information struct
struct OneSevenLiveWhipInfo {
    QString server;
    QString token;
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
    OneSevenLiveWhipInfo whipInfo;  // WHIP information
};

bool JsonToOneSevenLiveRtmpResponse(const nlohmann::json &json, OneSevenLiveRtmpResponse &response);

// Close live request struct
struct OneSevenLiveCloseLiveRequest {
    QString userID;
    QString reason;
};

bool OneSevenLiveCloseLiveRequestToJson(const OneSevenLiveCloseLiveRequest &request,
                                        nlohmann::json &json);

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

bool JsonToOneSevenLiveAblyTokenResponse(const nlohmann::json &json,
                                         OneSevenLiveAblyTokenResponse &response);
bool OneSevenLiveAblyTokenResponseToJson(const OneSevenLiveAblyTokenResponse &response,
                                         nlohmann::json &json);

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

// Gift struct
struct OneSevenLiveGift {
    QString giftID;
    int isHidden;
    int regionMode;
    QString name;
    int point;
    QString leaderboardIcon;
    QString vffURL;
    QString vffMD5;
    QString vffJson;
    QStringList regions;
};

// Custom event response struct
struct OneSevenLiveCustomEvent {
    QString eventID;
    QString userID;
    int status;
    QString eventName;
    QString description;
    qint64 startTime;
    qint64 endTime;
    qint64 realEndTime;
    bool isAchieved;
    QList<QString> giftIDs;
    QList<OneSevenLiveGift> gifts;
    qint64 goalPoints;
    qint64 dailyGoalPoints;
    QString displayStatus;
    QList<Json> rewards;  // Using Json type because rewards structure is not defined
    qint64 currentGoalPoints;
    qint64 currentDailyGoalPoints;
};

// Stop custom event request struct
struct OneSevenLiveCustomEventStatusRequest {
    int status;  // Status 2 means stop
    QString userID;
};

bool JsonToOneSevenLiveCustomEvent(const nlohmann::json &json, OneSevenLiveCustomEvent &response);
bool OneSevenLiveCustomEventToJson(const OneSevenLiveCustomEvent &request, nlohmann::json &json);
bool OneSevenLiveChangeCustomEventStatusRequestToJson(
    const OneSevenLiveCustomEventStatusRequest &request, nlohmann::json &json);

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

// Gift tab struct
struct OneSevenLiveGiftTab {
    QString id;
    int type;
    QString name;
    QList<OneSevenLiveGift> gifts;
};

// Gift tabs response struct
struct OneSevenLiveGiftTabsResponse {
    qint64 giftLastUpdate;
    QList<OneSevenLiveGiftTab> tabs;
};

// Function declarations for gift tab JSON conversion
bool JsonToOneSevenLiveGiftTabsResponse(const nlohmann::json &json,
                                        OneSevenLiveGiftTabsResponse &response);
bool OneSevenLiveGiftTabsResponseToJson(const OneSevenLiveGiftTabsResponse &response,
                                        nlohmann::json &json);

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
bool JsonToOneSevenLiveConfigStreamer(const nlohmann::json &json,
                                      OneSevenLiveConfigStreamer &response);
bool OneSevenLiveConfigStreamerToJson(const OneSevenLiveConfigStreamer &response,
                                      nlohmann::json &json);

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
bool JsonToOneSevenLiveConfig(const nlohmann::json &json, OneSevenLiveConfig &config);
bool OneSevenLiveConfigToJson(const OneSevenLiveConfig &config, nlohmann::json &json);

// Function declarations for event-related JSON parsing (moved here after all struct definitions)
bool JsonToOneSevenLiveEventList(const nlohmann::json &eventListJson,
                                 QList<OneSevenLiveEventInfo> &eventList);
bool JsonToOneSevenLiveHashtags(const nlohmann::json &hashtagsJson,
                                QList<OneSevenLiveHashtag> &hashtags);
bool JsonToOneSevenLiveEventItems(const nlohmann::json &eventsJson,
                                  QList<OneSevenLiveEventItem> &events);
bool JsonToOneSevenLiveEventTags(const nlohmann::json &tagsJson, QList<OneSevenLiveEventTag> &tags);
bool JsonToOneSevenLiveEventSection(const nlohmann::json &eventJson, OneSevenLiveEventList &event);
bool JsonToOneSevenLiveSubtabs(const nlohmann::json &subtabsJson,
                               QList<OneSevenLiveSubtab> &subtabs);

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
bool JsonToOneSevenLiveArmySubscriptionLevels(const nlohmann::json &json,
                                              OneSevenLiveArmySubscriptionLevels &levels);
bool OneSevenLiveArmySubscriptionLevelsToJson(const OneSevenLiveArmySubscriptionLevels &levels,
                                              nlohmann::json &json);

// Gifts response struct
struct OneSevenLiveGiftsResponse {
    qint64 lastUpdate;
    QList<OneSevenLiveGift> gifts;
};

// Function declarations for gifts JSON conversion
bool JsonToOneSevenLiveGiftsResponse(const nlohmann::json &json,
                                     OneSevenLiveGiftsResponse &response);
bool OneSevenLiveGiftsResponseToJson(const OneSevenLiveGiftsResponse &response,
                                     nlohmann::json &json);

// Rock Zone Viewer information structs

// Label token struct for rock zone viewer
struct OneSevenLiveLabelToken {
    QString key;
};

// Army info user struct for rock zone viewer
struct OneSevenLiveArmyInfoUser {
    QString userID;
    QString displayName;
    QString picture;
    QString name;
    int level;
    QString openID;
    QString region;
    OneSevenLiveGloryroadInfo gloryroadInfo;
    int gloryroadMode;
};

// Army info struct for rock zone viewer
struct OneSevenLiveArmyInfo {
    OneSevenLiveArmyInfoUser user;
    int rank;
    qint64 pointContribution;
    int seniority;
    qint64 startTime;
    qint64 endTime;
    bool isOnLive;
    int newStatus;
    qint64 periodStartTime;
};

// User attributes struct for rock zone viewer
struct OneSevenLiveUserAttr {
    int level;
    int sentPoint;
    int checkinLevel;
    int checkinCount;
    QString checkinBdgURL;
    int noteStatus;
    int followStatus;
    int gloryroadMode;
    OneSevenLiveGloryroadInfo gloryroadInfo;
};

// Anonymous info struct for rock zone viewer
struct OneSevenLiveAnonymousInfo {
    bool isInvisible;
    QString pureText;
};

// Display user struct for rock zone viewer
struct OneSevenLiveDisplayUser {
    int armyRank;
    QString badgeURL;
    QString bgColor;
    QString checkinBdgURL;
    int checkinLevel;
    QString circleBadgeURL;
    QString displayName;
    QString fgColor;
    OneSevenLiveGloryroadInfo gloryroadInfo;
    int gloryroadMode;
    bool hasProgram;
    bool isDirty;
    bool isDirtyUser;
    bool isGuardian;
    bool isProducer;
    bool isStreamer;
    bool isVIP;
    int level;
    int mLevel;
    QString pfxBadgeURL;
    QString picture;
    int producer;
    int program;
    QString topRightIconURL;
    QString userID;
    QString vipCharmURL;
};

// Gift rank one struct
struct OneSevenLiveGiftRankOne {
    QString displayName;
    QString picture;
    qint64 timestampMs;
    QString userID;
};

// Rock zone viewer struct
struct OneSevenLiveRockZoneViewer {
    int type;
    QList<int> badgeTypes;  // just for merge badge
    OneSevenLiveArmyInfo armyInfo;
    OneSevenLiveLabelToken labelToken;
    OneSevenLiveUserAttr userAttr;
    OneSevenLiveAnonymousInfo anonymousInfo;
    int armyLevel;
    OneSevenLiveDisplayUser displayUser;
    OneSevenLiveGiftRankOne giftRankOne;
};

// Function declarations for gift rank one JSON conversion
bool JsonToOneSevenLiveGiftRankOne(const nlohmann::json &json,
                                   OneSevenLiveGiftRankOne &giftRankOne);
bool OneSevenLiveGiftRankOneToJson(const OneSevenLiveGiftRankOne &giftRankOne,
                                   nlohmann::json &json);

// Function declarations for display user JSON conversion
bool JsonToOneSevenLiveDisplayUser(const nlohmann::json &json,
                                   OneSevenLiveDisplayUser &displayUser);
bool OneSevenLiveDisplayUserToJson(const OneSevenLiveDisplayUser &displayUser,
                                   nlohmann::json &json);

// Function declarations for rock zone viewers JSON conversion
bool JsonToOneSevenLiveRockZoneViewer(const nlohmann::json &json,
                                      OneSevenLiveRockZoneViewer &viewer);
bool OneSevenLiveRockZoneViewerToJson(const OneSevenLiveRockZoneViewer &viewer,
                                      nlohmann::json &json);

bool JsonToOneSevenLiveRockViewers(const nlohmann::json &json,
                                   QList<OneSevenLiveRockZoneViewer> &viewers);

// Army name struct
struct OneSevenLiveArmyName {
    QString customName;
    QString defaultName;
};

// Army rank name struct
struct OneSevenLiveArmyRankName {
    int rank;
    int rankTier;
    QString customName;
    QString defaultName;
};

// Army name response struct
struct OneSevenLiveArmyNameResponse {
    OneSevenLiveArmyName armyName;
    QList<OneSevenLiveArmyRankName> rankName;
};

// Function declarations for army name JSON conversion
bool JsonToOneSevenLiveArmyNameResponse(const nlohmann::json &json,
                                        OneSevenLiveArmyNameResponse &response);
bool OneSevenLiveArmyNameResponseToJson(const OneSevenLiveArmyNameResponse &response,
                                        nlohmann::json &json);

// Poke request struct
struct OneSevenLivePokeRequest {
    bool isPokeBack;
    QString srcID;
    QString userID;
};

// Poke all request struct
struct OneSevenLivePokeAllRequest {
    QString liveStreamID;
    int receiverGroup;
};

// Poke response struct
struct OneSevenLivePokeResponse {
    QString pokeAnimationID;
};

// Function declarations for poke JSON conversion
bool JsonToOneSevenLivePokeResponse(const nlohmann::json &json, OneSevenLivePokeResponse &response);
bool OneSevenLivePokeRequestToJson(const OneSevenLivePokeRequest &request, nlohmann::json &json);
bool OneSevenLivePokeAllRequestToJson(const OneSevenLivePokeAllRequest &request,
                                      nlohmann::json &json);

// Change event request struct
struct OneSevenLiveChangeEventRequest {
    qint64 eventID;
};

// Function declarations for change event JSON conversion
bool OneSevenLiveChangeEventRequestToJson(const OneSevenLiveChangeEventRequest &request,
                                          nlohmann::json &json);
