import React from 'react';

const GiftIcon = ({ icon, size = 24 }) => {
  const iconUrl = `https://cdn.17app.co/${icon}`
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
