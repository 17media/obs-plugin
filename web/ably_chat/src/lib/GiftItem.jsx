import React from 'react';
import GiftIcon from './GiftIcon'; // 假设 GiftIcon.jsx 在同一目录下
import { useTranslations } from 'next-intl';
import { MsgType_NEW_LUCKYBAG } from './constants';

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
    <span className="gift-item">
      {messageType === MsgType_NEW_LUCKYBAG ? 
        t('GIVE_LUCKYBAG_GIFT', {
          giftName: name,
          luckyBagName: luckyBagInfo.get('name'),
          giftNumber: 1
        }) 
        : 
        (
          <>
            {t('GIVE_GIFT')}
            <span className="gift-name">{name}</span>
            <span className="gift-point"> ({point}) </span>
          </>
        )
      }
      <GiftIcon icon={icon} size={30} />
    </span>
  );
};

export default GiftItem;
