#pragma once

#include <QString>
#include <QVariantMap>
#include <QList>
#include <QStringList>
#include <QDateTime>

#include "json11.hpp"

using namespace json11;

  // 定义当前的直播状态，包括未开播 0、直播创建好 1、开始直播 2
  enum class SeventeenLiveStreamingStatus {
    NotStarted,
    Live,
    Streaming
  };

  struct SeventeenLiveAPIResponse {
    QString key;
    QString data;
  };
  
  struct SeventeenLiveAPIResult {
    QString result;
    QString message;
  };
  
  struct SeventeenLiveUserInfo {
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
  };
  
  struct SeventeenLiveAutoEnter {
    bool autoEnter;
    qint64 liveStreamID;
  };
  
  struct SeventeenLiveLoginData {
    SeventeenLiveUserInfo userInfo;
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
    SeventeenLiveAutoEnter autoEnterLive;
    int newbieEnhanceGuidanceStyle;
    bool newbieGuidanceFocusMissionEnable;
  };

  bool JsonToSeventeenLiveLoginData(const Json &json, SeventeenLiveLoginData &loginData);
  
  /* struct for json data
  {
    "errorCode": 7,
    "errorMessage": "token invalid",
    "errorTitle": ""
  }
  */
  struct SeventeenLiveError {
    int errorCode;
    QString errorMessage;
    QString errorTitle;
  };

  // RTMP URL信息结构体
  struct SeventeenLiveRtmpUrl {
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

  // 拉流URL信息结构体
  struct SeventeenLivePullUrlsInfo {
    QList<SeventeenLiveRtmpUrl> rtmpURLs;
    qint64 seqNo;
  };

  // 商品信息结构体
  struct SeventeenLiveCommodityInfo {
    int type;
    int price;
    int amount;
    QString desc;
    qint64 endTimeMS;
  };

  // 活动图标信息结构体
  struct SeventeenLiveEventIcon {
    QString language;
    QString value;
  };

  // 活动信息结构体
  struct SeventeenLiveEventInfo {
    qint64 ID;
    int type;
    QString icon;
    qint64 endTime;
    int showTimer;
    QString name;
    QString URL;
    int pageSize;
    QString webViewTitle;
    QList<SeventeenLiveEventIcon> icons;
    QList<SeventeenLiveEventIcon> webViewTitles;
  };

  // 荣耀之路信息结构体
  struct SeventeenLiveGloryroadInfo {
    int point;
    int level;
    QString iconURL;
    QString badgeIconURL;
  };

  // 公会信息结构体
  struct SeventeenLiveClanInfo {
    int joinCount;
  };

  // 联赛信息结构体
  struct SeventeenLiveLeagueInfo {
    bool shouldShowEntrance;
  };

  // 用户信息结构体
  struct SeventeenLiveStreamUserInfo : public SeventeenLiveUserInfo {
    QString gender;
    bool isChoice;
    bool isInternational;
    int adsOn;
    qint64 subscribeExpireTime;
    int experience;
    QString version;
    QString deviceType;
    QString createClanID;
    SeventeenLiveClanInfo clanInfo;
    int chatMuteDuration;
    QString language;
    QString registerRegion;
    int vipGroupType;
    int followReminder;
    SeventeenLiveLeagueInfo leagueInfo;
    bool hasVipPurchase;
    bool disableMakeLiveHotToast;
    SeventeenLiveGloryroadInfo gloryroadInfo;
  };

  struct SeventeenLiveArchiveConfig {
    bool autoRecording;
    bool autoPublish;
    int clipPermission;
    int clipPermissionDownload;  // 新增字段
  };

  // hashtag结构体
  struct SeventeenLiveHashtag {
    QString text;
    bool isOfficial;
  };

  // 主房间信息结构体
  struct SeventeenLiveRoomInfo {
    QString userID;
    int streamerType;
    QString streamType;
    int status;
    QString caption;
    QString thumbnail;
    QList<SeventeenLiveRtmpUrl> rtmpUrls;
    SeventeenLivePullUrlsInfo pullURLsInfo;
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
    SeventeenLiveStreamUserInfo userInfo;
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
    SeventeenLiveCommodityInfo commodityInfo;
    bool canSellCommodity;
    int gridStyle;
    QString device;
    QList<SeventeenLiveEventInfo> eventList;
    SeventeenLiveArchiveConfig archiveConfig;  // 添加存档配置
    QString archiveID;                         // 添加存档ID
    bool hideGameMarquee;                      // 添加游戏跑马灯隐藏标志
    QStringList subtabs;
    QList<SeventeenLiveHashtag> lastUsedHashtags;
  };

  // 将Json转换为SeventeenLiveRoomInfo结构体
  bool JsonToSeventeenLiveRoomInfo(const Json &json, SeventeenLiveRoomInfo &roomInfo);
  bool SeventeenLiveRoomInfoToJson(const SeventeenLiveRoomInfo &roomInfo, Json &json);

  // 虚拟主播信息结构体
  struct SeventeenLiveVliverInfo {
    int vliverModel;
  };

  // 战队设定
  struct SeventeenLiveArmy {
    bool armyOnlyPN;
    bool enable;
    int requiredArmyRank;
    bool showOnHotPage;
  };

  // RTMP请求结构体
  struct SeventeenLiveRtmpRequest {
    QString userID;
    QString caption;
    QString device;
    qint64 eventID;
    QStringList hashtags;
    bool landscape;
    int streamerType;
    QString subtabID;
    SeventeenLiveArchiveConfig archiveConfig;
    SeventeenLiveVliverInfo vliverInfo;
    SeventeenLiveArmy armyOnly;
  };

  bool SeventeenLiveRtmpRequestToJson(const SeventeenLiveRtmpRequest &request, Json &json);
  bool JsonToSeventeenLiveRtmpRequest(const Json &json, SeventeenLiveRtmpRequest &request);

  struct SeventeenLiveStreamInfo {
    SeventeenLiveRtmpRequest request;
    QString categoryName;
    QDateTime createdAt;
    QString streamUuid;
  };

  bool SeventeenLiveStreamInfoToJson(const SeventeenLiveStreamInfo &streamInfo, Json &json);
  bool JsonToSeventeenLiveStreamInfo(const Json &json, SeventeenLiveStreamInfo &streamInfo);


  // 成就值状态结构体
  struct SeventeenLiveAchievementValueState {
    bool isValueCarryOver;
    int initSeconds;
  };

  // RTMP响应结构体
  struct SeventeenLiveRtmpResponse {
    QString liveStreamID;
    QString streamID;
    QString rtmpURL;
    QString rtmpProvider;
    int messageProvider;
    Json firstStreamInfo;  // 使用Json类型因为它是一个空对象
    QList<SeventeenLiveRtmpUrl> rtmpURLs;  // 复用已有的SeventeenLiveRtmpUrl结构体
    SeventeenLiveAchievementValueState achievementValueState;
    bool subtitleEnabled;
  };

  bool JsonToSeventeenLiveRtmpResponse(const Json &json, SeventeenLiveRtmpResponse &response);

  // 关闭直播请求结构体
  struct SeventeenLiveCloseLiveRequest {
    QString userID;
    QString reason;
  };

  bool SeventeenLiveCloseLiveRequestToJson(const SeventeenLiveCloseLiveRequest &request, Json &json);

  // 活动标签结构体
  struct SeventeenLiveEventTag {
    QString ID;
    QString name;
  };

  // Ably Token响应结构体
  struct SeventeenLiveAblyTokenResponse {
    int provider;
    QString token;
    QStringList channels;
  };

  bool JsonToSeventeenLiveAblyTokenResponse(const Json &json, SeventeenLiveAblyTokenResponse &response);
  bool SeventeenLiveAblyTokenResponseToJson(const SeventeenLiveAblyTokenResponse &response, Json &json);

  // 活动事件结构体
  struct SeventeenLiveEventItem {
    qint64 ID;
    QString name;
    QString bannerURL;
    QString descriptionURL;
    QStringList tagIDs;
    qint64 endTime;
  };

  // 活动事件列表结构体
  struct SeventeenLiveEventList {
    QList<SeventeenLiveEventItem> events;
    bool notEligibleForAllEvents;
    int promotionIndex;
    QList<SeventeenLiveEventTag> tags;
    QString instructionURL;
  };

  // 自定义活动结构体
  struct SeventeenLiveCustomEvent {
    qint64 endTime;
    int status;
  };

  // 盲盒抽奖结构体
  struct SeventeenLiveBoxGacha {
    bool previousSettingStatus;
    QString availableEventID;
  };

  // 子标签结构体
  struct SeventeenLiveSubtab {
    QString displayName;
    QString ID;
  };

  struct SeventeenLiveStreamState {
    SeventeenLiveVliverInfo vliverInfo;
  };

  // 配置流媒体响应结构体
  struct SeventeenLiveConfigStreamer {
    SeventeenLiveEventList event;
    SeventeenLiveCustomEvent customEvent;
    SeventeenLiveBoxGacha boxGacha;
    QList<SeventeenLiveSubtab> subtabs;
    SeventeenLiveStreamState lastStreamState;
    int hashtagSelectLimit;
  };

  // 解析JSON到SeventeenLiveConfigStreamerResponse结构体的函数声明
  bool JsonToSeventeenLiveConfigStreamer(const Json &json, SeventeenLiveConfigStreamer &response);
  bool SeventeenLiveConfigStreamerToJson(const SeventeenLiveConfigStreamer &response, Json &json);
