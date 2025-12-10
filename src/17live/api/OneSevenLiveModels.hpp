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
    int premiumType = 0;
};

// hashtag struct
struct OneSevenLiveHashtag {
    QString text;
    bool isOfficial = false;
};

struct OneSevenLiveUserInfo {
    QString userID;
    QString openID;
    QString displayName;
    QString name;
    QString bio;
    QString picture;
    QString website;
    int followerCount = 0;
    int followingCount = 0;
    int receivedLikeCount = 0;
    int likeCount = 0;
    int isFollowing = 0;
    int isNotif = 0;
    int isBlocked = 0;
    qint64 followTime = 0;
    qint64 followRequestTime = 0;
    qint64 roomID = 0;
    QString privacyMode;
    int ballerLevel = 0;
    int postCount = 0;
    int isCelebrity = 0;
    int baller = 0;
    int level = 0;
    int followPrivacyMode = 0;
    QString revenueShareIndicator;
    int clanStatus = 0;
    QStringList badgeInfo;
    QString region;
    int hideAllPointToLeaderboard = 0;
    int enableShop = 0;
    QVariantMap monthlyVIPBadges;
    qint64 lastLiveTimestamp = 0;
    qint64 lastCreateLiveTimestamp = 0;
    QString lastLiveRegion;
    QStringList loyaltyInfo;
    bool streamerRecapEnable = false;
    int gloryroadMode = 0;
    QList<OneSevenLiveHashtag> lastUsedHashtags;
    bool newbieDisplayAllGiftTabsToast = false;
    int avatarOnboardingPhase = 0;
    bool isUnderaged = false;
    QStringList levelBadges;
    int isEmailVerified = 0;
    QString extIDAppleTransfer;
    QString commentShadowColor;
    bool isFreePrivateMsgEnabled = false;
    bool isVliverOnlyModeEnabled = false;
    OneSevenLiveOnliveInfo onliveInfo;
};

bool JsonToOneSevenLiveUserInfo(const nlohmann::json &json, OneSevenLiveUserInfo &userInfo);

struct OneSevenLiveAutoEnter {
    bool autoEnter = false;
    qint64 liveStreamID = 0;
};

struct OneSevenLiveLoginData {
    OneSevenLiveUserInfo userInfo;
    QString message;
    QString result;
    QString refreshToken;
    QString jwtAccessToken;
    QString accessToken;
    int giftModuleState = 0;
    QString word;
    QString abtestNewbieFocus;
    QString abtestNewbieGuidance;
    QString abtestNewbieGuide;
    bool showRecommend = false;
    OneSevenLiveAutoEnter autoEnterLive;
    int newbieEnhanceGuidanceStyle = 0;
    bool newbieGuidanceFocusMissionEnable = false;
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
    int errorCode = 0;
    QString errorMessage;
    QString errorTitle;
};

// RTMP URL information struct
struct OneSevenLiveRtmpUrl {
    int provider = 0;
    QString streamType;
    QString url;
    QString urlLowQuality;
    QString webUrl;
    QString webUrlLowQuality;
    QString urlHighQuality;
    int weight = 0;
    bool throttle = false;
};

bool JsonToOneSevenLiveRtmpUrl(const nlohmann::json &json, OneSevenLiveRtmpUrl &rtmpUrl);

// Pull stream URL information struct
struct OneSevenLivePullUrlsInfo {
    QList<OneSevenLiveRtmpUrl> rtmpURLs;
    qint64 seqNo = 0;
};

bool JsonToOneSevenLiveRtmpUrls(const nlohmann::json &json, QList<OneSevenLiveRtmpUrl> &rtmpUrls);
bool JsonToOneSevenLivePullUrlsInfo(const nlohmann::json &pullUrlsInfoJson,
                                    OneSevenLivePullUrlsInfo &pullUrlsInfo);

// Product information struct
struct OneSevenLiveCommodityInfo {
    int type = 0;
    int price = 0;
    int amount = 0;
    QString desc;
    qint64 endTimeMS = 0;
};

// Event icon information struct
struct OneSevenLiveEventIcon {
    QString language;
    QString value;
};

