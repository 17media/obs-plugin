import React, { memo } from 'react';

import styled, { css } from 'styled-components';

import { MAP_INCENTIVE_SUPERFAN_BADGE_COLOR, IncentiveSuperfanLevelBadgeSize } from './constants';
import IcSuperfanLevel from './IcSuperfanLevel';

const Wrapper = styled.div`
  box-sizing: border-box;
  display: inline-flex;
  align-items: center;
  justify-content: space-around;
  height: 14px;
  border-radius: 4px;
  padding: 0 2px;
  background: #ffffff;
  text-shadow: none;

  ${({ color, size }) => css`
    min-width: ${size === IncentiveSuperfanLevelBadgeSize.NORMAL
    ? '36px'
    : '32px'};
    border: 1px solid ${color};
  `}
`;

const Level = styled.span`
  height: 10px;
  font-style: normal;
  font-weight: 500;
  font-size: 10px;
  line-height: 10px;
  text-align: center;
  color: ${({ color }) => color};
`;

const IncentiveSuperfanBadge = ({
                                    level,
                                    size = IncentiveSuperfanLevelBadgeSize.NORMAL,
                                }) => {
    const color = MAP_INCENTIVE_SUPERFAN_BADGE_COLOR[level];

    return (
        <Wrapper size={size} color={color}>
            <IcSuperfanLevel width={10} height={10} fill={color} />
            <Level color={color}>{level}</Level>
        </Wrapper>
    );
};

export default memo(IncentiveSuperfanBadge);
