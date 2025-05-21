import React from 'react';

import styled from 'styled-components';

import {
    mapLevelToTextColor,
    normalizeLevel,
} from './utils';

import IconWrapper from './IconWrapper';
import StreamerPicture from './StreamerPicture';

export const NameWrapper = styled.span`
  color: ${props => props.nameColor || mapLevelToTextColor(props.level)};
  cursor: pointer;
`;

const ChatUserName = ({
                                           openID,
                                           nameColor,
                                           displayName,
                                           level,
                                           picture,
                                           isStreamer,
                                       }) =>
    isStreamer && picture ? (
        <IconWrapper>
            <StreamerPicture src={picture} />
        </IconWrapper>
    ) : (
        <NameWrapper
            level={normalizeLevel(level)}
            nameColor={nameColor}
        >
            {openID || displayName}
        </NameWrapper>
    );

export default ChatUserName;
