import React from 'react';
import { useTranslations } from 'next-intl';
import styled from 'styled-components';
import SVG from './SVG';

const basePath = process.env.NEXT_PUBLIC_BASE_PATH || '/';

const Container = styled.span`
  display: inline-flex;
  align-items: center;
  gap: 5px;
`;

const LikeItem = () => {
  const t = useTranslations('ChatPage');

  return (
    <Container>
      {t('LIKE_STREAMER')}
      <SVG src={`${basePath}images/heart.svg`} width={18} height={18} />
    </Container>
  );
};

export default LikeItem;
