import React, { memo } from 'react';

import styled from 'styled-components';

import Multiline from './Multiline';
import SVG from './SVG';

import ChatUserNameWithNameCard from './ChatUserNameWithNameCard';
import CommentFrameWrapper from './CommentFrameWrapper';
import LevelBadge from './LevelBadge';
import BadgeImage from './BadgeImage';
import ChatWrapper from './ChatWrapper';
import InnerWrapper from './InnerWrapper';
import useComment from './hooks';
import Box from './Box';
import GiftItem from './GiftItem';

import {
    BD_WHITE,
    DEFAULT_COMMENT_BG_COLOR,
    DEFAULT_GUARDIAN_COMMENT_BG_COLOR,
    DEFAULT_STREAMER_COMMENT_BG_COLOR,
    REACTION_TYPE,
    USER_GUARDIAN,
    USER_STREAMER,
    MsgType_COMMENT,
    MsgType_NEW_GIFT,
    MsgType_NEW_LUCKYBAG,
    MsgType_JOIN_ROOM,
    MsgType_AI_COHOST_MESSAGE,
} from './constants';
import {
    getUserType,
    mapCommentShadowColor,
    mapUserTypeToColor,
    mapUserTypeToIcon,
} from './utils';
import CheckingLevel from './CheckingLevel';

const MultilineDesktop = styled(Multiline)`
    margin-left: 4px;
    color: ${({color}) => color};
`;

const renderMessageContent = (messageType, content, gift = null) => {
    switch (messageType) {
        case MsgType_COMMENT:
        case MsgType_JOIN_ROOM:
        case MsgType_AI_COHOST_MESSAGE:
            return content;
        case MsgType_NEW_GIFT:
        case MsgType_NEW_LUCKYBAG:
            return <GiftItem giftInfo={gift} />;
        default:
            return null;
    }
};

// Note: image/svg should assigned with a fixed width for comment frame calculation
const Chat = ({
                  id,
                  messageType,
                  openID,
                  displayName,
                  userID,
                  content,
                  level,
                  levelBadges: originalLevelBadges,
                  isGuardian,
                  isVIP,
                  isConcert,
                  streamerInfo,
                  type,
                  checkingLevel,
                  roomID,
                  isInvisible,
                  border,
                  nameColor,
                  textColor = BD_WHITE,
                  commentShadowColor,
                  backgroundColor = '',
                  prefixBadges,
                  middleBadge,
                  topRightBadge,
                  asideLiveWidth,
                  gift,
              }) => {
    const {
        commentRef,
        size,
        levelBadges,
        prefixBadgeContents,
        skipAnimationFrame,
        handleAnimationEnd,
    } = useComment({
        levelBadges: originalLevelBadges,
        prefixBadges,
        asideLiveWidth,
    });

    const isDefaultBackgroundColor = [
        DEFAULT_COMMENT_BG_COLOR,
        DEFAULT_STREAMER_COMMENT_BG_COLOR,
        DEFAULT_GUARDIAN_COMMENT_BG_COLOR,
    ].includes(backgroundColor);

    /**
     * 在 desktop 只針對 pubnub 有帶樣式資訊的一般訊息裝飾
     * 且 留言文字色/底色/陰影，只在 user 有設置底色時才會一起套用
     */
    const hasUserDecoration = !isDefaultBackgroundColor;
    const textShadowColor = mapCommentShadowColor(commentShadowColor);

    const isAiCohost = messageType === MsgType_AI_COHOST_MESSAGE;
    const userType = getUserType(
        {
            isGuardian,
            isVIP,
            userID,
            isSystem: false,
            isAiCohost,
        },
        streamerInfo
    );

    const SVGSrc = mapUserTypeToIcon(userType);
    const userTypeColor = mapUserTypeToColor(userType);

    if (isInvisible) {
        return (
            <ChatWrapper>
                <Multiline>{content}</Multiline>
            </ChatWrapper>
        );
    }

    const hasTopRightBadge = !!topRightBadge;
    const isStreamer = userType === USER_STREAMER;

    return (
        <ChatWrapper>
            <CommentFrameWrapper
                id={id}
                size={size}
                border={border}
                skipAnimationFrame={skipAnimationFrame}
                onAnimationEnd={handleAnimationEnd}
            >
                <InnerWrapper
                    ref={commentRef}
                    isFullWidth={false}
                    userType={userType}
                    reactionType={
                        messageType === REACTION_TYPE && type
                    }
                    hasUserDecoration={hasUserDecoration}
                    backgroundColor={backgroundColor}
                    textShadowColor={textShadowColor}
                    borderRadius={border?.get('commentCornerRadius')}
                    hasPaddingRight={hasTopRightBadge}
                >
                    {levelBadges?.map(badge => (
                        <LevelBadge
                            isUserLevelHasIcon={false}
                            styleID={badge.styleID}
                            level={badge.level}
                            key={badge.styleID}
                        />
                    ))}

                    {/* 前綴徽章 */}
                    {prefixBadgeContents && (
                        <Box display="inline" mr={1}>
                            {prefixBadgeContents}
                        </Box>
                    )}

                    {/* AI Cohost 頭像 */}
                    {
                        isAiCohost && (
                            <SVG 
                                src={'/images/ig_AIBaby_background.svg' } 
                                width={16}
                                height={16}
                                style={{
                                    clipPath: 'circle(50%)',
                                    marginRight: 8,
                                }}
                            />
                        )
                    }

                    <ChatUserNameWithNameCard
                        isStreamer={isStreamer}
                        level={level}
                        levelBadges={levelBadges}
                        isConcert={isConcert}
                        openID={openID || ''}
                        displayName={displayName || ''}
                        streamerInfo={streamerInfo}
                        userID={userID}
                        roomID={roomID}
                        nameColor={hasUserDecoration ? nameColor : ''}
                    />

                    {/* 後綴徽章 */}
                    {middleBadge && <BadgeImage src={middleBadge}/>}

                    {SVGSrc && (
                        <Box
                            display="inline-block"
                            width={userType === USER_GUARDIAN ? 24 : 21}
                        >
                            <SVG src={SVGSrc}/>
                        </Box>
                    )}

                    <CheckingLevel
                        checkingLevel={checkingLevel}
                        userType={userType}
                        marginLeft={4}
                        marginRight={4}
                    />

                    <MultilineDesktop
                        color={hasUserDecoration ? textColor : userTypeColor}
                    >
                        {renderMessageContent(messageType, content, gift)}
                    </MultilineDesktop>

                    {/* 右上徽章 */}
                    {hasTopRightBadge && (
                        <Box position="absolute" top="5px" right="6px">
                            <BadgeImage src={topRightBadge}/>
                        </Box>
                    )}
                </InnerWrapper>
            </CommentFrameWrapper>
        </ChatWrapper>
    );
};

// we don't need to update sent chat when streamerInfo update
export default memo(
    Chat,
    (prevProps, nextProps) =>
        prevProps.asideLiveWidth === nextProps.asideLiveWidth
);
