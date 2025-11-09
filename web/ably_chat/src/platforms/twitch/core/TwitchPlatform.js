/**
 * Twitch Platform Handler
 * Uses TMI.js to process Twitch chat messages
 */

import { BasePlatform } from '../../BasePlatform';
import { getTwitchToken } from '../api/auth';
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

    // In development, pre-inject mock data for style preview (no connection needed)
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
    try {
      const { channel, username, oauth, reconnect = true } = config;
      
      if (!channel) {
        throw new Error('Twitch config error: missing channel name');
      }

      this.channel = channel.toLowerCase().replace('#', '');
      this.connectionConfig = {
        channels: [this.channel],
        reconnect: reconnect
      };

      // Prefer explicitly provided oauth; otherwise use unified token retrieval in dev/prod
      let password = oauth;
      if (!password) {
        try {
          password = await getTwitchToken();
        } catch (e) {
          console.warn('Failed to obtain Twitch token, falling back to anonymous connection:', e);
        }
      }
      if (password) {
        this.connectionConfig.identity = {
          username: username || 'justinfan12345', // Use anonymous username if not provided
          password
        };
      } else {
        // Fallback to anonymous connection
        this.connectionConfig.identity = {
          username: 'justinfan12345',
          password: 'oauth:justinfan12345'
        };
      }

      // Create TMI client
      this.client = new tmi.Client(this.connectionConfig);

      // Set up event listeners
      this.setupEventListeners();

      // Connect to Twitch
      await this.client.connect();
      
      this.isConnected = true;
      this.emit('connected', { platform: this.platformId, channel: this.channel });
      // In development, inject mocks once after connect if not yet injected
      if (process.env.NODE_ENV === 'development' && !this.devMocksInjected) {
        this.injectDevMocks();
      }
      
    } catch (error) {
      console.error('Twitch connection failed:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
  }

  // Development: inject mock data (join/comment), compatible with unified structure
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
      message: 'This is a test comment from Twitch ~',
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
      console.error('Failed to disconnect Twitch:', error);
      throw error;
    }
  }

  setupEventListeners() {
    if (!this.client) return;

    // Chat messages
    this.client.on('message', (channel, tags, message, self) => {
      this.handleChatMessage(channel, tags, message, self);
    });

    // Join channel
    this.client.on('join', (channel, username, self) => {
      if (!self) { // Not self
        this.handleJoinMessage(channel, username);
      }
    });

    // Subscription/Follow events
    this.client.on('subscription', (channel, username, method, message, userstate) => {
      this.handleSubscription(channel, username, method, message, userstate);
    });

    this.client.on('resub', (channel, username, months, message, userstate, methods) => {
      this.handleResub(channel, username, months, message, userstate, methods);
    });

    // Gifts/Donations
    this.client.on('cheer', (channel, userstate, message) => {
      this.handleCheer(channel, userstate, message);
    });

    // Connection events
    this.client.on('connected', (addr, port) => {
      console.log(`Twitch connected to: ${addr}:${port}`);
    });

    this.client.on('disconnected', (reason) => {
      console.log('Twitch disconnected:', reason);
      if (this.isConnected) {
        this.isConnected = false;
        this.emit('disconnected', { platform: this.platformId, reason });
      }
    });

    this.client.on('reconnect', () => {
      console.log('Twitch reconnecting...');
    });

    // Error handling
    this.client.on('error', (error) => {
      console.error('Twitch error:', error);
      this.emit('error', { platform: this.platformId, error });
    });
  }

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