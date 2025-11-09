/**
 * Twitch平台处理器
 * 使用TMI.js处理Twitch聊天消息
 */

import { BasePlatform } from '../BasePlatform';
import tmi from 'tmi.js';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { MsgType_COMMENT, MsgType_JOIN_ROOM, MsgType_NEW_GIFT } from '@/lib/constants';

export class TwitchPlatform extends BasePlatform {
  constructor() {
    super('twitch', 'Twitch');
    this.client = null;
    this.channel = null;
    this.connectionConfig = null;
    this.devMocksInjected = false;

    // 在开发环境下提前注入 mock 数据，用于样式预览（无需连接）
    if (process.env.NODE_ENV === 'development') {
      setTimeout(() => {
        try {
          this.injectDevMocks();
        } catch (e) {
          console.warn('Twitch mock 注入失败:', e);
        }
      }, 300);
    }
  }

  async connect(config) {
    try {
      const { channel, username, oauth, reconnect = true } = config;
      
      if (!channel) {
        throw new Error('Twitch配置错误：缺少频道名称');
      }

      this.channel = channel.toLowerCase().replace('#', '');
      this.connectionConfig = {
        channels: [this.channel],
        reconnect: reconnect
      };

      // 如果有OAuth令牌，使用它
      if (oauth) {
        this.connectionConfig.identity = {
          username: username || 'justinfan12345', // 匿名用户名
          password: oauth
        };
      } else {
        // 匿名连接
        this.connectionConfig.identity = {
          username: 'justinfan12345',
          password: 'oauth:justinfan12345'
        };
      }

      // 创建TMI客户端
      this.client = new tmi.Client(this.connectionConfig);

      // 设置事件监听器
      this.setupEventListeners();

      // 连接到Twitch
      await this.client.connect();
      
      this.isConnected = true;
      this.emit('connected', { platform: this.platformId, channel: this.channel });
      // 若连接后仍未注入 mock，在开发环境下兜底一次
      if (process.env.NODE_ENV === 'development' && !this.devMocksInjected) {
        this.injectDevMocks();
      }
      
    } catch (error) {
      console.error('Twitch连接失败:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
  }

  // 开发环境：注入 mock 数据（加入/留言），与统一结构兼容
  injectDevMocks() {
    if (this.devMocksInjected || process.env.NODE_ENV !== 'development') return;

    const mockChat = {
      type: 'chat',
      channel: `#${this.channel || 'test'}`,
      tags: {
        id: nanoid(),
        'display-name': 'Twitch Tester',
        username: 'twitch_tester',
        'user-id': 'TWITCH_TESTER_ID',
        color: '#9146FF',
      },
      message: '这是来自 Twitch 的测试留言 ~',
      timestamp: Date.now(),
    };

    const mockJoin = {
      type: 'join',
      channel: `#${this.channel || 'test'}`,
      username: 'twitch_visitor',
      timestamp: Date.now(),
    };

    const mocks = [
      this.processRawMessage(mockChat),
      this.processRawMessage(mockJoin),
    ].filter(Boolean);

    mocks.forEach((mock) => this.enqueueMessage(mock));
    this.devMocksInjected = true;
  }
  async disconnect() {
    try {
      if (this.client) {
        await this.client.disconnect();
        this.client = null;
      }
      this.isConnected = false;
      this.emit('disconnected', { platform: this.platformId });
    } catch (error) {
      console.error('Twitch断开连接失败:', error);
      throw error;
    }
  }

  setupEventListeners() {
    if (!this.client) return;

    // 聊天消息
    this.client.on('message', (channel, tags, message, self) => {
      this.handleChatMessage(channel, tags, message, self);
    });

    // 加入频道
    this.client.on('join', (channel, username, self) => {
      if (!self) { // 不是自己加入
        this.handleJoinMessage(channel, username);
      }
    });

    // 订阅/关注事件
    this.client.on('subscription', (channel, username, method, message, userstate) => {
      this.handleSubscription(channel, username, method, message, userstate);
    });

    this.client.on('resub', (channel, username, months, message, userstate, methods) => {
      this.handleResub(channel, username, months, message, userstate, methods);
    });

    // 礼物/捐赠
    this.client.on('cheer', (channel, userstate, message) => {
      this.handleCheer(channel, userstate, message);
    });

    // 连接事件
    this.client.on('connected', (addr, port) => {
      console.log(`Twitch已连接到: ${addr}:${port}`);
    });

    this.client.on('disconnected', (reason) => {
      console.log('Twitch已断开连接:', reason);
      if (this.isConnected) {
        this.isConnected = false;
        this.emit('disconnected', { platform: this.platformId, reason });
      }
    });

    this.client.on('reconnect', () => {
      console.log('Twitch正在重新连接...');
    });

    // 错误处理
    this.client.on('error', (error) => {
      console.error('Twitch错误:', error);
      this.emit('error', { platform: this.platformId, error });
    });
  }

  handleChatMessage(channel, tags, message, self) {
    try {
      // 忽略自己的消息
      if (self) return;

      const unifiedMessage = this.processRawMessage({
        type: 'chat',
        channel,
        tags,
        message,
        timestamp: Date.now()
      });

      if (unifiedMessage) {
        this.enqueueMessage(unifiedMessage);
      }
    } catch (error) {
      console.error('处理Twitch聊天消息失败:', error);
      this.emit('error', { platform: this.platformId, error });
    }
  }

  handleJoinMessage(channel, username) {
    try {
      const unifiedMessage = this.processRawMessage({
        type: 'join',
        channel,
        username,
        timestamp: Date.now()
      });

      if (unifiedMessage) {
        this.enqueueMessage(unifiedMessage);
      }
    } catch (error) {
      console.error('处理Twitch加入消息失败:', error);
      this.emit('error', { platform: this.platformId, error });
    }
  }

  handleSubscription(channel, username, method, message, userstate) {
    try {
      const unifiedMessage = this.processRawMessage({
        type: 'subscription',
        channel,
        username,
        method,
        message,
        userstate,
        timestamp: Date.now()
      });

      if (unifiedMessage) {
        this.enqueueMessage(unifiedMessage);
      }
    } catch (error) {
      console.error('处理Twitch订阅消息失败:', error);
      this.emit('error', { platform: this.platformId, error });
    }
  }

  handleResub(channel, username, months, message, userstate, methods) {
    try {
      const unifiedMessage = this.processRawMessage({
        type: 'resub',
        channel,
        username,
        months,
        message,
        userstate,
        methods,
        timestamp: Date.now()
      });

      if (unifiedMessage) {
        this.enqueueMessage(unifiedMessage);
      }
    } catch (error) {
      console.error('处理Twitch重新订阅消息失败:', error);
      this.emit('error', { platform: this.platformId, error });
    }
  }

  handleCheer(channel, userstate, message) {
    try {
      const unifiedMessage = this.processRawMessage({
        type: 'cheer',
        channel,
        userstate,
        message,
        timestamp: Date.now()
      });

      if (unifiedMessage) {
        this.enqueueMessage(unifiedMessage);
      }
    } catch (error) {
      console.error('处理Twitch欢呼消息失败:', error);
      this.emit('error', { platform: this.platformId, error });
    }
  }

  processRawMessage(rawData) {
    try {
      const { type } = rawData;
      
      switch (type) {
        case 'chat':
          return this.processChatMessage(rawData);
        case 'join':
          return this.processJoinMessage(rawData);
        case 'subscription':
          return this.processSubscriptionMessage(rawData);
        case 'resub':
          return this.processResubMessage(rawData);
        case 'cheer':
          return this.processCheerMessage(rawData);
        default:
          console.warn('未知的Twitch消息类型:', type);
          return null;
      }
    } catch (error) {
      console.error('处理Twitch原始消息失败:', error);
      return null;
    }
  }

  // 构建与 Chat 组件兼容的 Immutable 内容
  prepareIndexedChat(base) {
    const type = base?.type;
    const id = base?.tags?.id || base?.id || nanoid();

    if (type === 'chat') {
      const { tags, message } = base;
      return fromJS({
        id,
        messageType: MsgType_COMMENT,
        displayName: tags['display-name'] || tags.username,
        openID: tags['user-id'],
        userID: tags['user-id'],
        content: message,
        level: 1,
        name: { textColor: tags.color || '#9146FF' },
        comment: { textColor: '#e5e7eb' },
        backgroundColor: '',
        streamerInfo: null,
      });
    }

    if (type === 'join') {
      const { username } = base;
      return fromJS({
        id,
        messageType: MsgType_JOIN_ROOM,
        displayName: username,
        openID: username,
        userID: username,
        content: `${username} 加入了频道`,
        level: 1,
        name: { textColor: '#9146FF' },
        comment: { textColor: '#e5e7eb' },
        backgroundColor: '',
        streamerInfo: null,
      });
    }

    if (type === 'subscription' || type === 'resub' || type === 'cheer') {
      const { username, months, userstate, message } = base;
      const bits = parseInt(userstate?.bits) || 0;
      const giftName = type === 'subscription' ? '订阅' : (type === 'resub' ? `订阅 ${months} 个月` : 'Bits');
      const count = type === 'cheer' ? bits : (months || 1);

      return fromJS({
        id,
        messageType: MsgType_NEW_GIFT,
        displayName: userstate?.['display-name'] || username,
        openID: userstate?.['user-id'] || username,
        userID: userstate?.['user-id'] || username,
        content: message || '',
        gift: fromJS({ name: giftName, point: count, icon: '' }),
        level: 1,
        name: { textColor: '#9146FF' },
        comment: { textColor: '#e5e7eb' },
        backgroundColor: '',
        streamerInfo: null,
      });
    }

    // fallback 普通评论
    return fromJS({
      id,
      messageType: MsgType_COMMENT,
      displayName: base?.username || 'Twitch用户',
      openID: base?.username,
      userID: base?.username,
      content: base?.message || '',
      level: 1,
      name: { textColor: '#9146FF' },
      comment: { textColor: '#e5e7eb' },
      backgroundColor: '',
      streamerInfo: null,
    });
  }

  processChatMessage(rawData) {
    const immutableContent = this.prepareIndexedChat({ ...rawData, type: 'chat' });
    return {
      id: immutableContent.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content: immutableContent,
    };
  }

  processJoinMessage(rawData) {
    const immutableContent = this.prepareIndexedChat({ ...rawData, type: 'join' });
    return {
      id: immutableContent.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content: immutableContent,
    };
  }

  processSubscriptionMessage(rawData) {
    const immutableContent = this.prepareIndexedChat({ ...rawData, type: 'subscription' });
    return {
      id: immutableContent.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content: immutableContent,
    };
  }

  processResubMessage(rawData) {
    const immutableContent = this.prepareIndexedChat({ ...rawData, type: 'resub' });
    return {
      id: immutableContent.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content: immutableContent,
    };
  }

  processCheerMessage(rawData) {
    const immutableContent = this.prepareIndexedChat({ ...rawData, type: 'cheer' });
    return {
      id: immutableContent.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content: immutableContent,
    };
  }

  parseBadges(badgesString) {
    if (!badgesString) return [];
    
    const badges = [];
    const badgePairs = badgesString.split(',');
    
    badgePairs.forEach(pair => {
      const [name, version] = pair.split('/');
      badges.push({ name, version });
    });
    
    return badges;
  }

  async sendMessage(message) {
    if (!this.client || !this.isConnected) {
      throw new Error('Twitch未连接');
    }
    
    try {
      await this.client.say(this.channel, message);
    } catch (error) {
      console.error('发送Twitch消息失败:', error);
      throw error;
    }
  }
}