// Event information struct
struct OneSevenLiveEventInfo {
    qint64 ID = 0;
    int type = 0;
    QString icon;
    qint64 endTime = 0;
    int showTimer = 0;
    QString name;
    QString URL;
    int pageSize = 0;
    QString webViewTitle;
    QList<OneSevenLiveEventIcon> icons;
    QList<OneSevenLiveEventIcon> webViewTitles;
};

// Glory road information struct
struct OneSevenLiveGloryroadInfo {
    int point = 0;
    int level = 0;
    QString iconURL;
    QString badgeIconURL;
};

bool JsonToOneSevenLiveGloryroadInfo(const nlohmann::json &jsonData,
                                     OneSevenLiveGloryroadInfo &gloryroadInfo);
bool OneSevenLiveGloryroadInfoToJson(const OneSevenLiveGloryroadInfo &gloryroadInfo,
                                     nlohmann::json &jsonData);

// League information struct
struct OneSevenLiveLeagueInfo {
    bool shouldShowEntrance = false;
};

struct OneSevenLiveUserArmyInfo {
    int joinCount = 0;
};

// User information struct
struct OneSevenLiveStreamUserInfo : public OneSevenLiveUserInfo {
    QString gender;
    bool isChoice = false;
    bool isInternational = false;
    int adsOn = 0;
    qint64 subscribeExpireTime = 0;
    int experience = 0;
    QString version;
    QString deviceType;
    QString createClanID;
    OneSevenLiveUserArmyInfo clanInfo;
    int chatMuteDuration = 0;
    QString language;
    QString registerRegion;
    int vipGroupType = 0;
    int followReminder = 0;
    OneSevenLiveLeagueInfo leagueInfo;
    bool hasVipPurchase = false;
    bool disableMakeLiveHotToast = false;
    OneSevenLiveGloryroadInfo gloryroadInfo;
};

struct OneSevenLiveArchiveConfig {
    bool autoRecording = false;
    bool autoPublish = false;
    int clipPermission = 0;
    int clipPermissionDownload = 0;  // New field
};

bool JsonToOneSevenLiveArchiveConfig(const nlohmann::json &json,
                                     OneSevenLiveArchiveConfig &archiveConfig);

// Main room information struct
struct OneSevenLiveRoomInfo {
    QString userID;
    int streamerType = 0;
    QString streamType;
    int status = 0;
    QString caption;
    QString thumbnail;
    QList<OneSevenLiveRtmpUrl> rtmpUrls;
    OneSevenLivePullUrlsInfo pullURLsInfo;
    int allowCallin = 0;
    QString restreamerOpenID;
    QString streamID;
    qint64 liveStreamID = 0;
    qint64 endTime = 0;
    qint64 beginTime = 0;
    qint64 receivedLikeCount = 0;
    int duration = 0;
    int viewerCount = 0;
    qint64 totalViewTime = 0;
    int liveViewerCount = 0;
    int audioOnly = 0;
    QString locationName;
    QString coverPhoto;
    double latitude = 0.0;
    double longitude = 0.0;
    int shareLocation = 0;
    int followerOnlyChat = 0;
    int chatAvailable = 0;
    int replayCount = 0;
    int replayAvailable = 0;
    int numberOfChunks = 0;
    int canSendGift = 0;
    OneSevenLiveStreamUserInfo userInfo;
    bool landscape = false;
    bool mute = false;
    int birthdayState = 0;
    int dayBeforeBirthday = 0;
    int achievementValue = 0;
    int mediaMessageReadState = 0;
    QString region;
    int specialTag = 0;
    QString guardianUserID;
    QString guardianPicture;
    QString campaignIcon;
    QString campaignURL;
    qint64 campaignEndTime = 0;
    int campaignShowTimer = 0;
    int campaignSize = 0;
    QString campaignTitle;
    int commodityState = 0;
    OneSevenLiveCommodityInfo commodityInfo;
    bool canSellCommodity = false;
    int gridStyle = 0;
    QString device;
    QList<OneSevenLiveEventInfo> eventList;
    OneSevenLiveArchiveConfig archiveConfig;  // Add archive configuration
    QString archiveID;                        // Add archive ID
    bool hideGameMarquee = false;                     // Add game marquee hide flag
    bool enableOBSGroupCall = false;                  // Add OBS group call enable flag
    QStringList subtabs;
    QList<OneSevenLiveHashtag> lastUsedHashtags;
};

