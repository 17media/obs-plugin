/**
 * 17Live Platform Handler
 * Handles 17Live message connection and processing
 */

import { BasePlatform } from '../../BasePlatform';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import {
  MsgType_COMMENT,
  MsgType_NEW_GIFT,
  MsgType_REACT,
  MsgType_JOIN_ROOM,
  MsgType_NEW_LUCKYBAG,
  MsgType_AI_COHOST_MESSAGE,
  MsgType_POKE,
  MsgType_LABOR_RECEIVE_REWARD,
  MsgType_ENTER_ANIMATION,
} from '@/lib/constants';
import { getEnterAnimationFiles, getGiftByID, getRoomInfo } from '../api';

export class OneSevenLivePlatform extends BasePlatform {
  constructor() {
    super('17live', '17Live');
    this.ablyClient = null;
    this.channel = null;
    this.roomInfo = null;
    this.gifts = null;
    this.roomID = '';
    this.userID = '';
    this.devEnterAnimationTimer = null;
    this.devEnterAnimationIndex = 0;
  }

  async connect(config = {}) {
    try {
      let { roomID, userID } = config;

      // Allow fetching roomID/userID from URL when not provided (aligned with Ably.jsx)
      if (!roomID || !userID) {
        const urlParams = new URLSearchParams(typeof window !== 'undefined' ? window.location.search : '');
        roomID = roomID || urlParams.get('roomID') || '';
        userID = userID || urlParams.get('userID') || '';
      }

      // Save connection context
      this.roomID = roomID;
      this.userID = userID;

      // Fetch room info and gifts
      this.roomInfo = await getRoomInfo();
      try {
        this.enterAnimationFiles = await getEnterAnimationFiles();
      } catch (e) {
        console.warn('Failed to preload enter animation files:', e);
      }

      this.isConnected = true;
      this.emit('connected', { platform: this.platformId, roomID });

      if (process.env.NODE_ENV === 'development' && typeof window !== 'undefined') {
        const { loadDevEnterAnimationMessages, loadDevMockMessages } = await import('./OneSevenLivePlatform.devMocks');

        const raws = await loadDevMockMessages();
        let ts = Date.now();
        for (const raw of raws) {
          const unified = await this.processRawMessage(raw);
          if (!unified) continue;
          unified.timestamp = ts++;
          this.enqueueMessage(unified);
        }

        const enterRaws = await loadDevEnterAnimationMessages();
        if (enterRaws && enterRaws.length) {
          this.devEnterAnimationIndex = 0;
          if (this.devEnterAnimationTimer) {
            clearTimeout(this.devEnterAnimationTimer);
            this.devEnterAnimationTimer = null;
          }

          const tick = async () => {
            if (!this.isConnected) return;

            const raw = enterRaws[this.devEnterAnimationIndex];
            this.devEnterAnimationIndex = (this.devEnterAnimationIndex + 1) % enterRaws.length;

            const unified = await this.processRawMessage(raw);
            if (unified) {
              unified.timestamp = Date.now();
              this.enqueueMessage(unified);
              const dur = unified?.content?.getIn?.(['enterAnimation', 'durationMs']) || 1300;
              this.devEnterAnimationTimer = setTimeout(tick, 1000 + Number(dur || 1300) + 300);
            } else {
              this.devEnterAnimationTimer = setTimeout(tick, 500);
            }
          };

          tick();
        }
      }
      
    } catch (error) {
      console.error('17Live connection failed:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
  }

  async disconnect() {
    try {
      if (this.devEnterAnimationTimer) {
        clearTimeout(this.devEnterAnimationTimer);
        this.devEnterAnimationTimer = null;
      }
      this.isConnected = false;
      this.emit('disconnected', { platform: this.platformId });
    } catch (error) {
      console.error('17Live disconnect failed:', error);
      throw error;
    }
  }

  // WebSocket-driven updates (consistent with Twitch/YouTube)
  handleWsMessage({ type, payload }) {
    if (type === 'ably_chat_connected') {
      const status = payload?.status;
      const connected = status === 'connected';
      this.isConnected = connected;
      if (connected) {
        this.emit('connected', { platform: this.platformId, roomID: this.roomID });
      } else {
        this.emit('disconnected', { platform: this.platformId });
      }
      return;
    }
    if (type === 'ably_chat_message') {
      const decoded = payload; // already decoded server-side
      this.processRawMessage(decoded).then(unifiedMessage => {
        if (unifiedMessage) this.enqueueMessage(unifiedMessage);
      }).catch(err => {
        console.error('Error processing 17Live message:', err);
      });
      return;
    }
  }

  decodeMessageData(data) {
    // Ably.jsx uses gzip_base64 + pako to decode; platform layer pass-through decoded data
    return data;
  }

  // Build content consistent with Ably.jsx#prepareIndexedChat; returns an Immutable object
  async prepareIndexedChat(message) {
    const id = nanoid();
    const streamerInfo = this.roomInfo?.userInfo;
    const msgType = typeof message.type !== 'undefined' ? message.type : message?.msgType;

    if (msgType === MsgType_NEW_GIFT || msgType === MsgType_NEW_LUCKYBAG) {
      const { displayUser, barrage, ...restGift } = message?.giftMsg || {};
      const gift = await getGiftByID(restGift?.giftID);

      if (msgType === MsgType_NEW_LUCKYBAG && restGift?.extID) {
        const luckyBag = await getGiftByID(restGift.extID);
        const indexedGift = fromJS({
          ...restGift,
          ...(displayUser || {}),
          barrage,
          id,
          messageType: msgType,
          gift,
          luckyBag,
          streamerInfo,
        });
        return indexedGift;
      }

      const indexedGift = fromJS({
        ...restGift,
        ...(displayUser || {}),
        barrage,
        id,
        messageType: msgType,
        gift,
        streamerInfo,
      });
      return indexedGift;
    } else if (msgType === MsgType_AI_COHOST_MESSAGE) {
      const { commentTxt } = message?.aiCohostMsg || {};
      const indexedChat = fromJS({
        content: commentTxt,
        comment: {
          textColor: '#333333',
        },
        // Use i18n key so UI can resolve translation per locale
        displayName: 'AI_COHOST',
        name: {
          textColor: '#527fff',
        },
        backgroundColor: '#FFFFFFE6',
        id,
        messageType: msgType,
        streamerInfo,
      });
      return indexedChat;
    } else if (msgType === MsgType_POKE) {
      const { sender } = message?.pokeInfo || {};
      return fromJS({
        ...(sender || {}),
        isStreamer: sender?.userID && streamerInfo?.userID ? sender.userID === streamerInfo.userID : false,
        pokeInfo: message?.pokeInfo,
        id,
        messageType: msgType,
        streamerInfo,
      });
    } else if (msgType === MsgType_LABOR_RECEIVE_REWARD) {
      const rewardMsg = message?.laborReceiveRewardMsg || {};
      const userInfo = rewardMsg?.userInfo || {};
      return fromJS({
        ...userInfo,
        ...(typeof userInfo.level === 'undefined' && typeof rewardMsg.level !== 'undefined'
          ? { level: rewardMsg.level }
          : {}),
        value: rewardMsg?.value,
        id,
        messageType: msgType,
        streamerInfo,
      });
    } else if (msgType === MsgType_REACT) {
      const reactMsg = message?.reactMsg || {};
      if (reactMsg.type !== 2) return null;
      const userInfo = reactMsg?.userInfo || reactMsg?.displayUser || {};
      return fromJS({
        ...userInfo,
        ...(typeof userInfo.level === 'undefined' && typeof reactMsg.level !== 'undefined'
          ? { level: reactMsg.level }
          : {}),
        reactType: reactMsg.type,
        id,
        messageType: msgType,
        streamerInfo,
      });
    } else if (msgType === MsgType_ENTER_ANIMATION) {
      const payload = message?.subscriberEnterMsg || message?.enterAnimationMsg || {};
      const animationId = payload?.animation;

      const textKey = (() => {
        if (animationId === 1) return 'guardian_entry_animation_message';
        if (animationId === 2) return 'VIP';
        if (animationId === 6) return 'producer_enterroom';
        if (animationId >= 7 && animationId <= 10) return 'army_enter_notification';
        if (animationId === 15) return 'army_enter_notification';
        if (animationId >= 11 && animationId <= 13) return 'mlevel_entry_notice_subscription';
        return 'enter_is_here';
      })();

      const localSrc = (() => {
        const notif = payload?.eventNotifMsg;
        if (notif && notif.templateURL) return notif.templateURL;

        if (animationId === 6) return '/enter_animation/ani_17k_producer_2.webp';
        if (animationId === 2) return '/enter_animation/vip_goin_m.webp';
        if (animationId === 1) return '/enter_animation/ani_vip_army_sergeant.webp';
        if (animationId === 3 || animationId === 4 || animationId === 5) return '/enter_animation/igSettingMlevelLow@3x.png';
        if (animationId === 7 || animationId === 8 || animationId === 9 || animationId === 10 || animationId === 15) {
          return '/enter_animation/ani_vip_army_general.webp';
        }
        if (animationId >= 11 && animationId <= 13) return '/enter_animation/ani_lv_050.webp';
        return '/enter_animation/ani_lv_050.webp';
      })();

      const durationMs = (() => {
        if (typeof payload?.durationMs === 'number') return payload.durationMs;
        if (animationId === 14) return 2200;
        if (animationId === 6) return 1800;
        if (animationId === 2) return 1500;
        if (animationId === 1) return 1500;
        return 1300;
      })();

      const userInfo = {
        displayName: payload?.displayName,
        userID: payload?.userID,
        picture: payload?.picture,
        level: payload?.level,
        mLevel: payload?.mLevel,
      };

      return fromJS({
        ...userInfo,
        enterAnimation: {
          ...payload,
          animationId,
          textKey,
          localSrc,
          durationMs,
        },
        id,
        messageType: msgType,
        streamerInfo,
      });
    }

    const { displayUser, barrage, ...restChat } = message?.commentMsg || {};
    const indexedChat = fromJS({
      ...restChat,
      ...(displayUser || {}),
      barrage,
      id,
      messageType: msgType,
      streamerInfo,
    });
    return indexedChat;
  }

  async processRawMessage(rawData) {
    const type = typeof rawData?.type !== 'undefined' ? rawData.type : rawData?.msgType;
    
    switch (type) {
      case MsgType_COMMENT:
        return this.processCommentMessage(rawData);
      case MsgType_NEW_GIFT:
      case MsgType_NEW_LUCKYBAG:
        return this.processGiftMessage(rawData);
      case MsgType_JOIN_ROOM:
        return this.processJoinMessage(rawData);
      case MsgType_AI_COHOST_MESSAGE:
        return this.processAICohostMessage(rawData);
      case MsgType_POKE:
        return this.processPokeMessage(rawData);
      case MsgType_LABOR_RECEIVE_REWARD:
        return this.processLaborReceiveRewardMessage(rawData);
      case MsgType_REACT:
        return this.processReactMessage(rawData);
      case MsgType_ENTER_ANIMATION:
        return this.processEnterAnimationMessage(rawData);
      default:
        // console.warn('Unknown 17Live message type:', type);
        return null;
    }
  }

  async processCommentMessage(data) {
    const content = await this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  async processGiftMessage(data) {
    const content = await this.prepareIndexedChat(data);

    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  async processJoinMessage(data) {
    const content = await this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  async processAICohostMessage(data) {
    const content = await this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  async processPokeMessage(data) {
    const content = await this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  async processLaborReceiveRewardMessage(data) {
    const content = await this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  async processReactMessage(data) {
    const reactType = data?.reactMsg?.type;
    if (reactType !== 2) return null;

    const content = await this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  async processEnterAnimationMessage(data) {
    const content = await this.prepareIndexedChat(data);
    if (!content) return null;
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }
}
