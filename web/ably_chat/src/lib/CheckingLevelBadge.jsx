import React, { memo } from 'react';

import styled from 'styled-components';

import { mapCheckingLevelImage } from './constants';
import { getCheckingLevelImage, getWebp2xURL } from './utils';

const CheckingLevelWrapper = styled.span`
  margin-left: ${props => props.marginLeft || 0}px;
  margin-right: ${props => props.marginRight || 0}px;
  vertical-align: middle;
  margin-top: -2px;
  display: inline-flex;
  align-items: center;
  height: 14px;
  width: 24px;
`;

const CheckingLevelBadge = ({
                                                                   checkingLevel = 2,
                                                                   marginLeft,
                                                                   marginRight,
                                                               }) => {
    const checkingLevelImageName = mapCheckingLevelImage[checkingLevel];
    const imageURL = getWebp2xURL(getCheckingLevelImage(checkingLevelImageName), {
        has2x: true,
    });

    if (!checkingLevelImageName) {
        return null;
    }

    return (
        <CheckingLevelWrapper marginLeft={marginLeft} marginRight={marginRight}>
            <img width={24} height={14} src={imageURL} />
        </CheckingLevelWrapper>
    );
};

export default memo(CheckingLevelBadge);