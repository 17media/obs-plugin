/**
 * 统一消息类型定义
 * 用于所有平台的消息格式标准化
 */

export const Platform = {
  ALL: 'all',
  SEVENTEEN_LIVE: '17live',
  YOUTUBE: 'youtube',
  TWITCH: 'twitch'
};

export const MessageType = {
  COMMENT: 'comment',
  GIFT: 'gift',
  JOIN: 'join',
  POKE: 'poke',
  AI_COHOST: 'ai_cohost',
  LUCKY_BAG: 'lucky_bag'
};

/**
 * 标准化消息格式
 */
export class UnifiedMessage {
  constructor({
    id,
    platform,
    type,
    content,
    author,
    timestamp,
    rawData,
    metadata = {}
  }) {
    this.id = id;
    this.platform = platform;
    this.type = type;
    this.content = content;
    this.author = author;
    this.timestamp = timestamp;
    this.rawData = rawData;
    this.metadata = metadata;
  }

  /**
   * 获取平台图标路径
   */
  getPlatformIcon() {
    const iconMap = {
      [Platform.SEVENTEEN_LIVE]: '/images/17live.svg',
      [Platform.YOUTUBE]: '/images/youtube.svg',
      [Platform.TWITCH]: '/images/twitch.svg'
    };
    return iconMap[this.platform] || '/images/17live.svg';
  }

  /**
   * 转换为Immutable对象（兼容现有代码）
   */
  toImmutable() {
    const { fromJS } = require('immutable');
    return fromJS({
      id: this.id,
      platform: this.platform,
      type: this.type,
      content: this.content,
      author: this.author,
      timestamp: this.timestamp,
      rawData: this.rawData,
      metadata: this.metadata,
      platformIcon: this.getPlatformIcon()
    });
  }
}

/**
 * 消息作者信息
 */
export class MessageAuthor {
  constructor({
    id,
    name,
    displayName,
    avatar,
    level,
    badges = [],
    isStreamer = false
  }) {
    this.id = id;
    this.name = name;
    this.displayName = displayName;
    this.avatar = avatar;
    this.level = level;
    this.badges = badges;
    this.isStreamer = isStreamer;
  }
}