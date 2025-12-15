/**
 * Twitch Platform Handler
 * Uses TMI.js to process Twitch chat messages
 */

import { BasePlatform } from '../../BasePlatform';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { MsgType_COMMENT, MsgType_JOIN_ROOM, MsgType_NEW_GIFT } from '@/lib/constants';
// Dev-only mock messages (aligned with 17live pattern)
// import twitchMockChat from '@/../public/mock/twitch_chat_message.json';
// import twitchMockJoin from '@/../public/mock/twitch_chat_join.json';
// import twitchMockSub from '@/../public/mock/twitch_chat_subscription.json';

export class TwitchPlatform extends BasePlatform {
  constructor() {
    super('twitch', 'Twitch');
    this.devMocksInjected = false;
    // Always attempt to inject mocks at construction time; gating handled in injectDevMocks
    try {
      this.injectDevMocks();
    } catch (e) {
      console.warn('Failed to inject Twitch mock:', e);
    }
  }

  async connect(config) {
    this.isConnected = true;
    this.emit('connected', { platform: this.platformId, config: config || {} });
  }

  injectDevMocks() {
    if (this.devMocksInjected || process.env.NEXT_PUBLIC_MOCK !== '1') return;
    // Use imported mock data to ensure bundler resolves JSON correctly
    const mocks = [
      // this.processRawMessage(twitchMockChat),
      // this.processRawMessage(twitchMockJoin),
      // this.processRawMessage(twitchMockSub),
    ].filter(Boolean);

    mocks.forEach((mock) => this.enqueueMessage(mock));
    this.devMocksInjected = true;
  }
  async disconnect() {
    this.isConnected = false;
    this.emit('disconnected', { platform: this.platformId });
  }

  // No external event listeners; messages arrive via WebSocket routing

  handleWsMessage({ type, payload }) {
    if (type === 'twitch_chat_connected') {
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
    if (type === 'twitch_chat_message') {
      const raw = payload?.raw || '';
      const parsed = this.parseWsRaw(raw);
      if (parsed) {
        const unified = this.processRawMessage({ type: 'chat', ...parsed, timestamp: Date.now() });
        if (unified) this.enqueueMessage(unified);
      }
      return;
    }
  }

  parseWsRaw(raw) {
    if (!raw || typeof raw !== 'string') return null;
    const msgMatch = raw.match(/PRIVMSG\s+#([^\s]+)\s+:(.*)$/);
    if (!msgMatch) return null;
    const channel = msgMatch[1];
    const message = msgMatch[2];
    let username = '';
    const userMatch = raw.match(/:([^!\s]+)!/);
    if (userMatch) username = userMatch[1];
    const tagPartEnd = raw.indexOf(' :');
    const tagStr = tagPartEnd > 0 ? raw.substring(0, tagPartEnd) : '';
    const tags = {};
    if (tagStr.includes('=')) {
      tagStr.split(';').forEach(kv => {
        const i = kv.indexOf('=');
        if (i > 0) {
          const k = kv.substring(0, i);
          const v = kv.substring(i + 1);
          tags[k] = v;
        }
      });
    }
    if (!tags['display-name'] && username) tags['display-name'] = username;
    return { channel, tags, message, username };
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
        comment: { textColor: '#FFFFFF' },
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
        comment: { textColor: '#FFFFFF' },
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
        comment: { textColor: '#FFFFFF' },
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
      comment: { textColor: '#FFFFFF' },
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