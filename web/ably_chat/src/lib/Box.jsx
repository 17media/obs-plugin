import styled from 'styled-components';

import {
    background,
    border,
    color,
    flexbox,
    fontSize,
    layout,
    position,
    shadow,
    space,
    textAlign,
} from 'styled-system';

const Box = styled.div`
  ${fontSize}
  ${textAlign}
  ${border}
  ${color}
  ${space}
  ${layout}
  ${position}
  ${shadow}
  ${flexbox}
  ${background}
`;

export default Box;