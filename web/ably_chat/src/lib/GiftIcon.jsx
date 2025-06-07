import React from 'react';
import { CDN_URL } from './constants';
const GiftIcon = ({ icon, size = 24 }) => {
  const iconUrl = `${CDN_URL}/${icon}`
  const style = {
    width: `${size}px`,
    height: `${size}px`,
    backgroundImage: `url(${iconUrl})`,
    backgroundSize: 'cover',
    backgroundPosition: 'center',
    display: 'inline-block',
  };

  return <div style={style} />;
};

export default GiftIcon;
