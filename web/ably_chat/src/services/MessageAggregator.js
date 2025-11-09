import { EventEmitter } from 'events';
import { OneSevenLivePlatform } from '../platforms/17live/OneSevenLivePlatform';
import { YouTubePlatform } from '../platforms/youtube/YouTubePlatform';
import { TwitchPlatform } from '../platforms/twitch/TwitchPlatform';
import { wsManager } from './WebSocketManager';
import { sendWSMessage } from './WSSender';

/**
 * 统一消息聚合管理器
 * 管理多个平台的消息流，提供统一的消息处理和分发机制
 */
export class MessageAggregator extends EventEmitter {
  constructor() {
    super();
    
    // 平台实例映射
    this.platforms = new Map();
    
    // 活动平台配置
    this.activePlatforms = new Set();
    
    // 消息队列（按时间戳排序）
    this.messageQueue = [];
    
    // 消息去重映射（基于消息ID）
    this.messageIds = new Set();
    
    // 最大消息队列长度
    this.maxQueueSize = 1000;
    
    // 消息处理间隔（毫秒）
    this.processInterval = 100;
    
    // 定时器引用
    this.processTimer = null;
    
    // 平台处理器映射
    this.platformHandlers = {
      '17live': OneSevenLivePlatform,
      'youtube': YouTubePlatform,
      'twitch': TwitchPlatform
    };
    
    this.initialize();
  }

  initialize() {
    // 启动消息处理定时器
    this.startMessageProcessor();
    
    // 监听平台事件
    this.setupEventListeners();

    // 统一初始化 WebSocket 连接
    wsManager.connect().catch(err => {
      console.error('初始化 WebSocket 失败:', err);
    });
    wsManager.on('open', ({ url }) => console.log('WS 已连接:', url));
    wsManager.on('close', () => console.log('WS 已关闭'));
    wsManager.on('error', (e) => console.error('WS 错误:', e));
  }

  setupEventListeners() {
    // 监听平台连接状态变化
    this.on('platform_connected', this.handlePlatformConnected.bind(this));
    this.on('platform_disconnected', this.handlePlatformDisconnected.bind(this));
    this.on('platform_error', this.handlePlatformError.bind(this));
  }

  /**
   * 添加平台
   */
  async addPlatform(platformId, config) {
    try {
      if (this.platforms.has(platformId)) {
        throw new Error(`平台 ${platformId} 已存在`);
      }

      const PlatformClass = this.platformHandlers[platformId];
      if (!PlatformClass) {
        throw new Error(`不支持的平��: ${platformId}`);
      }

      // 创建平台实例
      const platform = new PlatformClass();
      
      // 监听平台消息事件
      platform.on('message', (message) => {
        this.handlePlatformMessage(platformId, message);
      });

      platform.on('connected', (data) => {
        this.emit('platform_connected', { platformId, data });
      });

      platform.on('disconnected', (data) => {
        this.emit('platform_disconnected', { platformId, data });
      });

      platform.on('error', (error) => {
        this.emit('platform_error', { platformId, error });
      });

      // 添加到平台映射
      this.platforms.set(platformId, platform);

      // 如果提供了配置，自动连接
      if (config && Object.keys(config).length > 0) {
        await this.connectPlatform(platformId, config);
      }

      console.log(`平台 ${platformId} 已添加`);
      
    } catch (error) {
      console.error(`添加平台 ${platformId} 失败:`, error);
      throw error;
    }
  }

