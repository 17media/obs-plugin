/**
 * 17Live平台消息UI组件
 * 专门处理17Live平台消息的显示
 */

import React, { memo } from 'react';
import { fromJS } from 'immutable';
import styled from 'styled-components';
import Chat from '@/lib/Chat';
import { getChatProps } from '@/util/getChatProps';
import {
  MsgType_COMMENT,
  MsgType_NEW_GIFT,
  MsgType_JOIN_ROOM,
  MsgType_AI_COHOST_MESSAGE,
  MsgType_POKE,
  MsgType_NEW_LUCKYBAG,
} from '@/lib/constants';

const PlatformIcon = styled.img`
  width: 16px;
  height: 16px;
  margin-right: 8px;
  border-radius: 50%;
`;

const MessageWrapper = styled.div`
  display: flex;
  align-items: flex-start;
  margin-bottom: 4px;
  position: relative;
`;

const PlatformBadge = styled.div`
  position: absolute;
  left: -20px;
  top: 50%;
  transform: translateY(-50%);
  width: 16px;
  height: 16px;
  display: flex;
  align-items: center;
  justify-content: center;
`;

export const OneSevenLiveMessage = memo(({ message, streamerInfo, asideLiveWidth }) => {
  const {
    id,
    type,
    content,
    author,
    metadata,
    rawData
  } = message;

  // 转换消息格式以兼容现有的Chat组件
  const chatProps = convertToChatProps(message, streamerInfo);

  return (
    <MessageWrapper>
      <PlatformBadge>
        <PlatformIcon
          src="/images/17live.svg"
          alt="17Live"
          title="17Live"
        />
      </PlatformBadge>
      <div style={{ flex: 1 }}>
        <Chat 
          {...getChatProps(chatProps)}
          asideLiveWidth={asideLiveWidth}
        />
      </div>
    </MessageWrapper>
  );
});

/**
 * 将统一消息格式转换为Chat组件需要的props
 */
export function convertToChatProps(message, streamerInfo) {
  const { id, type, content, author, metadata, rawData } = message;

  const baseProps = {
    id,
    messageType: getMessageType(type),
    content,
    streamerInfo: metadata?.streamerInfo || streamerInfo,
  };

  switch (type) {
    case 'comment':
      return fromJS({
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        name: { textColor: metadata?.textColor || '#333333' },
        comment: { textColor: metadata?.textColor || '#333333' },
        backgroundColor: metadata?.backgroundColor || '',
        level: author.level || 1,
        isStreamer: author.isStreamer || false,
        openID: author.id,
      });

    case 'gift':
      const giftData = rawData?.giftMsg || {};
      return fromJS({
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        gift: metadata?.gift,
        luckyBag: metadata?.luckyBag,
        messageType: metadata?.luckyBag ? MsgType_NEW_LUCKYBAG : MsgType_NEW_GIFT,
      });

    case 'join':
      return fromJS({
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        name: { textColor: metadata?.textColor || '#333333' },
        comment: { textColor: metadata?.textColor || '#333333' },
        backgroundColor: metadata?.backgroundColor || '',
        messageType: MsgType_JOIN_ROOM,
      });

    case 'ai_cohost':
      return fromJS({
        ...baseProps,
        userID: 'ai_cohost',
        displayName: author.displayName,
        messageType: MsgType_AI_COHOST_MESSAGE,
        backgroundColor: metadata?.backgroundColor || '#FFFFFFE6',
        name: { textColor: '#527fff' },
        comment: { textColor: metadata?.textColor || '#333333' },
      });

    case 'poke':
      return fromJS({
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        messageType: MsgType_POKE,
        pokeInfo: metadata?.pokeInfo,
        isStreamer: author.isStreamer || false,
      });

    default:
      return fromJS(baseProps);
  }
}

function getMessageType(type) {
  const typeMap = {
    comment: MsgType_COMMENT,
    gift: MsgType_NEW_GIFT,
    join: MsgType_JOIN_ROOM,
    ai_cohost: MsgType_AI_COHOST_MESSAGE,
    poke: MsgType_POKE,
  };
  return typeMap[type] || MsgType_COMMENT;
}

export default OneSevenLiveMessage;