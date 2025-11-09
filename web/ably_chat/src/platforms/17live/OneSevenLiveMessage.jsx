/**
 * 17Live平台消息UI组件
 * 专门处理17Live平台消息的显示
 */

import React, { memo } from 'react';
import styled from 'styled-components';
import Chat from '@/lib/Chat';

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
          {...chatProps}
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
    asideLiveWidth: undefined,
    streamerInfo: metadata?.streamerInfo || streamerInfo
  };

  switch (type) {
    case 'comment':
      return {
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        nameColor: metadata?.textColor || '#333333',
        textColor: metadata?.textColor || '#333333',
        backgroundColor: metadata?.backgroundColor || '',
        level: author.level || 1,
        isStreamer: author.isStreamer || false,
        openID: author.id
      };

    case 'gift':
      const giftData = rawData?.giftMsg || {};
      return {
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        gift: metadata?.gift,
        luckyBag: metadata?.luckyBag,
        messageType: metadata?.luckyBag ? 32 : 13
      };

    case 'join':
      return {
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        messageType: 18
      };

    case 'ai_cohost':
      return {
        ...baseProps,
        userID: 'ai_cohost',
        displayName: author.displayName,
        messageType: 120,
        backgroundColor: metadata?.backgroundColor || '#FFFFFFE6',
        nameColor: '#527fff',
        textColor: metadata?.textColor || '#333333'
      };

    case 'poke':
      return {
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        messageType: 47,
        pokeInfo: metadata?.pokeInfo,
        isStreamer: author.isStreamer || false
      };

    default:
      return baseProps;
  }
}

function getMessageType(type) {
  const typeMap = {
    'comment': 3,
    'gift': 13,
    'join': 18,
    'ai_cohost': 120,
    'poke': 47
  };
  return typeMap[type] || 3;
}

export default OneSevenLiveMessage;