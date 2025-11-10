/**
 * Twitch Platform Handler
 * Uses TMI.js to process Twitch chat messages
 */

import { BasePlatform } from '../../BasePlatform';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { MsgType_COMMENT, MsgType_JOIN_ROOM, MsgType_NEW_GIFT } from '@/lib/constants';

export class TwitchPlatform extends BasePlatform {
  constructor() {
    super('twitch', 'Twitch');
    this.devMocksInjected = false;

    if (process.env.NODE_ENV === 'development') {
      setTimeout(() => {
        try {
          this.injectDevMocks();
        } catch (e) {
          console.warn('Failed to inject Twitch mock:', e);
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

    const mockChat = {
      type: 'chat',
      channel: '#test',
      tags: {
        id: nanoid(),
        'display-name': 'Twitch Tester',
        username: 'twitch_tester',
        'user-id': 'TWITCH_TESTER_ID',
        color: '#9146FF',
      },
      message: 'This is a test comment from Twitch ~',
      timestamp: Date.now(),
    };

    const mockJoin = {
      type: 'join',
      channel: '#test',
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
    this.isConnected = false;
    this.emit('disconnected', { platform: this.platformId });
  }

  // No external event listeners; messages arrive via WebSocket routing

  handleChatMessage(channel, tags, message, self) {
    try {
      // Ignore self messages
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
      console.error('Failed to process Twitch chat message:', error);
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
      console.error('Failed to process Twitch join message:', error);
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
      console.error('Failed to process Twitch subscription message:', error);
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
      console.error('Failed to process Twitch resubscription message:', error);
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
      console.error('Failed to process Twitch cheer message:', error);
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
          console.warn('Unknown Twitch message type:', type);
          return null;
      }
    } catch (error) {
      console.error('Failed to process Twitch raw message:', error);
      return null;
    }
  }

  // Build Immutable content compatible with the Chat component
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
        content: `${username} joined the channel`,
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
      const giftName = type === 'subscription' ? 'Subscription' : (type === 'resub' ? `Resubscription for ${months} months` : 'Bits');
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

    // Fallback regular comment
    return fromJS({
      id,
      messageType: MsgType_COMMENT,
      displayName: base?.username || 'Twitch user',
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
      throw new Error('Twitch not connected');
    }
    
    try {
      await this.client.say(this.channel, message);
    } catch (error) {
      console.error('Failed to send Twitch message:', error);
      throw error;
    }
  }
}