import styled from 'styled-components';

import { getDefaultImage, getPicture } from './utils';

const StreamerPictureAttrs = props => ({
    style: {
        backgroundImage: `
      url(${getPicture(props.src)}),
      url(${getDefaultImage()})
    `,
    },
});

const StreamerPicture = styled.div.attrs(StreamerPictureAttrs)`
  display: inline-block;
  width: 30px;
  height: 30px;
  box-shadow: 0 1px 1px 0 rgba(0, 0, 0, 0.2);
  border: solid 1px #3cc8d2;
  border-radius: 50%;
  background-size: cover;
  vertical-align: middle;
  cursor: pointer;
`;

export default StreamerPicture;
