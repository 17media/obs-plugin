import React from 'react';

import styled from 'styled-components';

import LevelBadgeBase from './LevelBadgeBase';
import LevelIconBadge from './LevelIconBadge';
import IncentiveSuperfanBadge from './IncentiveSuperfanBadge';
import { IncentiveSuperfanLevelBadgeType, LevelBadgeSize, LevelBadgeSizeToSuperFanBadgeSize } from './constants';

const UserLevelBadge = styled(LevelBadgeBase)`
  width: 36px;
  height: 14px;
  font-size: 12px;
  border-radius: 4px;
  text-shadow: none;
`;

const LevelBadge = ({
    styleID,
    level,
    size = LevelBadgeSize.NORMAL,
    isUserLevelHasIcon = true,
}) => {
    switch (styleID) {
        case IncentiveSuperfanLevelBadgeType.SUPERFAN:
            return (
                <IncentiveSuperfanBadge
                    size={LevelBadgeSizeToSuperFanBadgeSize[size]}
                    level={level}
                />
            );

        case IncentiveSuperfanLevelBadgeType.USER:
            return isUserLevelHasIcon ? (
                <LevelIconBadge level={level} isSmall={size === LevelBadgeSize.SMALL} />
            ) : (
                <UserLevelBadge level={level} />
            );

        default:
            return <></>;
    }
};

export default LevelBadge;
