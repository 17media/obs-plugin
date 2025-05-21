import styled from 'styled-components';

import { LevelBadgeWrapper } from './LevelBadgeBase';
import { BD_WHITE } from './constants';

const ChatWrapper = styled.li`
  margin-bottom: 4px;
  font-size: 14px;
  overflow-wrap: break-word;

  color: ${({ color }) =>
    color || BD_WHITE};

  &:last-of-type {
    margin-bottom: 0;
  }

  ${LevelBadgeWrapper} {
    width: 25px;
    height: 14px;
    font-size: 12px;
    border-radius: 4px;
    margin-right: 4px;
    line-height: 1;
  }
`;

export default ChatWrapper;