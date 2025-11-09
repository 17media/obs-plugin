/**
 * YouTube平台处理器
 * 处理YouTube直播聊天消息
 */

import { BasePlatform } from '../../BasePlatform';
import { getYouTubeToken } from '../api/auth';
import axios from 'axios';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { MsgType_COMMENT, MsgType_JOIN_ROOM } from '@/lib/constants';

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
    this.devMocksInjected = false;

    // 在开发环境下提前注入 mock 数据，用于样式预览（无需连接）
    if (process.env.NODE_ENV === 'development') {
      setTimeout(() => {
        try {
          this.injectDevMocks();
        } catch (e) {
          // 安静失败以免影响启动
          console.warn('YouTube mock 注入失败:', e);
        }
      }, 300);
    }
  }

  async connect(config) {
    try {
      const { apiKey, accessToken, liveChatId } = config || {};

      this.liveChatId = liveChatId;

      // 获取令牌：优先使用显式配置，其次使用环境/REST
      let token = apiKey || accessToken || null;
      if (!token) {
        try {
          token = await getYouTubeToken();
        } catch (e) {
          console.warn('YouTube token 获取失败:', e);
        }
      }

      // 根据令牌形式决定调用方式：Bearer 访问令牌 或 API Key
      if (token) {
        const lower = token.toLowerCase();
        if (lower.startsWith('bearer ')) {
          this.accessToken = token.slice(7).trim();
          this.apiKey = null;
        } else if (token.startsWith('ya29.')) {
          // 常见Google OAuth访问令牌前缀
          this.accessToken = token.trim();
          this.apiKey = null;
        } else {
          this.apiKey = token.trim();
          this.accessToken = null;
        }
      }

      if ((!this.apiKey && !this.accessToken) || !this.liveChatId) {
        throw new Error('YouTube配置错误：缺少有效Token或直播聊天ID');
      }

      // 开始轮询
      this.startPolling();
      this.isConnected = true;
      this.emit('connected', { platform: this.platformId, liveChatId });
      
      // 若连接后仍未注入 mock（例如延迟或被跳过），在开发环境下兜底一次
      if (process.env.NODE_ENV === 'development' && !this.devMocksInjected) {
        this.injectDevMocks();
      }
      
    } catch (error) {
      console.error('YouTube连接失败:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
  }

  // 开发环境：注入 mock 数据（加入/留言），与统一结构兼容
  injectDevMocks() {
    if (this.devMocksInjected || process.env.NODE_ENV !== 'development') return;

    const mockComment = {
      id: nanoid(),
      snippet: {
        type: 'textMessageEvent',
        displayMessage: '这是来自 YouTube 的测试留言 ~',
        publishedAt: new Date().toISOString(),
      },
      authorDetails: {
        displayName: 'YouTube Tester',
        channelId: 'UC_TESTER_YT',
        isChatOwner: false,
        isChatModerator: false,
      },
    };

    const mockJoinContent = fromJS({
      id: nanoid(),
      messageType: MsgType_JOIN_ROOM,
      displayName: 'YouTube Visitor',
      openID: 'UC_VISITOR_YT',
      userID: 'UC_VISITOR_YT',
      content: 'YouTube Visitor 加入了直播间',
      level: 1,
      name: { textColor: '#5e84f1' },
      comment: { textColor: '#333333' },
      backgroundColor: '',
      streamerInfo: null,
    });

    const mocks = [
      this.processRawMessage(mockComment),
      {
        id: mockJoinContent.get('id'),
        platform: this.platformId,
        timestamp: Date.now(),
        content: mockJoinContent,
      },
    ].filter(Boolean);

    mocks.forEach((mock) => this.enqueueMessage(mock));
    this.devMocksInjected = true;
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
    };

    if (this.apiKey) {
      params.key = this.apiKey;
    }

    if (this.nextPageToken) {
      params.pageToken = this.nextPageToken;
    }

    const response = await axios.get('https://www.googleapis.com/youtube/v3/liveChat/messages', {
      params,
      timeout: 10000,
      headers: this.accessToken ? { Authorization: `Bearer ${this.accessToken}` } : undefined,
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

  // 构建与 Chat 组件兼容的 Immutable 内容
  prepareIndexedChat(rawData) {
    const { snippet, authorDetails } = rawData || {};
    const id = rawData?.id || nanoid();
    const displayName = authorDetails?.displayName || 'YouTube用户';
    const isOwner = !!authorDetails?.isChatOwner;
    const isModerator = !!authorDetails?.isChatModerator;
    const nameColor = isOwner ? '#ffd700' : (isModerator ? '#5e84f1' : '#333333');

    return fromJS({
      id,
      messageType: MsgType_COMMENT,
      displayName,
      openID: authorDetails?.channelId,
      userID: authorDetails?.channelId,
      content: snippet?.displayMessage || '',
      level: 1,
      isStreamer: isOwner,
      name: { textColor: nameColor },
      comment: { textColor: '#333333' },
      backgroundColor: '',
      streamerInfo: null,
    });
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

      const timestamp = new Date(snippet.publishedAt).getTime();
      const immutableContent = this.prepareIndexedChat(rawData);

      return {
        id: immutableContent.get('id'),
        platform: this.platformId,
        timestamp,
        content: immutableContent,
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