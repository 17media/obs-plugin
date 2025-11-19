/**
 * YouTube platform handler
 * Handles YouTube live chat messages
 */

import { BasePlatform } from '../../BasePlatform';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { MsgType_COMMENT, MsgType_JOIN_ROOM } from '@/lib/constants';
// Dev-only mock messages (aligned with 17live pattern)
// import youtubeMockComment from '@/../public/mock/youtube_chat_message.json';
// import youtubeMockJoin from '@/../public/mock/youtube_chat_join.json';

export class YouTubePlatform extends BasePlatform {
  constructor() {
    super('youtube', 'YouTube');
    this.devMocksInjected = false;
    // Always attempt to inject mocks at construction time; gating handled in injectDevMocks
    try {
      this.injectDevMocks();
    } catch (e) {
      console.warn('YouTube mock injection failed:', e);
    }
  }

  async connect(config) {
    this.isConnected = true;
    this.emit('connected', { platform: this.platformId, config: config || {} });
  }

  injectDevMocks() {
    if (this.devMocksInjected || process.env.NEXT_PUBLIC_MOCK !== '1') return;
    const mocks = [
      // this.processRawMessage(youtubeMockComment),
      // {
      //   id: youtubeMockJoin.id,
      //   platform: this.platformId,
      //   timestamp: Date.now(),
      //   content: fromJS({
      //     id: youtubeMockJoin.id,
      //     messageType: MsgType_JOIN_ROOM,
      //     displayName: youtubeMockJoin.authorDetails?.displayName || 'YouTube Visitor',
      //     openID: youtubeMockJoin.authorDetails?.channelId,
      //     userID: youtubeMockJoin.authorDetails?.channelId,
      //     content: 'YouTube Visitor joined the live room',
      //     level: 1,
      //     name: { textColor: '#FF0000' },
      //     comment: { textColor: '#FFFFFF' },
      //     backgroundColor: '',
      //     streamerInfo: null,
      //   }),
      // },
    ].filter(Boolean);

    mocks.forEach((mock) => this.enqueueMessage(mock));
    console.log('youtube', mocks);
    this.devMocksInjected = true;
  }
  async disconnect() {
    this.isConnected = false;
    this.emit('disconnected', { platform: this.platformId });
  }

  // Polling removed; messages arrive via WebSocket routing

  handleWsMessage({ type, payload }) {
    if (type === 'youtube_chat_connected') {
      const status = payload?.status;
      const connected = status === 'connected';
      this.isConnected = connected;
      if (connected) {
        this.emit('connected', { platform: this.platformId, config: {} });
      } else {
        this.emit('disconnected', { platform: this.platformId });
      }
      return;
    }
    if (type === 'youtube_chat_message') {
      const unified = this.processRawMessage(payload);
      if (unified) this.enqueueMessage(unified);
      return;
    }
  }

  // Build Immutable content compatible with Chat component
  prepareIndexedChat(rawData) {
    const { snippet, authorDetails } = rawData || {};
    const id = rawData?.id || nanoid();
    const displayName = authorDetails?.displayName || 'YouTube User';
    const isOwner = !!authorDetails?.isChatOwner;

    return fromJS({
      id,
      messageType: MsgType_COMMENT,
      displayName,
      openID: authorDetails?.channelId,
      userID: authorDetails?.channelId,
      content: snippet?.displayMessage || '',
      level: 1,
      isStreamer: isOwner,
      name: { textColor: '#FF0000' },
      comment: { textColor: '#FFFFFF' },
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
