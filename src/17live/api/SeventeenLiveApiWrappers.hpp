#pragma once

#include "json11.hpp"

#include <QString>
#include <QList>
#include <QVariant>
#include <QVariantMap>
#include <QStringList>
#include <QObject>

namespace seventeenlive {

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

class SeventeenLiveApiWrappers : public QObject {
  Q_OBJECT

  bool TryInsertCommand(const char *url, const char *content_type, std::string request_type, const char *data,
      json11::Json &ret, long *error_code = nullptr, int data_size = 0, bool token_required = true);
  bool UpdateAccessToken();
  bool InsertCommand(const char *url, const char *content_type, std::string request_type, const char *data,
   json11::Json &ret, int data_size = 0, bool token_required = true);


public:
  SeventeenLiveApiWrappers();
  SeventeenLiveApiWrappers(std::string token_);
  
  bool Login(const QString &username, const QString &password, SeventeenLiveLoginData &loginData);

  bool GetSelfInfo();

  bool CommonRequest(const std::string action);

  /**
   * @brief 对字符串进行MD5加密
   * @param str 需要加密的字符串
   * @return 返回MD5加密后的字符串（16进制格式）
   */
  static QString md5(const QString& str);

  /**
   * @brief 获取当前时间的毫秒级时间戳
   * @return int64_t 返回自 1970-01-01 00:00:00 UTC 以来的毫秒数
   */
  static int64_t getCurrentTimestampMs();

  QString getLastErrorMessage() const { return lastErrorMessage; }

protected:
	std::string refresh_token;
	std::string token;
	bool implicit = false;
	uint64_t expire_time = 0;
	int currentScopeVer = 0;

private:
  QString lastErrorMessage;
};

} // namespace seventeenlive
