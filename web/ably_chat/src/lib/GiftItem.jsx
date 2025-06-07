import React from 'react';
import GiftIcon from './GiftIcon'; // 假设 GiftIcon.jsx 在同一目录下

const GiftItem = ({ giftInfo }) => {
  if (!giftInfo) {
    return null;
  }

  const name = giftInfo.get('name');
  const point = giftInfo.get('point');
  const icon = giftInfo.get('icon');

  return (
    <span className="gift-item">
      送給主播 <span className="gift-name">{name}</span>
      <span className="gift-point"> ({point}) </span>
      <GiftIcon icon={icon} size={30} />
    </span>
  );
};

export default GiftItem;
