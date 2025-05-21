import React from 'react';

import styled from 'styled-components';

import { LEVEL_COLORS } from './constants';
import { normalizeLevel } from './utils';

export const LevelBadgeWrapper = styled.span.attrs(props => ({
    style: {
        background: LEVEL_COLORS[`LEVEL_${props.normalizedLevel}`],
    },
}))`
  display: inline-flex;
  justify-content: center;
  align-items: center;
  color: #ffffff;
  border-radius: 8px;
  width: 40px;
  height: 24px;
`;

const LevelBadgeBase = ({ level, className }) => (
    <LevelBadgeWrapper
        normalizedLevel={normalizeLevel(level)}
        className={className}
    >
        {level}
    </LevelBadgeWrapper>
);

export default LevelBadgeBase;