  /**
   * 移除平台
   */
  async removePlatform(platformId) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`平台 ${platformId} 不存在`);
      }

      // 断开连接
      if (platform.isConnected) {
        await platform.disconnect();
      }

      // 移除事件监听器
      platform.removeAllListeners();

      // 从映射中删除
      this.platforms.delete(platformId);
      this.activePlatforms.delete(platformId);

      console.log(`平台 ${platformId} 已移除`);
      
    } catch (error) {
      console.error(`移除平台 ${platformId} 失败:`, error);
      throw error;
    }
  }

  /**
   * 连接平台
   */
  async connectPlatform(platformId, config) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`平台 ${platformId} 不存在`);
      }

      if (platform.isConnected) {
        console.log(`平台 ${platformId} 已连接`);
        return;
      }

      await platform.connect(config);
      this.activePlatforms.add(platformId);
      
      console.log(`平台 ${platformId} 已连接`);
      
    } catch (error) {
      console.error(`连接平台 ${platformId} 失败:`, error);
      throw error;
    }
  }

  /**
   * 断开平台连接
   */
  async disconnectPlatform(platformId) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`平台 ${platformId} 不存在`);
      }

      if (!platform.isConnected) {
        console.log(`平台 ${platformId} 已断开连接`);
        return;
      }

      await platform.disconnect();
      this.activePlatforms.delete(platformId);
      
      console.log(`平台 ${platformId} 已断开连接`);
      
    } catch (error) {
      console.error(`断开平台 ${platformId} 失败:`, error);
      throw error;
    }
  }

  /**
   * 处理平台消息
   */
  handlePlatformMessage(platformId, message) {
    try {
      // 消息去重
      if (this.messageIds.has(message.id)) {
        return;
      }

      // 添加平台标识
      const enrichedMessage = {
        ...message,
        platform: platformId,
        aggregatedAt: Date.now()
      };

      // 添加到消息队列
      this.messageQueue.push(enrichedMessage);
      this.messageIds.add(message.id);

      // 保持队列长度
      if (this.messageQueue.length > this.maxQueueSize) {
        const removedMessage = this.messageQueue.shift();
        this.messageIds.delete(removedMessage.id);
      }

      // 按时间戳排序
      this.messageQueue.sort((a, b) => a.timestamp - b.timestamp);

      // 立即触发消息事件
      this.emit('message', enrichedMessage);

      // 中央转发到 WebSocket（按需）
      try {
        const content = enrichedMessage.content;
        const type = content && typeof content.get === 'function' ? content.get('messageType') : undefined;
        const gift = content && typeof content.get === 'function' ? content.get('gift') : undefined;

        if (gift) {
          const playData = {
            type: 'play_vff',
            vffURL: gift.get('vffURL'),
            vffJson: gift.get('vffJson'),
          };
          sendWSMessage({
            type,
            platform: platformId,
            payload: playData,
          });
        }
      } catch (e) {
        console.error('WS 转发失败:', e);
      }
      
    } catch (error) {
      console.error('处理平台消息失败:', error);
      this.emit('error', { type: 'message_processing', error, platformId, message });
    }
  }

  /**
   * 处理平台连接事件
   */
  handlePlatformConnected({ platformId, data }) {
    console.log(`平台 ${platformId} 连接成功:`, data);
    this.emit('status_change', { 
      platformId, 
      status: 'connected',
      timestamp: Date.now()
    });
  }

  /**
   * 处理平台断开连接事件
   */
  handlePlatformDisconnected({ platformId, data }) {
    console.log(`平台 ${platformId} 断开连接:`, data);
    this.activePlatforms.delete(platformId);
    this.emit('status_change', { 
      platformId, 
      status: 'disconnected',
      timestamp: Date.now()
    });
  }

  /**
   * 处理平台错误事件
   */
  handlePlatformError({ platformId, error }) {
    console.error(`平台 ${platformId} 错误:`, error);
    this.emit('error', { 
      type: 'platform_error',
      platformId, 
      error,
      timestamp: Date.now()
    });
  }

  /**
   * 启动消息处理器
   */
  startMessageProcessor() {
    if (this.processTimer) {
      clearInterval(this.processTimer);
    }

    this.processTimer = setInterval(() => {
      this.processMessageQueue();
    }, this.processInterval);
  }

  /**
   * 处理消息队列
   */
  processMessageQueue() {
    if (this.messageQueue.length === 0) {
      return;
    }

    const now = Date.now();
    const messagesToProcess = [];

    // 获取应该处理的消息（基于时间戳）
    while (this.messageQueue.length > 0) {
      const message = this.messageQueue[0];
      
      // 如果消息时间戳小于等于当前时间，处理它
      if (message.timestamp <= now) {
        messagesToProcess.push(this.messageQueue.shift());
      } else {
        break;
      }
    }

    // 批量发送消息
    if (messagesToProcess.length > 0) {
      this.emit('messages_batch', messagesToProcess);
    }
  }

  /**
   * 获取所有平台状态
   */
  getPlatformsStatus() {
    const status = {};
    
    this.platforms.forEach((platform, platformId) => {
      status[platformId] = {
        platformId,
        platformName: platform.platformName,
        isConnected: platform.isConnected,
        isActive: this.activePlatforms.has(platformId),
        queueSize: platform.messageQueue ? platform.messageQueue.length : 0
      };
    });

    return status;
  }

  /**
   * 获取消息统计
   */
  getMessageStats() {
    const stats = {
      total: this.messageQueue.length,
      byPlatform: {},
      byType: {},
      timestamp: Date.now()
    };

    // 按平台统计
    this.messageQueue.forEach(message => {
      const platform = message.platform;
      const type = message.type;

      if (!stats.byPlatform[platform]) {
        stats.byPlatform[platform] = 0;
      }
      stats.byPlatform[platform]++;

      if (!stats.byType[type]) {
        stats.byType[type] = 0;
      }
      stats.byType[type]++;
    });

    return stats;
  }

  /**
   * 清空消息队列
   */
  clearMessageQueue() {
    this.messageQueue = [];
    this.messageIds.clear();
    this.emit('queue_cleared', { timestamp: Date.now() });
  }

  /**
   * 发送消息到指定平台
   */
  async sendMessage(platformId, message) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`平台 ${platformId} 不存在`);
      }

      if (!platform.isConnected) {
        throw new Error(`平台 ${platformId} 未连接`);
      }

      await platform.sendMessage(message);
      
    } catch (error) {
      console.error(`发送消息到平台 ${platformId} 失败:`, error);
      throw error;
    }
  }

  /**
   * 获取所有活动平台的消息
   */
  getActiveMessages(limit = 100) {
    const activeMessages = this.messageQueue.filter(message => 
      this.activePlatforms.has(message.platform)
    );

    return activeMessages.slice(-limit);
  }

  /**
   * 停止聚合器
   */
  async stop() {
    try {
      // 停止消息处理器
      if (this.processTimer) {
        clearInterval(this.processTimer);
        this.processTimer = null;
      }

      // 断开所有平台连接
      const disconnectPromises = Array.from(this.platforms.keys()).map(platformId => 
        this.disconnectPlatform(platformId).catch(error => 
          console.error(`断开平台 ${platformId} 失败:`, error)
        )
      );

      await Promise.all(disconnectPromises);

      // 清空数据
      this.clearMessageQueue();
      this.platforms.clear();
      this.activePlatforms.clear();

      // 移除所有事件监听器
      this.removeAllListeners();

      console.log('消息聚合器已停止');
      
    } catch (error) {
      console.error('停止消息聚合器失败:', error);
      throw error;
    }
  }
}

// 创建单例实例
export const messageAggregator = new MessageAggregator();

export default MessageAggregator;