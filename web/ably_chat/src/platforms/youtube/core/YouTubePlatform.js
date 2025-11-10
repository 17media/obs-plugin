/**
 * YouTube platform handler
 * Handles YouTube live chat messages
 */

import { BasePlatform } from '../../BasePlatform';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { MsgType_COMMENT, MsgType_JOIN_ROOM } from '@/lib/constants';

export class YouTubePlatform extends BasePlatform {
  constructor() {
    super('youtube', 'YouTube');
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
    this.isConnected = true;
    this.emit('connected', { platform: this.platformId, config: config || {} });
    if (process.env.NODE_ENV === 'development' && !this.devMocksInjected) {
      this.injectDevMocks();
    }
  }

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
    this.isConnected = false;
    this.emit('disconnected', { platform: this.platformId });
  }

  // Polling removed; messages arrive via WebSocket routing

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
