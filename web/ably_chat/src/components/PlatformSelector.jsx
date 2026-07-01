"use client";
import React, { useState, useEffect } from 'react';
import styled from 'styled-components';
import { useTranslations } from 'next-intl';
import PopoverSelect from './PopoverSelect';
import { CHAT_HEADER_SELECT_WIDTHS } from './selectWidths';

const StatusDot = styled.span`
  display: inline-block;
  width: 10px;
  height: 10px;
  border-radius: 50%;
  margin: 0 6px 0 6px;
  background: ${(p) => (p.$connected ? '#00D22E' : '#A1A9B6')};
`;

const StatusWrap = styled.span`
  display: inline-flex;
  align-items: center;
  justify-content: flex-end;
  gap: 6px;
  color: #A1A9B6;
  min-width: 0;
  max-width: 92px;
`;

const SelectedStatusWrap = styled.span`
  display: inline-flex;
  align-items: center;
  gap: 6px;
  color: #A1A9B6;
  margin-left: auto;
  padding-right: 24px;
  min-width: 0;
  max-width: 92px;
`;

const StatusText = styled.span`
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
`;

export const PlatformSelector = ({ onSelectionChange, messageAggregator }) => {
  const t = useTranslations('PlatformSelector');
  const [platformsStatus, setPlatformsStatus] = useState({});
  const [selected, setSelected] = useState('all');

  const platformDefs = [
    { id: 'all', name: t('platforms.all') },
    { id: '17live', name: t('platforms.17live') },
    { id: 'twitch', name: t('platforms.twitch') },
    { id: 'youtube', name: t('platforms.youtube') },
  ];

  const statusText = (platformId) => {
    if (platformId === 'all') return '';
    const isConnected = platformsStatus[platformId]?.status === 'connected';
    const status = isConnected ? t('status.connected') : t('status.disconnected');
    return t('status.format', { status });
  };

  const updateSelection = (value) => {
    setSelected(value);
    // Notify parent of selection change for unified filtering updates
    if (onSelectionChange) {
      onSelectionChange(value);
    }
  };

  const connectPlatform = async (platformId, config = {}) => {
    if (!messageAggregator) return;
    try {
      const statusMap = messageAggregator.getPlatformsStatus?.();
      if (!statusMap || !statusMap[platformId]) {
        await messageAggregator.addPlatform(platformId, config);
      }
      await messageAggregator.connectPlatform(platformId, config);
    } catch (error) {
      console.error(`Failed to connect ${platformId}:`, error);
    }
  };

  useEffect(() => {
    if (!messageAggregator) return;

    const handleStatusChange = ({ platformId, status }) => {
      setPlatformsStatus(prev => ({
        ...prev,
        [platformId]: { ...(prev[platformId] || {}), status },
      }));
    };

    messageAggregator.on('status_change', handleStatusChange);

    (async () => {
      // Create instances for youtube and twitch to trigger mock injection via constructors.
      // Keep them disconnected in UI; only 17live is connected by default.
      try {
        const statusMap = messageAggregator.getPlatformsStatus?.();
        if (!statusMap || !statusMap['twitch']) {
          await messageAggregator.addPlatform('twitch', {});
        }
        if (!statusMap || !statusMap['17live']) {
          await messageAggregator.addPlatform('17live', {});
        }
        if (!statusMap || !statusMap['youtube']) {
          await messageAggregator.addPlatform('youtube', {});
        }
      } catch { }

      // Connect 17live only; youtube and twitch remain disconnected but instances exist.
      await connectPlatform('17live', {});

      // Explicitly mark twitch as disconnected for status display, without connecting it.
      setPlatformsStatus(prev => ({
        ...prev,
        twitch: { ...(prev.twitch || {}), status: 'disconnected' },
      }));
      setPlatformsStatus(prev => ({
        ...prev,
        youtube: { ...(prev.youtube || {}), status: 'disconnected' },
      }));

      updateSelection('all');
    })();

    return () => {
      messageAggregator.off('status_change', handleStatusChange);
    };
  }, [messageAggregator]);

  return (
    <PopoverSelect
      ariaLabel={t('label')}
      options={platformDefs}
      value={selected}
      onChange={updateSelection}
      getOptionValue={(o) => o.id}
      getOptionLabel={(o) => o.name}
      renderSelectedRight={(o) =>
        o.id !== 'all' ? (
          <SelectedStatusWrap>
            <StatusDot $connected={platformsStatus[o.id]?.status === 'connected'} />
            <StatusText>{statusText(o.id)}</StatusText>
          </SelectedStatusWrap>
        ) : null
      }
      renderItemRight={(o) =>
        o.id !== 'all' ? (
          <StatusWrap>
            <StatusDot $connected={platformsStatus[o.id]?.status === 'connected'} />
            <StatusText>{statusText(o.id)}</StatusText>
          </StatusWrap>
        ) : null
      }
      minWidth="0"
      maxWidth={CHAT_HEADER_SELECT_WIDTHS.platform}
      width="100%"
    />
  );
};

export default PlatformSelector;
