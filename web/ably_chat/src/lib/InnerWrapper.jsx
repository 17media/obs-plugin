import styled, { css } from 'styled-components';

import { mapReactionToBackgroundColor, mapUserTypeToBackgroundColor } from './utils';

const userDecorationCss = css`
  background-color: ${({ $backgroundColor, $userType, $reactionType }) =>
    $backgroundColor ||
    mapUserTypeToBackgroundColor($userType) ||
    ($reactionType !== undefined && mapReactionToBackgroundColor($reactionType))};

  ${({ $textShadowColor }) =>
    $textShadowColor && `text-shadow: 1px 1px 0.5px ${$textShadowColor};`}
`;

const InnerWrapper = styled.div`
  position: relative;
  box-sizing: border-box;
  display: inline-block;
  word-break: break-all;
  overflow-wrap: anywhere;
  width: ${({ $isFullWidth }) => ($isFullWidth ? '100%' : 'fit-content')};
  max-width: ${({ $maxWidthPx }) => ($maxWidthPx ? `${$maxWidthPx}px` : '100%')};
  min-width: 0;
  padding: 5px 8px;
  ${({ $hasPaddingRight }) => $hasPaddingRight && 'padding-right: 30px;'}
  line-height: var(--chat-line-height, 24px);
  border-radius: ${({ $borderRadius }) => `${$borderRadius || 8}px`};

  ${({ $hasUserDecoration }) => $hasUserDecoration && userDecorationCss}
`;

export default InnerWrapper;
