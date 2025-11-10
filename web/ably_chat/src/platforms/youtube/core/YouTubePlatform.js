/**
 * YouTube platform handler
 * Handles YouTube live chat messages
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
    this.pollingInterval = 5000; // Default 5 seconds
    this.pollingTimer = null;
    this.lastPollTime = 0;
    this.retryCount = 0;
    this.maxRetries = 3;
    this.devMocksInjected = false;

    // In development, inject mock data early for style preview (no connection required)
    if (process.env.NODE_ENV === 'development') {
      setTimeout(() => {
        try {
          this.injectDevMocks();
        } catch (e) {
          // Fail silently to avoid affecting startup
          console.warn('YouTube mock injection failed:', e);
        }
      }, 300);
    }
  }

  async connect(config) {
    try {
      const { apiKey, accessToken, liveChatId } = config || {};

      this.liveChatId = liveChatId;

      // Get token: prefer explicit config, then environment/REST
      let token = apiKey || accessToken || null;
      if (!token) {
        try {
          token = await getYouTubeToken();
        } catch (e) {
          console.warn('YouTube token fetch failed:', e);
        }
      }

      // Decide call method based on token type: Bearer access token or API key
      if (token) {
        const lower = token.toLowerCase();
        if (lower.startsWith('bearer ')) {
          this.accessToken = token.slice(7).trim();
          this.apiKey = null;
        } else if (token.startsWith('ya29.')) {
          // Common Google OAuth access token prefix
          this.accessToken = token.trim();
          this.apiKey = null;
        } else {
          this.apiKey = token.trim();
          this.accessToken = null;
        }
      }

      if ((!this.apiKey && !this.accessToken) || !this.liveChatId) {
        throw new Error('YouTube configuration error: missing valid token or live chat ID');
      }

      // Start polling
      this.startPolling();
      this.isConnected = true;
      this.emit('connected', { platform: this.platformId, liveChatId });
      
      // If mocks not injected after connect (delay or skipped), inject once in dev
      if (process.env.NODE_ENV === 'development' && !this.devMocksInjected) {
        this.injectDevMocks();
      }
      
    } catch (error) {
      console.error('YouTube connection failed:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
  }

  // Development: inject mock data (join/comment), compatible with unified structure
  injectDevMocks() {
    if (this.devMocksInjected || process.env.NODE_ENV !== 'development') return;

    const mockComment = {
      id: nanoid(),
      snippet: {
        type: 'textMessageEvent',
        displayMessage: 'This is a test comment from YouTube ~',
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
      content: 'YouTube Visitor joined the live room',
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
      console.error('YouTube disconnect failed:', error);
      throw error;
    }
  }

  startPolling() {
    if (!this.isConnected) return;

    const poll = async () => {
      try {
        await this.pollMessages();
        this.retryCount = 0; // Reset retry count
      } catch (error) {
        console.error('YouTube polling failed:', error);
        this.handlePollingError(error);
      }
    };

    // Execute once immediately
    poll();
    
    // Set timer
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
      return; // Avoid overly frequent requests
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
    
    // Update page token and polling interval
    this.nextPageToken = data.nextPageToken;
    if (data.pollingIntervalMillis) {
      this.pollingInterval = Math.max(1000, data.pollingIntervalMillis); // At least 1 second
    }

    // Process messages
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
        // API key invalid or insufficient permissions
        console.error('YouTube API permission error:', data.error?.message);
        this.emit('error', { 
          platform: this.platformId, 
          error: new Error('YouTube API permissions insufficient, please check API key') 
        });
        return;
      }
      
      if (status === 429) {
        // Rate limit, exponential backoff
        this.retryCount++;
        const backoffTime = Math.min(60000, 1000 * Math.pow(2, this.retryCount));
        console.warn(`YouTube API rate limited, retry after ${backoffTime}ms`);
        
        setTimeout(() => {
          this.startPolling();
        }, backoffTime);
        return;
      }
    }

    // Other errors, simple retry
    this.retryCount++;
    if (this.retryCount < this.maxRetries) {
      setTimeout(() => {
        this.startPolling();
      }, 5000);
    } else {
      console.error('Too many YouTube polling failures, stop retrying');
      this.emit('error', { 
        platform: this.platformId, 
        error: new Error('YouTube connection failed, please check configuration') 
      });
    }
  }

  // Build Immutable content compatible with Chat component
  prepareIndexedChat(rawData) {
    const { snippet, authorDetails } = rawData || {};
    const id = rawData?.id || nanoid();
    const displayName = authorDetails?.displayName || 'YouTube User';
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
  
      // Only process chat messages
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
      console.error('Failed to process YouTube message:', error);
      return null;
    }
  }

  async sendMessage(message) {
    // YouTube requires OAuth to send messages; not supported here
    throw new Error('YouTube platform sending messages requires OAuth; not supported');
  }
}
