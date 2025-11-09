/**
 * 17Live平台处理器
 * 处理17Live平台的消息连接和处理
 */

import { BasePlatform } from '../BasePlatform';
import * as Ably from 'ably';
import { nanoid } from 'nanoid';
import { fromJS } from 'immutable';
import { getAblyDecodeData } from '@/util/getAblyDecodeData';
import { getAblyTokenFromServer, getGifts, getGiftByID, getRoomInfo } from '@/api';

export class OneSevenLivePlatform extends BasePlatform {
  constructor() {
    super('17live', '17Live');
    this.ablyClient = null;
    this.channel = null;
    this.roomInfo = null;
    this.gifts = null;
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

  processRawMessage(rawData) {
    const { type } = rawData;
    
    switch (type) {
      case 'COMMENT':
        return this.processCommentMessage(rawData);
      case 'NEW_GIFT':
      case 'NEW_LUCKYBAG':
        return this.processGiftMessage(rawData);
      case 'JOIN_ROOM':
        return this.processJoinMessage(rawData);
      case 'AI_COHOST_MESSAGE':
        return this.processAICohostMessage(rawData);
      case 'POKE':
        return this.processPokeMessage(rawData);
      default:
        console.warn('未知的17Live消息类型:', type);
        return null;
    }
  }

  processCommentMessage(data) {
    const { commentMsg } = data;
    const { displayUser, barrage, content } = commentMsg;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'comment',
      content: content,
      author: {
        id: displayUser.userID,
        name: displayUser.userName,
        displayName: displayUser.displayName,
        level: displayUser.level,
        isStreamer: displayUser.userID === this.roomInfo?.userInfo?.userID
      },
      timestamp: Date.now(),
      rawData: data,
      metadata: {
        barrage,
        textColor: commentMsg.comment?.textColor,
        backgroundColor: commentMsg.comment?.backgroundColor,
        streamerInfo: this.roomInfo?.userInfo
      }
    };
  }

  processGiftMessage(data) {
    const { giftMsg } = data;
    const { displayUser, giftID, giftNum, extID } = giftMsg;
    const gift = getGiftByID(giftID);
    const luckyBag = extID ? getGiftByID(extID) : undefined;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'gift',
      content: `${displayUser.displayName} 送出了 ${giftNum} 个 ${gift?.name || '礼物'}`,
      author: {
        id: displayUser.userID,
        name: displayUser.userName,
        displayName: displayUser.displayName,
        level: displayUser.level
      },
      timestamp: Date.now(),
      rawData: data,
      metadata: {
        gift,
        giftNum,
        giftID,
        luckyBag,
        streamerInfo: this.roomInfo?.userInfo
      }
    };
  }

  processJoinMessage(data) {
    // 与 Ably.jsx 一致：JOIN_ROOM 也使用 commentMsg 结构
    const { commentMsg } = data;
    const { displayUser } = commentMsg || {};
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'join',
      content: `${displayUser.displayName} 加入了直播间`,
      author: {
        id: displayUser.userID,
        name: displayUser.userName,
        displayName: displayUser.displayName,
        level: displayUser.level
      },
      timestamp: Date.now(),
      rawData: data
    };
  }

  processAICohostMessage(data) {
    const { aiCohostMsg } = data;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'ai_cohost',
      content: aiCohostMsg.commentTxt,
      author: {
        id: 'ai_cohost',
        name: 'AI_COHOST',
        displayName: 'AI助手',
        isAI: true
      },
      timestamp: Date.now(),
      rawData: data,
      metadata: {
        backgroundColor: '#FFFFFFE6',
        textColor: '#333333',
        streamerInfo: this.roomInfo?.userInfo
      }
    };
  }

  processPokeMessage(data) {
    const { pokeInfo } = data;
    const { sender } = pokeInfo;
    
    return {
      id: nanoid(),
      platform: this.platformId,
      type: 'poke',
      content: `${sender.displayName} 戳了一下`,
      author: {
        id: sender.userID,
        name: sender.userName,
        displayName: sender.displayName,
        level: sender.level,
        isStreamer: sender.userID === this.roomInfo?.userInfo?.userID
      },
      timestamp: Date.now(),
      rawData: data,
      metadata: {
        pokeInfo,
        streamerInfo: this.roomInfo?.userInfo
      }
    };
  }

  async sendMessage(message) {
    // 17Live目前不支持发送消息
    throw new Error('17Live平台不支持发送消息');
  }
}