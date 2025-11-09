/**
 * 17Live平台处理器
 * 处理17Live平台的消息连接和处理
 */

import { BasePlatform } from '../../BasePlatform';
import * as Ably from 'ably';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { getAblyDecodeData } from '@/platforms/17live/util/getAblyDecodeData';
import {
  MsgType_COMMENT,
  MsgType_NEW_GIFT,
  MsgType_JOIN_ROOM,
  MsgType_NEW_LUCKYBAG,
  MsgType_AI_COHOST_MESSAGE,
  MsgType_POKE,
} from '@/lib/constants';
import { getAblyTokenFromServer, getGifts, getGiftByID, getRoomInfo } from '../api';
import { sendWSMessage } from '@/services/WSSender';

// Dev-only mock messages (same as Ably.jsx)
import giftdata from '@/../public/mock/chat_new_gift_2.json';
import comment from '@/../public/mock/chat_message.json';
import newjoin from '@/../public/mock/chat_new_join.json';
import aicohost from '@/../public/mock/chat_ai_cohost.json';
import pokeone from '@/../public/mock/chat_poke.json';
import pokeall from '@/../public/mock/chat_poke_all.json';
import pokeback0 from '@/../public/mock/chat_poke_back_0.json';
import pokeback1 from '@/../public/mock/chat_poke_back_1.json';
import pokeback2 from '@/../public/mock/chat_poke_back_2.json';
import pokeback3 from '@/../public/mock/chat_poke_back_3.json';

export class OneSevenLivePlatform extends BasePlatform {
  constructor() {
    super('17live', '17Live');
    this.ablyClient = null;
    this.channel = null;
    this.roomInfo = null;
    this.gifts = null;
    this.roomID = '';
    this.userID = '';
  }

  async connect(config = {}) {
    try {
      let { roomID, userID, ablyToken } = config;

      // 允许无配置时从 URL 获取 roomID/userID（与 Ably.jsx 对齐）
      if (!roomID || !userID) {
        const urlParams = new URLSearchParams(typeof window !== 'undefined' ? window.location.search : '');
        roomID = roomID || urlParams.get('roomID') || '';
        userID = userID || urlParams.get('userID') || '';
      }

      // 保存连接上下文
      this.roomID = roomID;
      this.userID = userID;

      // 拉取房间信息与礼物信息
      this.roomInfo = await getRoomInfo();
      await getGifts();

      // 创建Ably客户端（与 Ably.jsx 保持一致设置）
      this.ablyClient = new Ably.Realtime({
        environment: '17media',
        fallbackHosts: [
          '17-media-a-fallback.ably-realtime.com',
          '17-media-b-fallback.ably-realtime.com',
          '17-media-c-fallback.ably-realtime.com',
        ],
        authCallback: async (data, cb) => {
          try {
            // 由调用方传入的 token 优先；否则继续抛出以让上层处理
            if (ablyToken) {
              cb(null, ablyToken);
              return;
            }
            // 与 Ably.jsx 保持一致：按 roomID 请求 token
            const token = await getAblyTokenFromServer(roomID);
            cb(null, token);
          } catch (e) {
            cb(e);
          }
        },
      });

      // 按 Ably.jsx 约定订阅房间 ID 频道
      this.channel = this.ablyClient.channels.get(roomID);
      
      // 监听消息
      this.channel.subscribe((message) => {
        this.handleAblyMessage(message);
      });

      this.isConnected = true;
      this.emit('connected', { platform: this.platformId, roomID });

      if (process.env.NODE_ENV === 'development') {
        const mocks = [
          this.prepareIndexedChat(comment),
          this.prepareIndexedChat(newjoin),
          this.prepareIndexedChat(giftdata),
          this.prepareIndexedChat(aicohost),
          this.prepareIndexedChat(pokeone),
          this.prepareIndexedChat(pokeall),
          this.prepareIndexedChat(pokeback0),
          this.prepareIndexedChat(pokeback1),
          this.prepareIndexedChat(pokeback2),
          this.prepareIndexedChat(pokeback3),
        ];
        console.log('mocks', mocks);
        mocks.forEach((mock) => {
          if (mock) {
            const unifiedMessage = {
              id: mock.get('id'),
              platform: this.platformId,
              timestamp: Date.now(),
              content: mock,
            };
            this.enqueueMessage(unifiedMessage);
          }
        });
      }
      
    } catch (error) {
      console.error('17Live连接失败:', error);
      this.emit('error', { platform: this.platformId, error });
      throw error;
    }
  }

