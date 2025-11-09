/**
 * YouTube平台消息UI组件
 * 专门处理YouTube平台消息的显示
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

const AuthorBadge = styled.span`
  background-color: #ff0000;
  color: white;
  font-size: 10px;
  padding: 2px 4px;
  border-radius: 2px;
  margin-left: 4px;
  font-weight: bold;
`;

export const YouTubeMessage = memo(({ message, streamerInfo, asideLiveWidth }) => {
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
          src="/images/youtube.svg" 
          alt="YouTube" 
          title="YouTube"
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
    messageType: 'COMMENT',
    content,
    asideLiveWidth: undefined,
    streamerInfo
  };

  switch (type) {
    case 'comment':
      return {
        ...baseProps,
        userID: author.id,
        displayName: author.displayName,
        nameColor: author.isChatOwner ? '#ffd700' : 
                   author.isChatModerator ? '#5e84f1' :
                   author.isVerified ? '#c0c0c0' : '#333333',
        textColor: '#333333',
        backgroundColor: '',
        level: 1, // YouTube没有等级系统
        isStreamer: author.isChatOwner,
        openID: author.id,
        // 添加YouTube特有的标识
        isVerified: author.isVerified,
        isModerator: author.isChatModerator,
        isSponsor: author.isChatSponsor
      };

    default:
      return baseProps;
  }
}

export default YouTubeMessage;