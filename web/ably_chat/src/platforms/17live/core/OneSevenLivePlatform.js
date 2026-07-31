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

function shouldLogAnim14Debug() {
  if (typeof window === 'undefined') return process.env.NODE_ENV === 'development';
  try {
    const params = new URLSearchParams(window.location.search);
    const v = params.get('logAnim14') || params.get('debugAnim14') || '';
    return (
      process.env.NODE_ENV === 'development' ||
      v === '1' ||
      v.toLowerCase() === 'true' ||
      v.toLowerCase() === 'yes'
    );
  } catch {
    return process.env.NODE_ENV === 'development';
  }
}

const ENTER_ANIMATION_ASSET_MAP = Object.freeze({
  3: '/enter_animation/ani_lv_050.webp',
  4: '/enter_animation/ani_lv_100.webp',
  5: '/enter_animation/ani_lv_120.webp',
  6: '/enter_animation/ani_17k_producer_2.webp',
  7: '/enter_animation/ani_vip_army_sergeant.webp',
  8: '/enter_animation/ani_vip_army_captain.webp',
  9: '/enter_animation/ani_vip_army_colonel.webp',
  10: '/enter_animation/ani_vip_army_general.webp',
  12: '/enter_animation/vip_goin_m.webp',
  13: '/enter_animation/vip_goin_l.webp',
  16: '/enter_animation/ani_lv_160.webp',
  17: '/enter_animation/ani_lv_200.webp',
});

const ENTER_ANIMATION_DURATION_MAP = Object.freeze({
  1: 1500,
  2: 1500,
  6: 1800,
  14: 2200,
});

const ENTER_ANIMATION_BADGE_KEY_MAP = Object.freeze({
  1: 'guardian_entry_animation_message',
  2: 'VIP',
  6: 'producer_enterroom',
  12: 'mlevel_entry_notice_subscription',
  13: 'mlevel_entry_notice_subscription',
  15: 'army_enter_notification',
});

function getEnterAnimationBadgeKey(animationId) {
  if (Object.prototype.hasOwnProperty.call(ENTER_ANIMATION_BADGE_KEY_MAP, animationId)) {
    return ENTER_ANIMATION_BADGE_KEY_MAP[animationId];
  }

  if (animationId >= 7 && animationId <= 10) return 'army_enter_notification';
  if (animationId === 3 || animationId === 4 || animationId === 5 || animationId === 16 || animationId === 17) {
    return 'LV%@';
  }

  return '';
}

function getEnterAnimationDurationMs(animationId, payloadDurationMs) {
  if (typeof payloadDurationMs === 'number') return payloadDurationMs;
  return ENTER_ANIMATION_DURATION_MAP[animationId] || 1300;
}

function getDefaultEnterAnimationAsset(animationId) {
  return ENTER_ANIMATION_ASSET_MAP[animationId] || '';
}

function resolveEnterAnimationAssetSrc(animationId, notif, lookupEventAnimSrc) {
  if (animationId === 1) {
    const guardianAssetSrc = lookupEventAnimSrc('new_guardian_enter_ios');
    if (guardianAssetSrc) return guardianAssetSrc;
  }

  if (animationId === 14 && notif?.animationID) {
    const eventAssetSrc = lookupEventAnimSrc(notif.animationID);
    if (eventAssetSrc) return eventAssetSrc;
  }

  return notif?.templateURL || getDefaultEnterAnimationAsset(animationId);
}

