import styled, { css } from 'styled-components';

import { mapReactionToBackgroundColor, mapUserTypeToBackgroundColor } from './utils';

const userDecorationCss = css`
  background-color: ${({ backgroundColor, userType, reactionType }) =>
    backgroundColor ||
    mapUserTypeToBackgroundColor(userType) ||
    (reactionType !== undefined && mapReactionToBackgroundColor(reactionType))};

  ${({ textShadowColor }) =>
    textShadowColor && `text-shadow: 1px 1px 0.5px ${textShadowColor};`}
`;

const InnerWrapper = styled.div`
  position: relative;
  word-break: break-all;
  width: ${({ isFullWidth }) => (isFullWidth ? '100%' : 'fit-content')};
  padding: 5px 8px;
  ${({ hasPaddingRight }) => hasPaddingRight && 'padding-right: 30px;'}
  line-height: 24px;
  border-radius: ${({ borderRadius }) => `${borderRadius || 8}px`};

  ${({ hasUserDecoration }) => hasUserDecoration && userDecorationCss}
`;

export default InnerWrapper;