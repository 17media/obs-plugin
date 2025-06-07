import styled from 'styled-components';

import LevelIcon from './LevelIcon';

const LevelIconBadge = styled(LevelIcon)`
  font-size: 12px;
  border: 1px solid #ffffff;
  border-radius: 4px;
  height: 20px;
  width: auto;
  min-width: 44px;
  text-align: center;
  padding: 2px;
  text-shadow: none;

  &:after {
    content: "${props => props.level}";
    margin: 2px;
    font-size: 12px;
    font-weight: 300;
    color: #ffffff;
  }

  > svg {
    height: 14px;
    width: 14px;
    margin-right: 1px;
  }

  ${props =>
    props.isSmall &&
    `
    box-sizing: border-box;
    border: none;
    height: 14px;

    > svg {
      width: 10px;
      height: 10px;
    }
  `}
`;

export default LevelIconBadge;