// Convert Json to OneSevenLiveRoomInfo struct
bool JsonToOneSevenLiveRoomInfo(const nlohmann::json &json, OneSevenLiveRoomInfo &roomInfo);
bool OneSevenLiveRoomInfoToJson(const OneSevenLiveRoomInfo &roomInfo, nlohmann::json &json);

// Virtual streamer information struct
struct OneSevenLiveVliverInfo {
    int vliverModel = 0;
};

// Army settings
struct OneSevenLiveArmy {
    bool armyOnlyPN = false;
    bool enable = false;
    int requiredArmyRank = 0;
    bool showOnHotPage = false;
};

// RTMP request struct
struct OneSevenLiveRtmpRequest {
    QString userID;
    QString caption;
    QString device;
    qint64 eventID = 0;
    QStringList hashtags;
    bool landscape = false;
    int streamerType = 0;
    QString subtabID;
    OneSevenLiveArchiveConfig archiveConfig;
    OneSevenLiveVliverInfo vliverInfo;
    OneSevenLiveArmy armyOnly;
    bool enableOBSGroupCall = false;
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
    bool isValueCarryOver = false;
    int initSeconds = 0;
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
    int messageProvider = 0;
    Json firstStreamInfo;                 // Use Json type because it's an empty object
    QList<OneSevenLiveRtmpUrl> rtmpURLs;  // Reuse existing OneSevenLiveRtmpUrl struct
    OneSevenLiveAchievementValueState achievementValueState;
    bool subtitleEnabled = false;
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
    int provider = 0;
    QString token;
    QStringList channels;
};

bool JsonToOneSevenLiveAblyTokenResponse(const nlohmann::json &json,
                                         OneSevenLiveAblyTokenResponse &response);
bool OneSevenLiveAblyTokenResponseToJson(const OneSevenLiveAblyTokenResponse &response,
                                         nlohmann::json &json);

// Event item struct
struct OneSevenLiveEventItem {
    qint64 ID = 0;
    QString name;
    QString bannerURL;
    QString descriptionURL;
    QStringList tagIDs;
    qint64 endTime = 0;
};

// Event list struct
struct OneSevenLiveEventList {
    QList<OneSevenLiveEventItem> events;
    bool notEligibleForAllEvents = false;
    int promotionIndex = 0;
    QList<OneSevenLiveEventTag> tags;
    QString instructionURL;
};

// Gift struct
struct OneSevenLiveGift {
    QString giftID;
    int isHidden = 0;
    int regionMode = 0;
    QString name;
    int point = 0;
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
    int status = 0;
    QString eventName;
    QString description;
    qint64 startTime = 0;
    qint64 endTime = 0;
    qint64 realEndTime = 0;
    bool isAchieved = false;
    QList<QString> giftIDs;
    QList<OneSevenLiveGift> gifts;
    qint64 goalPoints = 0;
    qint64 dailyGoalPoints = 0;
    QString displayStatus;
    QList<Json> rewards;  // Using Json type because rewards structure is not defined
    qint64 currentGoalPoints;
    qint64 currentDailyGoalPoints;
};

// Stop custom event request struct
struct OneSevenLiveCustomEventStatusRequest {
    int status = 0;  // Status 2 means stop
    QString userID;
};

bool JsonToOneSevenLiveCustomEvent(const nlohmann::json &json, OneSevenLiveCustomEvent &response);
bool OneSevenLiveCustomEventToJson(const OneSevenLiveCustomEvent &request, nlohmann::json &json);
bool OneSevenLiveChangeCustomEventStatusRequestToJson(
    const OneSevenLiveCustomEventStatusRequest &request, nlohmann::json &json);

// Box gacha struct
struct OneSevenLiveBoxGacha {
    bool previousSettingStatus = false;
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
    int type = 0;
    QString name;
    QList<OneSevenLiveGift> gifts;
};