  async disconnect() {
    try {
      if (this.channel) {
        this.channel.unsubscribe();
      }
      if (this.ablyClient) {
        this.ablyClient.close();
      }
      this.isConnected = false;
      this.emit('disconnected', { platform: this.platformId });
    } catch (error) {
      console.error('17Live断开连接失败:', error);
      throw error;
    }
  }

  handleAblyMessage(message) {
    try {
      const decodedData = getAblyDecodeData(message);
      const unifiedMessage = this.processRawMessage(decodedData);

      if (unifiedMessage) {
        this.enqueueMessage(unifiedMessage);
      }
    } catch (error) {
      console.error('处理17Live消息失败:', error);
      this.emit('error', { platform: this.platformId, error });
    }
  }

  decodeMessageData(data) {
    // Ably.jsx 使用 gzip_base64 + pako 解码，平台层直接透传已解码数据
    return data;
  }

  // 与 Ably.jsx#prepareIndexedChat 保持一致的内容构建，返回 Immutable 对象
  prepareIndexedChat(message) {
    const id = nanoid();
    const streamerInfo = this.roomInfo?.userInfo;

    if (message.type === MsgType_NEW_GIFT || message.type === MsgType_NEW_LUCKYBAG) {
      const { displayUser, barrage, ...restGift } = message?.giftMsg || {};
      const gift = getGiftByID(restGift?.giftID);

      if (message.type === MsgType_NEW_LUCKYBAG && restGift?.extID) {
        const luckyBag = getGiftByID(restGift.extID);
        const indexedGift = fromJS({
          ...restGift,
          ...(displayUser || {}),
          barrage,
          id,
          messageType: message.type,
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
        messageType: message.type,
        gift,
        streamerInfo,
      });
      return indexedGift;
    } else if (message.type === MsgType_AI_COHOST_MESSAGE) {
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
        messageType: message.type,
        streamerInfo,
      });
      return indexedChat;
    } else if (message.type === MsgType_POKE) {
      const { sender } = message?.pokeInfo || {};
      return fromJS({
        ...(sender || {}),
        isStreamer: sender?.userID && streamerInfo?.userID ? sender.userID === streamerInfo.userID : false,
        pokeInfo: message?.pokeInfo,
        id,
        messageType: message.type,
        streamerInfo,
      });
    }

    const { displayUser, barrage, ...restChat } = message?.commentMsg || {};
    const indexedChat = fromJS({
      ...restChat,
      ...(displayUser || {}),
      barrage,
      id,
      messageType: message.type,
      streamerInfo,
    });
    return indexedChat;
  }

  processRawMessage(rawData) {
    const { type } = rawData;
    
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
      default:
        console.warn('未知的17Live消息类型:', type);
        return null;
    }
  }

  processCommentMessage(data) {
    const content = this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  processGiftMessage(data) {
    const content = this.prepareIndexedChat(data);

    const gift = content.get('gift');
    
    if (gift) {
      let playData = {
        type: 'play_vff',
        vffURL: gift.get('vffURL'),
        vffJson: gift.get('vffJson'),
      }
      const composite = decodeMessage.giftMsg?.giftMetas[0]?.composite;
      if (composite) {
        playData.compositeData = Object.fromEntries(composite.map(item => [item.tag, item.imageURL]));
      }
      sendWSMessage({
        type: data.type,
        payload: playData,
        roomID: chat.get('roomID'),
        userID: chat.get('userID'),
      });
    }

    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  processJoinMessage(data) {
    const content = this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  processAICohostMessage(data) {
    const content = this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }

  processPokeMessage(data) {
    const content = this.prepareIndexedChat(data);
    return {
      id: content.get('id'),
      platform: this.platformId,
      timestamp: Date.now(),
      content,
    };
  }
}