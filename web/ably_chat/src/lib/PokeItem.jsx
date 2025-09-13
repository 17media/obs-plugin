import React from 'react';
import { useTranslations } from 'next-intl';
import styled from 'styled-components';
import SVG from './SVG';

const basePath = process.env.NEXT_PUBLIC_BASE_PATH || '';

const PokeContainer = styled.div`
  display: inline-flex;
  align-items: center;
  gap: 5px;
`;

const PokeItem = ({ pokeInfo, streamerInfo }) => {
  const t = useTranslations('ChatPage');

  if (!pokeInfo) {
    return null;
  }

  const userID = streamerInfo.get("userID");


  // Poke all viewers
  if (pokeInfo.get("receiverGroup") == 2) {
    return (
      <PokeContainer>
        {t('POKE_ALL')}
        <SVG src={`${basePath}/images/ic-poke.svg`} width={24} height={18} />
      </PokeContainer>
    );
  }

  // Streamer pokes specific user
  if (pokeInfo.get("receiverGroup") == 0 && pokeInfo.get("sender").get("userID") == userID) {
    return (
      <PokeContainer>
        {t('POKE_ONE', { receiverName: pokeInfo.get("receiver").get("displayName") })}
        <SVG src={`${basePath}/images/ic-poke.svg`} width={24} height={18} />
      </PokeContainer>
    );
  }

  // Poke back to streamer
  if (pokeInfo.get("isPokeBack")) {
    return (
      <PokeContainer>
        {t('POKE_BACK')}
        <SVG src={`${basePath}/images/ic-poke.svg`} width={24} height={18} />
      </PokeContainer>
    );
  }

  return null;
};

export default PokeItem;