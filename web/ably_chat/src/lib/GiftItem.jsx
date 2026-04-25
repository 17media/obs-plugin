import React from 'react';
import styled from 'styled-components';
import GiftIcon from './GiftIcon'; // Assume GiftIcon.jsx is in the same directory
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
  color: #FFFFFF;
  font-size: 0.875rem;
`;

const GiftItem = ({ messageType, giftInfo, giftPoint, luckyBagInfo }) => {
  const t = useTranslations('ChatPage');

  if (!giftInfo) {
    return (
      <GiftItemContainer>
        <>
          {t('GIVE_GIFT_DEFAULT', {
            point: giftPoint
          })}
        </>
      </GiftItemContainer>
    )
  }

  if (messageType === MsgType_NEW_LUCKYBAG && !luckyBagInfo) {
    messageType = MsgType_NEW_GIFT; // Default to gift if lucky bag info is missing
  }

  const name = giftInfo.get('name');
  const point = giftInfo.get('point');
  const icon = giftInfo.get('icon');
  const isEventPointEnabled = giftInfo.get('isEventPointEnabled') || giftInfo.get('isEventPointEnbled');
  const eventPoint = giftInfo.get('eventPoint');

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
            {isEventPointEnabled && typeof eventPoint !== 'undefined' && eventPoint !== null ? (
              <GiftPoint>{t('EVENT_POINTS_SUFFIX', { eventPoint })}</GiftPoint>
            ) : null}
          </>
        )
      }
      <GiftIcon icon={icon} size={30} />
    </GiftItemContainer>
  );
};

export default GiftItem;
