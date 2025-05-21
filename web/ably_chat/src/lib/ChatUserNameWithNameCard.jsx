import React, { useMemo } from 'react';

import ChatUserName from './ChatUserName';
import { IncentiveSuperfanLevelBadgeType } from './constants';

const ChatUserNameWithNameCard = ({
                                      level,
                                      levelBadges,
                                      isConcert,
                                      openID,
                                      nameColor,
                                      displayName,
                                      streamerInfo,
                                      isStreamer,
                                  }) => {

    const superfanLevelBadge = useMemo(
        () =>
            levelBadges?.find(
                badge => badge.styleID === IncentiveSuperfanLevelBadgeType.SUPERFAN
            ),
        [levelBadges]
    );

    const userLevelBadge = useMemo(
        () =>
            levelBadges?.find(
                badge => badge.styleID === IncentiveSuperfanLevelBadgeType.USER
            ),
        [levelBadges]
    );

    return (
        <>
            {isConcert ? (
                <ChatUserName
                    openID={openID}
                    nameColor={nameColor}
                    displayName={displayName}
                    superfanLevel={superfanLevelBadge?.level}
                    level={userLevelBadge?.level || level}
                    picture={streamerInfo?.get('picture')}
                    isStreamer={isStreamer}
                />
            ) : (
                <ChatUserName
                    nameColor={nameColor}
                    openID={openID}
                    displayName={displayName}
                    superfanLevel={superfanLevelBadge?.level}
                    level={userLevelBadge?.level || level}
                    picture={streamerInfo?.get('picture')}
                    isStreamer={isStreamer}
                />
            )}
        </>
    );
};

export default ChatUserNameWithNameCard;
