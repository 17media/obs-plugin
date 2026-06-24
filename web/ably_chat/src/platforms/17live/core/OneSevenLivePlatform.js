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
import { getWebpDurationMs } from '@/lib/webpDuration';
import { getEnterAnimationFiles, getGiftByID, getI18nConfig, getRoomInfo } from '../api';

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

  resolveDevEnterAnimationAssetSrc(content) {
    const raw =
      content?.getIn?.(['enterAnimation', 'assetSrc']) ||
      content?.getIn?.(['enterAnimation', 'localSrc']) ||
      content?.getIn?.(['enterAnimation', 'src']) ||
      content?.getIn?.(['enterAnimation', 'asset']) ||
      content?.getIn?.(['enterAnimation', 'fileName']) ||
      content?.getIn?.(['enterAnimation', 'file']) ||
      '';

    if (raw) {
      if (raw.startsWith('http://') || raw.startsWith('https://') || raw.startsWith('/')) return raw;
      return `/enter_animation/${raw}`;
    }

    const animationId = Number(
      content?.getIn?.(['enterAnimation', 'animationId']) ||
      content?.getIn?.(['enterAnimation', 'animation']) ||
      0
    );

    if (animationId === 11) return '/enter_animation/vip_goin_s.webp';
    if (animationId === 12) return '/enter_animation/vip_goin_m.webp';
    if (animationId === 13) return '/enter_animation/vip_goin_l.webp';
    return '';
  }

  async getDevEnterAnimationIntervalMs(unified) {
    const fallbackHoldMs = Number(unified?.content?.getIn?.(['enterAnimation', 'durationMs']) || 1300);
    const assetSrc = this.resolveDevEnterAnimationAssetSrc(unified?.content);
    const actualHoldMs = (await getWebpDurationMs(assetSrc)) || fallbackHoldMs;

    // Match overlay timing: 1000ms enter + hold duration + 300ms exit, plus a small buffer.
    return 1000 + Number(actualHoldMs || fallbackHoldMs) + 300 + 200;
  }

  async connect(config = {}) {
    try {
      let { roomID, userID } = config;
      const urlParams = new URLSearchParams(typeof window !== 'undefined' ? window.location.search : '');

      // Allow fetching roomID/userID from URL when not provided (aligned with Ably.jsx)
      if (!roomID || !userID) {
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
      try {
        this.i18nConfig = await getI18nConfig();
      } catch (e) {
        console.warn('Failed to preload i18n config:', e);
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

        const enterRaws0 = await loadDevEnterAnimationMessages();
        const devMockOnlyAnimId = Number(
          config?.devMockEnterAnimationId || urlParams.get('mockAnim') || 0
        );

        const enterRaws =
          devMockOnlyAnimId > 0
            ? enterRaws0.filter((raw) => {
                const rawAnimationId = Number(
                  raw?.subscriberEnterMsg?.animation || raw?.enterAnimationMsg?.animation || 0
                );
                return rawAnimationId === devMockOnlyAnimId;
              })
            : Array.from({ length: 17 }, (_, index) => index + 1)
                .map((animationId) =>
                  enterRaws0.find((raw) => {
                    const rawAnimationId = Number(
                      raw?.subscriberEnterMsg?.animation || raw?.enterAnimationMsg?.animation || 0
                    );
                    return rawAnimationId === animationId;
                  })
                )
                .filter(Boolean);
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
              const intervalMs = await this.getDevEnterAnimationIntervalMs(unified);
              this.devEnterAnimationTimer = setTimeout(tick, intervalMs);
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
      const animationId = Number(payload?.animation || 0);
      const notif = payload?.eventNotifMsg;
      const i18nMap = this.i18nConfig && typeof this.i18nConfig === 'object' ? this.i18nConfig : null;
      const filesList = this.enterAnimationFiles && typeof this.enterAnimationFiles === 'object'
        ? this.enterAnimationFiles
        : null;

      const resolveI18nString = (key) => {
        if (!key) return '';
        if (!i18nMap) return '';
        const v = i18nMap[key];
        return typeof v === 'string' ? v : '';
      };

      const formatI18nTemplate = (tpl, params) => {
        if (typeof tpl !== 'string') return '';
        const values = Array.isArray(params) ? params.map((p) => (p && p.value ? String(p.value) : '')) : [];
        let out = tpl;
        out = out.replace(/%(\d+)\$@/g, (_, n) => {
          const idx = Number(n) - 1;
          return idx >= 0 && idx < values.length ? values[idx] : '';
        });
        if (out.includes('%@')) {
          out = out.replace(/%@/g, values[0] || '');
        }
        return out;
      };

      const resolveTokenText = (token) => {
        if (!token || typeof token !== 'object') return '';
        const key = token.key;
        const tpl = resolveI18nString(key);
        if (!tpl) return typeof key === 'string' ? key : '';
        return formatI18nTemplate(tpl, token.params);
      };

      const lookupEventAnimSrc = (animationID) => {
        if (!animationID) return '';
        const files =
          filesList && Array.isArray(filesList.files)
            ? filesList.files
            : filesList && Array.isArray(filesList.animations)
              ? filesList.animations
              : [];
        const item = files.find((f) => {
          if (!f || typeof f !== 'object') return false;
          return (
            f.animationID === animationID ||
            f.animationId === animationID ||
            f.id === animationID ||
            f.name === animationID
          );
        });
        if (!item) return '';
        return (
          item.webpURL ||
          item.webpUrl ||
          item.webp ||
          item.url ||
          item.URL ||
          ''
        );
      };

      const badgeKey = (() => {
        if (animationId === 1) return 'guardian_entry_animation_message';
        if (animationId === 2) return 'VIP';
        if (animationId === 6) return 'producer_enterroom';
        if ((animationId >= 7 && animationId <= 10) || animationId === 15) return 'army_enter_notification';
        if (animationId === 12 || animationId === 13) return 'mlevel_entry_notice_subscription';
        if (animationId === 3 || animationId === 4 || animationId === 5 || animationId === 16 || animationId === 17) return 'LV%@';
        return '';
      })();

      const marqueeKey = animationId === 6 ? '' : 'enter_is_here';

      const assetSrc = (() => {
        if (animationId === 1) {
          const fromFiles = lookupEventAnimSrc('new_guardian_enter_ios');
          if (fromFiles) return fromFiles;
        }
        if (animationId === 14 && notif) {
          const fromFiles = lookupEventAnimSrc(notif.animationID);
          if (fromFiles) return fromFiles;
          if (notif.templateURL) return notif.templateURL;
        }
        if (notif && notif.templateURL) return notif.templateURL;

        if (animationId === 3) return '/enter_animation/ani_lv_050.webp';
        if (animationId === 4) return '/enter_animation/ani_lv_100.webp';
        if (animationId === 5) return '/enter_animation/ani_lv_120.webp';
        if (animationId === 6) return '/enter_animation/ani_17k_producer_2.webp';
        if (animationId === 7) return '/enter_animation/ani_vip_army_sergeant.webp';
        if (animationId === 8) return '/enter_animation/ani_vip_army_captain.webp';
        if (animationId === 9) return '/enter_animation/ani_vip_army_colonel.webp';
        if (animationId === 10) return '/enter_animation/ani_vip_army_general.webp';
        if (animationId === 12) return '/enter_animation/vip_goin_m.webp';
        if (animationId === 13) return '/enter_animation/vip_goin_l.webp';
        if (animationId === 16) return '/enter_animation/ani_lv_160.webp';
        if (animationId === 17) return '/enter_animation/ani_lv_200.webp';
        return '';
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

      const event14 =
        animationId === 14 && notif
          ? {
              eventNameText: resolveTokenText(notif.name),
              eventDescText: resolveTokenText(notif.descriptionToken),
              eventGradientFrom: notif.gradientFrom,
              eventGradientTo: notif.gradientTo,
              eventTextSize: notif.textSize,
              eventTextColor: notif.textColor,
              eventNameColor: notif.nameColor,
              eventStrokeColor: notif.strokeColor,
              eventAnimationID: notif.animationID,
            }
          : {};

      return fromJS({
        ...userInfo,
        enterAnimation: {
          ...payload,
          animationId,
          badgeKey,
          marqueeKey,
          assetSrc,
          durationMs,
          ...event14,
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
