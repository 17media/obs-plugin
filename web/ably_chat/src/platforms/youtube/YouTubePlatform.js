/**
 * YouTube平台处理器
 * 处理YouTube直播聊天消息
 */

import { BasePlatform } from '../BasePlatform';
import axios from 'axios';
import { nanoid } from 'nanoid';

export class YouTubePlatform extends BasePlatform {
  constructor() {
    super('youtube', 'YouTube');
    this.apiKey = null;
    this.liveChatId = null;
    this.nextPageToken = null;
    this.pollingInterval = 5000; // 默认5秒
    this.pollingTimer = null;
    this.lastPollTime = 0;
    this.retryCount = 0;
    this.maxRetries = 3;
  }

  async connect(config) {
    try {
      const { apiKey, liveChatId } = config;
      
      this.apiKey = apiKey;
      this.liveChatId = liveChatId;
      
      if (!apiKey || !liveChatId) {
        throw new Error('YouTube配置错误：缺少API密钥或直播聊天ID');
      }

      // 开始轮询
      this.startPolling();
      this.isConnected = true;
      this.emit('connected', { platform: this.platformId, liveChatId });
      
    } catch (error) {
      console.error('YouTube连接失败:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
  }

  async disconnect() {
    try {
      if (this.pollingTimer) {
        clearTimeout(this.pollingTimer);
        this.pollingTimer = null;
      }
      this.isConnected = false;
      this.emit('disconnected', { platform: this.platformId });
    } catch (error) {
      console.error('YouTube断开连接失败:', error);
      throw error;
    }
  }

  startPolling() {
    if (!this.isConnected) return;

    const poll = async () => {
      try {
        await this.pollMessages();
        this.retryCount = 0; // 重置重试计数
      } catch (error) {
        console.error('YouTube轮询失败:', error);
        this.handlePollingError(error);
      }
    };

    // 立即执行一次
    poll();
    
    // 设置定时器
    this.scheduleNextPoll();
  }

  scheduleNextPoll() {
    if (!this.isConnected) return;

    this.pollingTimer = setTimeout(() => {
      this.startPolling();
    }, this.pollingInterval);
  }

  async pollMessages() {
    const now = Date.now();
    if (now - this.lastPollTime < this.pollingInterval) {
      return; // 避免过于频繁的请求
    }
    
    this.lastPollTime = now;

    const params = {
      part: 'snippet,authorDetails',
      liveChatId: this.liveChatId,
      key: this.apiKey
    };

    if (this.nextPageToken) {
      params.pageToken = this.nextPageToken;
    }

    const response = await axios.get('https://www.googleapis.com/youtube/v3/liveChat/messages', {
      params,
      timeout: 10000
    });

    const { data } = response;
    
    // 更新分页token和轮询间隔
    this.nextPageToken = data.nextPageToken;
    if (data.pollingIntervalMillis) {
      this.pollingInterval = Math.max(1000, data.pollingIntervalMillis); // 最少1秒
    }

    // 处理消息
    if (data.items && data.items.length > 0) {
      data.items.forEach(item => {
        const message = this.processRawMessage(item);
        if (message) {
          this.enqueueMessage(message);
        }
      });
    }
  }

  handlePollingError(error) {
    if (error.response) {
      const { status, data } = error.response;
      
      if (status === 403) {
        // API密钥无效或权限不足
        console.error('YouTube API权限错误:', data.error?.message);
        this.emit('error', { 
          platform: this.platformId, 
          error: new Error('YouTube API权限不足，请检查API密钥') 
        });
        return;
      }
      
      if (status === 429) {
        // 速率限制，指数退避
        this.retryCount++;
        const backoffTime = Math.min(60000, 1000 * Math.pow(2, this.retryCount));
        console.warn(`YouTube API速率限制，${backoffTime}ms后重试`);
        
        setTimeout(() => {
          this.startPolling();
        }, backoffTime);
        return;
      }
    }

    // 其他错误，简单重试
    this.retryCount++;
    if (this.retryCount < this.maxRetries) {
      setTimeout(() => {
        this.startPolling();
      }, 5000);
    } else {
      console.error('YouTube轮询失败次数过多，停止重试');
      this.emit('error', { 
        platform: this.platformId, 
        error: new Error('YouTube连接失败，请检查配置') 
      });
    }
  }

  processRawMessage(rawData) {
    try {
      const { snippet, authorDetails } = rawData;
      
      if (!snippet || !authorDetails) {
        return null;
      }

      // 只处理聊天消息
      if (snippet.type !== 'textMessageEvent') {
        return null;
      }

      const messageId = rawData.id || nanoid();
      const timestamp = new Date(snippet.publishedAt).getTime();
      const content = snippet.displayMessage || '';
      
      return {
        id: messageId,
        platform: this.platformId,
        type: 'comment',
        content: content,
        author: {
          id: authorDetails.channelId,
          name: authorDetails.displayName,
          displayName: authorDetails.displayName,
          avatar: authorDetails.profileImageUrl,
          isVerified: authorDetails.isVerified,
          isChatOwner: authorDetails.isChatOwner,
          isChatModerator: authorDetails.isChatModerator,
          isChatSponsor: authorDetails.isChatSponsor
        },
        timestamp: timestamp,
        rawData: rawData,
        metadata: {
          messageId: rawData.id,
          liveChatId: snippet.liveChatId,
          type: snippet.type
        }
      };
    } catch (error) {
      console.error('处理YouTube消息失败:', error);
      return null;
    }
  }

  async sendMessage(message) {
    // YouTube需要OAuth认证才能发送消息，这里暂时不支持
    throw new Error('YouTube平台发送消息需要OAuth认证，暂不支持');
  }
}