export class OneSevenLivePlatform extends BasePlatform {
  constructor() {
    super('17live', '17Live');
    this.ablyClient = null;
    this.channel = null;
    this.roomInfo = null;
    this.gifts = null;
    this.enterAnimationFiles = null;
    this.enterAnimationFilesPromise = null;
    this.enterAnimationFilesLoadState = 'idle';
    this.enterAnimationFilesLoadSource = 'not_started';
    this.enterAnimationFilesLoadRequestedAt = 0;
    this.enterAnimationFilesLoadResolvedAt = 0;
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

  async ensureEnterAnimationFilesLoaded(source = 'unknown') {
    const current = this.enterAnimationFiles;
    const files = Array.isArray(current?.files)
      ? current.files
      : Array.isArray(current?.animations)
        ? current.animations
        : [];

    if (files.length > 0) {
      if (this.enterAnimationFilesLoadState !== 'loaded') {
        this.enterAnimationFilesLoadState = 'loaded';
        this.enterAnimationFilesLoadSource = source;
        this.enterAnimationFilesLoadResolvedAt = this.enterAnimationFilesLoadResolvedAt || Date.now();
      }
      return current;
    }

    if (this.enterAnimationFilesPromise) {
      return this.enterAnimationFilesPromise;
    }

    this.enterAnimationFilesLoadState = 'loading';
    this.enterAnimationFilesLoadSource = source;
    this.enterAnimationFilesLoadRequestedAt = Date.now();
    this.enterAnimationFilesLoadResolvedAt = 0;

    // if (shouldLogAnim14Debug()) {
    //   console.log('[enter_animation][files] preload start', {
    //     source: this.enterAnimationFilesLoadSource,
    //     roomID: this.roomID || '',
    //     userID: this.userID || '',
    //     requestedAt: this.enterAnimationFilesLoadRequestedAt,
    //   });
    // }

    this.enterAnimationFilesPromise = (async () => {
      try {
        const result = await getEnterAnimationFiles();
        this.enterAnimationFiles = result;
        this.enterAnimationFilesLoadState = 'loaded';
        this.enterAnimationFilesLoadResolvedAt = Date.now();

        // if (shouldLogAnim14Debug()) {
        //   const loadedFiles = Array.isArray(result?.files)
        //     ? result.files
        //     : Array.isArray(result?.animations)
        //       ? result.animations
        //       : [];
        //   console.log('[enter_animation][files] preloaded', {
        //     source: this.enterAnimationFilesLoadSource,
        //     requestedAt: this.enterAnimationFilesLoadRequestedAt,
        //     resolvedAt: this.enterAnimationFilesLoadResolvedAt,
        //     fileCount: loadedFiles.length,
        //     sampleIds: loadedFiles
        //       .slice(0, 5)
        //       .map((f) => f?.animationID || f?.animationId || f?.id || f?.name || null),
        //   });
        // }

        return result;
      } catch (e) {
        this.enterAnimationFilesLoadState = 'failed';
        this.enterAnimationFilesLoadResolvedAt = Date.now();
        throw e;
      } finally {
        this.enterAnimationFilesPromise = null;
      }
    })();

    return this.enterAnimationFilesPromise;
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
        await this.ensureEnterAnimationFilesLoaded('platform.connect');
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
      // if (connected && shouldLogAnim14Debug()) {
      //   console.log('[enter_animation][files] ws connected state', {
      //     loadState: this.enterAnimationFilesLoadState,
      //     loadSource: this.enterAnimationFilesLoadSource,
      //     requestedAt: this.enterAnimationFilesLoadRequestedAt || null,
      //     resolvedAt: this.enterAnimationFilesLoadResolvedAt || null,
      //     roomID: this.roomID || '',
      //     userID: this.userID || '',
      //   });
      // }
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

      if (animationId === 14) {
        try {
          await this.ensureEnterAnimationFilesLoaded('anim14.prepareIndexedChat');
        } catch (e) {
        // console.warn('[enter_animation][files] ensure failed before anim14 lookup', e);
        }
      }

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

      const resolveTokenText = (token, options = {}) => {
        if (!token || typeof token !== 'object') return '';
        const key = token.key;
        const tpl = resolveI18nString(key);
        if (!tpl) {
          if (typeof options.fallbackTemplate === 'string' && options.fallbackTemplate) {
            return formatI18nTemplate(options.fallbackTemplate, token.params);
          }
          return typeof key === 'string' ? key : '';
        }
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

      const badgeKey = getEnterAnimationBadgeKey(animationId);

      const marqueeKey = animationId === 6 ? '' : 'enter_is_here';

      const assetSrc = resolveEnterAnimationAssetSrc(animationId, notif, lookupEventAnimSrc);

      const durationMs = getEnterAnimationDurationMs(animationId, payload?.durationMs);

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
              eventDescText: (() => {
                const resolved = resolveTokenText(notif.descriptionToken);
                return resolved === notif?.descriptionToken?.key ? '' : resolved;
              })(),
              eventGradientFrom: notif.gradientFrom,
              eventGradientTo: notif.gradientTo,
              eventTextSize: notif.textSize,
              eventTextColor: notif.textColor,
              eventNameColor: notif.nameColor,
              eventStrokeColor: notif.strokeColor,
              eventAnimationID: notif.animationID,
            }
          : {};

      if (animationId === 14 && shouldLogAnim14Debug()) {
        const eventAnimationID = notif?.animationID ?? null;
        const resolvedAssetSrc = assetSrc || null;
        const files =
          filesList && Array.isArray(filesList.files)
            ? filesList.files
            : filesList && Array.isArray(filesList.animations)
              ? filesList.animations
              : [];
        const matchedFile = eventAnimationID
          ? files.find((f) => {
              if (!f || typeof f !== 'object') return false;
              return (
                f.animationID === eventAnimationID ||
                f.animationId === eventAnimationID ||
                f.id === eventAnimationID ||
                f.name === eventAnimationID
              );
            }) || null
          : null;
        // console.log('[enter_animation][anim14] normalized', {
        //   rawAnimation: payload?.animation ?? null,
        //   animationId,
        //   displayName: payload?.displayName ?? null,
        //   userID: payload?.userID ?? null,
        //   hasNotif: Boolean(notif),
        //   notifAnimationID: eventAnimationID,
        //   hasTemplateURL: Boolean(notif?.templateURL),
        //   hasIconURL: Boolean(notif?.icouURL),
        //   nameTokenKey: notif?.name?.key ?? null,
        //   descTokenKey: notif?.descriptionToken?.key ?? null,
        //   filesLoaded: Boolean(filesList),
        //   filesLoadState: this.enterAnimationFilesLoadState,
        //   filesLoadSource: this.enterAnimationFilesLoadSource,
        //   filesRequestedAt: this.enterAnimationFilesLoadRequestedAt || null,
        //   filesResolvedAt: this.enterAnimationFilesLoadResolvedAt || null,
        //   filesCount: files.length,
        //   matchedFileKey: matchedFile
        //     ? matchedFile.animationID || matchedFile.animationId || matchedFile.id || matchedFile.name || null
        //     : null,
        //   matchedFileUrl: matchedFile
        //     ? matchedFile.webpURL || matchedFile.webpUrl || matchedFile.webp || matchedFile.url || matchedFile.URL || null
        //     : null,
        //   resolvedAssetSrc,
        //   fallbackTemplateURL: notif?.templateURL ?? null,
        //   resolvedEventNameText: event14.eventNameText || null,
        //   resolvedEventDescText: event14.eventDescText || null,
        //   rawMessageBody: message || null,
        //   rawEnterAnimationPayload: payload || null,
        // });
        // if (!filesList) {
        //   console.warn('[enter_animation][files] lookup without loaded files', {
        //     animationId,
        //     notifAnimationID: eventAnimationID,
        //     loadState: this.enterAnimationFilesLoadState,
        //     loadSource: this.enterAnimationFilesLoadSource,
        //     requestedAt: this.enterAnimationFilesLoadRequestedAt || null,
        //     resolvedAt: this.enterAnimationFilesLoadResolvedAt || null,
        //     roomID: this.roomID || '',
        //     userID: this.userID || '',
        //   });
        // }
      }

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
