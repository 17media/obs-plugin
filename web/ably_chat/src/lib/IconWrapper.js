import styled from 'styled-components';

import StreamerPicture from './StreamerPicture';

const IconWrapper = styled.span`
  display: inline-block;
  position: relative;
  height: 1.6em;
  margin-left: 2px;
  vertical-align: middle;
  svg,
  ${StreamerPicture} {
    position: relative;
    top: 50%;
    transform: translateY(-50%);
  }
`;

export default IconWrapper;