// Gift tabs response struct
struct OneSevenLiveGiftTabsResponse {
    qint64 giftLastUpdate = 0;
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
    int hashtagSelectLimit = 0;
    int armyOnly = 0;
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
    int rank = 0;                         // Level ranking
    int subscribersAmount = 0;            // Number of subscribers
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
    qint64 lastUpdate = 0;
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
    int level = 0;
    QString openID;
    QString region;
    OneSevenLiveGloryroadInfo gloryroadInfo;
    int gloryroadMode = 0;
};

// Army info struct for rock zone viewer
struct OneSevenLiveArmyInfo {
    OneSevenLiveArmyInfoUser user;
    int rank = 0;
    qint64 pointContribution = 0;
    int seniority = 0;
    qint64 startTime = 0;
    qint64 endTime = 0;
    bool isOnLive = false;
    int newStatus = 0;
    qint64 periodStartTime = 0;
};

// User attributes struct for rock zone viewer
struct OneSevenLiveUserAttr {
    int level = 0;
    int sentPoint = 0;
    int checkinLevel = 0;
    int checkinCount = 0;
    QString checkinBdgURL;
    int noteStatus = 0;
    int followStatus = 0;
    int gloryroadMode = 0;
    OneSevenLiveGloryroadInfo gloryroadInfo;
};

// Anonymous info struct for rock zone viewer
struct OneSevenLiveAnonymousInfo {
    bool isInvisible = false;
    QString pureText;
};

// Display user struct for rock zone viewer
struct OneSevenLiveDisplayUser {
    int armyRank = 0;
    QString badgeURL;
    QString bgColor;
    QString checkinBdgURL;
    int checkinLevel = 0;
    QString circleBadgeURL;
    QString displayName;
    QString fgColor;
    OneSevenLiveGloryroadInfo gloryroadInfo;
    int gloryroadMode = 0;
    bool hasProgram = false;
    bool isDirty = false;
    bool isDirtyUser = false;
    bool isGuardian = false;
    bool isProducer = false;
    bool isStreamer = false;
    bool isVIP = false;
    int level = 0;
    int mLevel = 0;
    QString pfxBadgeURL;
    QString picture;
    int producer = 0;
    int program = 0;
    QString topRightIconURL;
    QString userID;
    QString vipCharmURL;
};

// Gift rank one struct
struct OneSevenLiveGiftRankOne {
    QString displayName;
    QString picture;
    qint64 timestampMs = 0;
    QString userID;
};

// Guardian Owner struct
struct OneSevenLiveGuardianOwner {
    QString userID;
    QString displayName;
    QString picture;
    QString name;
    int level = 0;
    QString openID;
    QString region;
    int gloryroadMode = 0;
};

// Guardian struct
struct OneSevenLiveGuardian {
    OneSevenLiveGuardianOwner owner;
    int bidPrice = 0;
    qint64 expireTime = 0;
};

// Rock Zone Viewer struct
struct OneSevenLiveRockZoneViewer {
    int type = 0;
    QList<int> badgeTypes;  // just for merge badge
    OneSevenLiveArmyInfo armyInfo;
    OneSevenLiveLabelToken labelToken;
    OneSevenLiveUserAttr userAttr;
    OneSevenLiveAnonymousInfo anonymousInfo;
    int armyLevel = 0;
    OneSevenLiveDisplayUser displayUser;
    OneSevenLiveGiftRankOne giftRankOne;
    OneSevenLiveGuardian guardian;
};

// Function declarations for guardian JSON conversion
bool JsonToOneSevenLiveGuardianOwner(const nlohmann::json &json, OneSevenLiveGuardianOwner &owner);
bool OneSevenLiveGuardianOwnerToJson(const OneSevenLiveGuardianOwner &owner, nlohmann::json &json);
bool JsonToOneSevenLiveGuardian(const nlohmann::json &json, OneSevenLiveGuardian &guardian);
bool OneSevenLiveGuardianToJson(const OneSevenLiveGuardian &guardian, nlohmann::json &json);

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

QList<OneSevenLiveRockZoneViewer> SortOneSevenLiveRockZoneViewers(
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
