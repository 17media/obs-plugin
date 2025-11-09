/**
 * Twitch平台处理器
 * 使用TMI.js处理Twitch聊天消息
 */

import { BasePlatform } from '../BasePlatform';
import tmi from 'tmi.js';
import { nanoid } from 'nanoid';

export class TwitchPlatform extends BasePlatform {
  constructor() {
    super('twitch', 'Twitch');
    this.client = null;
    this.channel = null;
    this.connectionConfig = null;
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
      
    } catch (error) {
      console.error('Twitch连接失败:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
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

  processChatMessage(rawData) {
    const { tags, message } = rawData;
    
    return {
      id: tags.id || nanoid(),
      platform: this.platformId,
      type: 'comment',
      content: message,
      author: {
        id: tags['user-id'],
        name: tags.username,
        displayName: tags['display-name'] || tags.username,
        avatar: null, // Twitch IRC不提供头像
        color: tags.color,
        isMod: tags.mod,
        isSubscriber: tags.subscriber,
        isTurbo: tags.turbo,
        userType: tags['user-type'],
        badges: this.parseBadges(tags.badges)
      },
      timestamp: Date.now(),
      rawData: rawData,
      metadata: {
        emotes: tags.emotes,
        messageType: tags['message-type'],
        channel: rawData.channel
      }
    };
  }

  processJoinMessage(rawData) {
    const { username } = rawData;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'join',
      content: `${username} 加入了频道`,
      author: {
        id: username,
        name: username,
        displayName: username
      },
      timestamp: Date.now(),
      rawData: rawData
    };
  }

  processSubscriptionMessage(rawData) {
    const { username, message, userstate } = rawData;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'subscription',
      content: `${username} 订阅了频道${message ? ': ' + message : ''}`,
      author: {
        id: userstate['user-id'] || username,
        name: username,
        displayName: username,
        isSubscriber: true
      },
      timestamp: Date.now(),
      rawData: rawData,
      metadata: {
        subscriptionType: userstate['msg-param-sub-plan'],
        message
      }
    };
  }

  processResubMessage(rawData) {
    const { username, months, message, userstate } = rawData;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'resub',
      content: `${username} 订阅了 ${months} 个月${message ? ': ' + message : ''}`,
      author: {
        id: userstate['user-id'] || username,
        name: username,
        displayName: username,
        isSubscriber: true
      },
      timestamp: Date.now(),
      rawData: rawData,
      metadata: {
        months,
        subscriptionType: userstate['msg-param-sub-plan'],
        message
      }
    };
  }

  processCheerMessage(rawData) {
    const { userstate, message } = rawData;
    const bits = parseInt(userstate.bits) || 0;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'cheer',
      content: `${userstate['display-name']} 欢呼了 ${bits} bits${message ? ': ' + message : ''}`,
      author: {
        id: userstate['user-id'],
        name: userstate.username,
        displayName: userstate['display-name'] || userstate.username
      },
      timestamp: Date.now(),
      rawData: rawData,
      metadata: {
        bits,
        message
      }
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