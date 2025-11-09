import React from 'react';
import styled from 'styled-components';
import GiftIcon from './GiftIcon'; // 假设 GiftIcon.jsx 在同一目录下
import { useTranslations } from 'next-intl';
import { MsgType_NEW_LUCKYBAG } from './constants';

const GiftItemContainer = styled.span`
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
`;

const GiftName = styled.span`
  font-weight: 500;
  color: #f59e0b;
`;

const GiftPoint = styled.span`
  color: #6b7280;
  font-size: 0.875rem;
`;

const GiftItem = ({ messageType, giftInfo, luckyBagInfo }) => {
  const t = useTranslations('ChatPage');

  if (!giftInfo) {
    return null;
  }

  if (messageType === MsgType_NEW_LUCKYBAG && !luckyBagInfo) {
    return null;
  }

  const name = giftInfo.get('name');
  const point = giftInfo.get('point');
  const icon = giftInfo.get('icon');

  return (
    <GiftItemContainer>
      {messageType === MsgType_NEW_LUCKYBAG ? 
        t('GIVE_LUCKYBAG_GIFT', {
          giftName: name,
          luckyBagName: luckyBagInfo.get('name'),
          point
        }) 
        : 
        (
          <>
            {t('GIVE_GIFT')}
            <GiftName>{name}</GiftName>
            <GiftPoint> ({point}) </GiftPoint>
          </>
        )
      }
      <GiftIcon icon={icon} size={30} />
    </GiftItemContainer>
  );
};

export default GiftItem;
