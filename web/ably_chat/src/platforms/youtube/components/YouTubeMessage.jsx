/**
 * YouTube platform message UI component
 * Specially handles YouTube platform message display
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

  // Convert message format to be compatible with existing Chat component
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
 * Convert unified message format to props required by Chat component
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
        level: 1, // YouTube does not have a level system
        isStreamer: author.isChatOwner,
        openID: author.id,
        // Add YouTube-specific flags
        isVerified: author.isVerified,
        isModerator: author.isChatModerator,
        isSponsor: author.isChatSponsor
      };

    default:
      return baseProps;
  }
}

export default YouTubeMessage;