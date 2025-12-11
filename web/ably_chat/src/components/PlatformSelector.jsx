"use client";
import React, { useState, useEffect, useRef } from 'react';
import styled from 'styled-components';
import { useTranslations } from 'next-intl';

const Wrapper = styled.div`
  position: relative;
  /* Adaptive width to avoid multilingual text overflow */
  width: auto;
  min-width: 162px;
  max-width: 320px;
`;

const Trigger = styled.button`
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
  gap: 4px;
  isolation: isolate;
  width: 100%;
  height: 32px;
  padding: 6px 12px;
  /* Background color set to #3C404C to improve text contrast */
  background: #3C404C;
  border-radius: 6px;
  border: none;
  outline: none;
  color: #A1A9B6;
  font-family: Inter, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, "Apple Color Emoji", "Segoe UI Emoji";
  font-weight: 400;
  font-size: 14px;
  line-height: 20px;
  position: relative;
  cursor: pointer;
`;

const LabelText = styled.span`
  flex: 1;
  text-align: left;
  /* Long text truncation with ellipsis */
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
`;

const RightChevron = styled.span`
  position: absolute;
  right: 8px;
  top: 50%;
  transform: translateY(-50%) rotate(${(p) => (p.$open ? '-135deg' : '45deg')});
  transition: transform 150ms ease;
  width: 8px;
  height: 8px;
  border-bottom: 1px solid #B3B3B3;
  border-right: 1px solid #B3B3B3;
  pointer-events: none;
`;

const Popover = styled.div`
  position: absolute;
  top: calc(100% + 8px);
  left: 0;
  width: 100%;
  background: #3C404C;
  border: 1px solid #3C404C;
  border-radius: 8px;
  padding: 8px;
  z-index: 50;
`;

const List = styled.div`
  display: flex;
  flex-direction: column;
  gap: 8px;
`;

const Item = styled.button`
  width: 100%;
  text-align: left;
  background: transparent;
  border: none;
  border-radius: 6px;
  padding: 6px 8px;
  /* Unified text color and font size */
  color: #A1A9B6;
  font-family: Inter, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, "Apple Color Emoji", "Segoe UI Emoji";
  font-weight: 400;
  font-size: 14px;
  line-height: 20px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  cursor: pointer;
  &:hover { background: #4A4F5D; }
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
`;

const StatusDot = styled.span`
  display: inline-block;
  width: 10px;
  height: 10px;
  border-radius: 50%;
  margin: 0 6px 0 6px;
  background: ${(p) => (p.$connected ? '#00D22E' : '#A1A9B6')};
`;

const NameLabel = styled.span`
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
`;

const StatusWrap = styled.span`
  display: inline-flex;
  align-items: center;
  justify-content: flex-end;
  gap: 6px;
  color: #A1A9B6;
`;

const SelectedStatusWrap = styled.span`
  display: inline-flex;
  align-items: center;
  gap: 6px;
  color: #A1A9B6;
  margin-left: auto;
  padding-right: 24px;
`;

export const PlatformSelector = ({ onSelectionChange, messageAggregator }) => {
  const t = useTranslations('PlatformSelector');
  const [platformsStatus, setPlatformsStatus] = useState({});
  const [selected, setSelected] = useState('all');
  const [open, setOpen] = useState(false);
  const triggerRef = useRef(null);
  const popoverRef = useRef(null);

  const platformDefs = [
    { id: 'all', name: t('platforms.all') },
    { id: '17live', name: t('platforms.17live') },
    { id: 'twitch', name: t('platforms.twitch') },
  ];

  const statusText = (platformId) => {
    if (platformId === 'all') return '';
    const isConnected = platformsStatus[platformId]?.status === 'connected';
    const status = isConnected ? t('status.connected') : t('status.disconnected');
    return t('status.format', { status });
  };

  const updateSelection = (value) => {
    setSelected(value);
    setOpen(false);
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
      } catch { }

      // Connect 17live only; youtube and twitch remain disconnected but instances exist.
      await connectPlatform('17live', {});

      // Explicitly mark twitch as disconnected for status display, without connecting it.
      setPlatformsStatus(prev => ({
        ...prev,
        twitch: { ...(prev.twitch || {}), status: 'disconnected' },
      }));

      updateSelection('all');
    })();

    return () => {
      messageAggregator.off('status_change', handleStatusChange);
    };
  }, [messageAggregator]);

  // Close on outside click and ESC
  useEffect(() => {
    const handleClickOutside = (e) => {
      if (!open) return;
      const t = triggerRef.current;
      const p = popoverRef.current;
      if (t && t.contains(e.target)) return;
      if (p && p.contains(e.target)) return;
      setOpen(false);
    };
    const handleKey = (e) => {
      if (e.key === 'Escape') setOpen(false);
    };
    document.addEventListener('mousedown', handleClickOutside);
    document.addEventListener('keyup', handleKey);
    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
      document.removeEventListener('keyup', handleKey);
    };
  }, [open]);

  return (
    <Wrapper>
      <Trigger
        aria-label={t('label')}
        onClick={() => setOpen((prev) => !prev)}
        ref={triggerRef}
      >
        <LabelText>{platformDefs.find(d => d.id === selected)?.name || t('platforms.all')}</LabelText>
        {selected !== 'all' && (
          <SelectedStatusWrap>
            <StatusDot $connected={platformsStatus[selected]?.status === 'connected'} />
            <span>{statusText(selected)}</span>
          </SelectedStatusWrap>
        )}
        <RightChevron aria-hidden="true" $open={open} />
      </Trigger>
      {open && (
        <Popover ref={popoverRef}>
          <List>
            {platformDefs.map((p) => (
              <Item key={p.id} onClick={() => updateSelection(p.id)}>
                <NameLabel>{p.name}</NameLabel>
                {p.id !== 'all' && (
                  <StatusWrap>
                    <StatusDot $connected={platformsStatus[p.id]?.status === 'connected'} />
                    <span>{statusText(p.id)}</span>
                  </StatusWrap>
                )}
              </Item>
            ))}
          </List>
        </Popover>
      )}
    </Wrapper>
  );
};

export default PlatformSelector;
