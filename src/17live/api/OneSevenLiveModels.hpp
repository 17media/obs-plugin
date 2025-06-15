#pragma once

#include <QString>
#include <QVariantMap>
#include <QList>
#include <QStringList>
#include <QDateTime>

#include "json11.hpp"

using namespace json11;

// 定义当前的直播状态，包括未开播 0、直播创建好 1、开始直播 2
enum class OneSevenLiveStreamingStatus {
  NotStarted,
  Live,
  Streaming
};

// 通过索引获取Provider名称的函数
static QString GetProviderNameByIndex(int index) {
  switch (index) {
    case 0: return "DEFAULT";
    case 1: return "UCLOUD";
    case 2: return "QINIU";
    case 3: return "QCLOUD";
    case 4: return "WANSU";
    case 5: return "WANSU_LOW_LATENCY";
    case 6: return "WANSU_SPECIFIED_IP";
    case 7: return "SRS";
    case 8: return "CHT";
    case 9: return "AWS";
    case 10: return "QINIU_AUTH";
    case 11: return "WANSU_AUTH";
    case 12: return "LIVE17";
    case 13: return "WANSU_CDN";
    case 14: return "GOOGLE_CDN";
    case 15: return "AKAMAI_CDN";
    case 16: return "CLOUDFRONT_CDN";
    case 17: return "TENCENT";
    case 18: return "LIVE17_AUTH";
    default: return "UNKNOWN";
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

  // RTMP URL信息结构体
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

  // 拉流URL信息结构体
  struct OneSevenLivePullUrlsInfo {
    QList<OneSevenLiveRtmpUrl> rtmpURLs;
    qint64 seqNo;
  };

  // 商品信息结构体
  struct OneSevenLiveCommodityInfo {
    int type;
    int price;
    int amount;
    QString desc;
    qint64 endTimeMS;
  };

  // 活动图标信息结构体
  struct OneSevenLiveEventIcon {
    QString language;
    QString value;
  };

  // 活动信息结构体
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

  // 荣耀之路信息结构体
  struct OneSevenLiveGloryroadInfo {
    int point;
    int level;
    QString iconURL;
    QString badgeIconURL;
  };

  // 公会信息结构体
  struct OneSevenLiveClanInfo {
    int joinCount;
  };

  // 联赛信息结构体
  struct OneSevenLiveLeagueInfo {
    bool shouldShowEntrance;
  };

  // 用户信息结构体
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
    int clipPermissionDownload;  // 新增字段
  };

  // hashtag结构体
  struct OneSevenLiveHashtag {
    QString text;
    bool isOfficial;
  };

  // 主房间信息结构体
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
    OneSevenLiveArchiveConfig archiveConfig;  // 添加存档配置
    QString archiveID;                         // 添加存档ID
    bool hideGameMarquee;                      // 添加游戏跑马灯隐藏标志
    QStringList subtabs;
    QList<OneSevenLiveHashtag> lastUsedHashtags;
  };

  // 将Json转换为OneSevenLiveRoomInfo结构体
  bool JsonToOneSevenLiveRoomInfo(const Json &json, OneSevenLiveRoomInfo &roomInfo);
  bool OneSevenLiveRoomInfoToJson(const OneSevenLiveRoomInfo &roomInfo, Json &json);

  // 虚拟主播信息结构体
  struct OneSevenLiveVliverInfo {
    int vliverModel;
  };

  // 战队设定
  struct OneSevenLiveArmy {
    bool armyOnlyPN;
    bool enable;
    int requiredArmyRank;
    bool showOnHotPage;
  };

  // RTMP请求结构体
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


  // 成就值状态结构体
  struct OneSevenLiveAchievementValueState {
    bool isValueCarryOver;
    int initSeconds;
  };

  // RTMP响应结构体
  struct OneSevenLiveRtmpResponse {
    QString liveStreamID;
    QString streamID;
    QString rtmpURL;
    QString rtmpProvider;
    int messageProvider;
    Json firstStreamInfo;  // 使用Json类型因为它是一个空对象
    QList<OneSevenLiveRtmpUrl> rtmpURLs;  // 复用已有的OneSevenLiveRtmpUrl结构体
    OneSevenLiveAchievementValueState achievementValueState;
    bool subtitleEnabled;
  };

  bool JsonToOneSevenLiveRtmpResponse(const Json &json, OneSevenLiveRtmpResponse &response);

  // 关闭直播请求结构体
  struct OneSevenLiveCloseLiveRequest {
    QString userID;
    QString reason;
  };

  bool OneSevenLiveCloseLiveRequestToJson(const OneSevenLiveCloseLiveRequest &request, Json &json);

  // 活动标签结构体
  struct OneSevenLiveEventTag {
    QString ID;
    QString name;
  };

  // Ably Token响应结构体
  struct OneSevenLiveAblyTokenResponse {
    int provider;
    QString token;
    QStringList channels;
  };

  bool JsonToOneSevenLiveAblyTokenResponse(const Json &json, OneSevenLiveAblyTokenResponse &response);
  bool OneSevenLiveAblyTokenResponseToJson(const OneSevenLiveAblyTokenResponse &response, Json &json);

  // 活动事件结构体
  struct OneSevenLiveEventItem {
    qint64 ID;
    QString name;
    QString bannerURL;
    QString descriptionURL;
    QStringList tagIDs;
    qint64 endTime;
  };

  // 活动事件列表结构体
  struct OneSevenLiveEventList {
    QList<OneSevenLiveEventItem> events;
    bool notEligibleForAllEvents;
    int promotionIndex;
    QList<OneSevenLiveEventTag> tags;
    QString instructionURL;
  };

  // 自定义活动结构体
  struct OneSevenLiveCustomEvent {
    qint64 endTime;
    int status;
  };

  // 盲盒抽奖结构体
  struct OneSevenLiveBoxGacha {
    bool previousSettingStatus;
    QString availableEventID;
  };

  // 子标签结构体
  struct OneSevenLiveSubtab {
    QString displayName;
    QString ID;
  };

  struct OneSevenLiveStreamState {
    OneSevenLiveVliverInfo vliverInfo;
  };

  // 配置流媒体响应结构体
  struct OneSevenLiveConfigStreamer {
    OneSevenLiveEventList event;
    OneSevenLiveCustomEvent customEvent;
    OneSevenLiveBoxGacha boxGacha;
    QList<OneSevenLiveSubtab> subtabs;
    OneSevenLiveStreamState lastStreamState;
    int hashtagSelectLimit;
    int armyOnly;
  };

  // 解析JSON到OneSevenLiveConfigStreamerResponse结构体的函数声明
  bool JsonToOneSevenLiveConfigStreamer(const Json &json, OneSevenLiveConfigStreamer &response);
  bool OneSevenLiveConfigStreamerToJson(const OneSevenLiveConfigStreamer &response, Json &json);

  // 附加组件结构体
  struct OneSevenLiveAddOns {
    QMap<QString, int> features;
  };

  // 配置结构体，用于处理如下的json数据：
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

  // 解析JSON到OneSevenLiveConfig结构体的函数声明
  bool JsonToOneSevenLiveConfig(const Json &json, OneSevenLiveConfig &config);
  bool OneSevenLiveConfigToJson(const OneSevenLiveConfig &config, Json &json);

// 国际化令牌参数结构体
struct OneSevenLiveI18nTokenParam {
  QString value;
};

// 国际化令牌结构体
struct OneSevenLiveI18nToken {
  QString key;
  QList<OneSevenLiveI18nTokenParam> params;
};

// 军团订阅级别结构体
struct OneSevenLiveArmySubscriptionLevel {
  int rank;                      // 级别排名
  int subscribersAmount;         // 订阅者数量
  OneSevenLiveI18nToken i18nToken;  // 国际化令牌
};

// 军团订阅级别列表结构体
struct OneSevenLiveArmySubscriptionLevels {
  QList<OneSevenLiveArmySubscriptionLevel> subscriptionLevels;
};

// JSON转换函数声明
bool JsonToOneSevenLiveArmySubscriptionLevels(const Json &json, OneSevenLiveArmySubscriptionLevels &levels);
bool OneSevenLiveArmySubscriptionLevelsToJson(const OneSevenLiveArmySubscriptionLevels &levels, Json &json);
