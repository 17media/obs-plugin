import React from 'react';

import { USER_STREAMER } from './constants';
import CheckingLevelBadge from './CheckingLevelBadge';

const CheckingLevel = ({
                                            checkingLevel,
                                            userType,
                                            marginLeft,
                                            marginRight,
                                        }) => {
    return checkingLevel && checkingLevel >= 2 && userType !== USER_STREAMER ? (
        <CheckingLevelBadge
            checkingLevel={checkingLevel}
            marginLeft={marginLeft}
            marginRight={marginRight}
        />
    ) : null;
};

export default CheckingLevel;
