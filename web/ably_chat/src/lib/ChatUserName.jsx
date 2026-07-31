import React from 'react';

import styled from 'styled-components';

import {
    mapLevelToTextColor,
    normalizeLevel,
} from './utils';

import IconWrapper from './IconWrapper';
import StreamerPicture from './StreamerPicture';
import { sendWSMessage } from '@/services/WSSender';

export const NameWrapper = styled.span`
  color: ${props => props.$nameColor || mapLevelToTextColor(props.$level)};
  cursor: pointer;
`;

const ChatUserName = ({
                                           openID,
                                           nameColor,
                                           displayName,
                                           level,
                                           picture,
                                           streamerPicture,
                                           isStreamer,
                                           platform,
                                           userID,
                                           roomID,
                                       }) =>
    isStreamer && streamerPicture ? (
        <IconWrapper
            onClick={() => {
                if (platform !== '17live' || !userID) return;
                sendWSMessage({
                    type: 'action',
                    payload: {
                        type: 'open_user_dialog',
                        userID,
                        displayName: openID || displayName || '',
                        picture: picture || '',
                        level: typeof level === 'number' ? level : 0,
                    },
                    platform,
                    roomID,
                    userID,
                });
            }}
        >
            <StreamerPicture src={streamerPicture} />
        </IconWrapper>
    ) : (
        <NameWrapper
            $level={normalizeLevel(level)}
            $nameColor={nameColor}
            onClick={() => {
                if (platform !== '17live' || !userID) return;
                sendWSMessage({
                    type: 'action',
                    payload: {
                        type: 'open_user_dialog',
                        userID,
                        displayName: openID || displayName || '',
                        picture: picture || '',
                        level: typeof level === 'number' ? level : 0,
                    },
                    platform,
                    roomID,
                    userID,
                });
            }}
        >
            {openID || displayName}
        </NameWrapper>
    );

export default ChatUserName;
