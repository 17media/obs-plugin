"use client";
import React, { useState, useEffect, useRef } from 'react';
import styled from 'styled-components';
import {useTranslations} from 'next-intl';

const Wrapper = styled.div`
  position: relative;
  width: 162px;
`;

const Trigger = styled.button`
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 4px;
  isolation: isolate;
  width: 100%;
  height: 32px;
  padding: 6px 12px;
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
  color: #A1A9B6;
  font-family: Inter, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, "Apple Color Emoji", "Segoe UI Emoji";
  font-weight: 400;
  font-size: 16px;
  line-height: 22px;
  white-space: nowrap;
  cursor: pointer;
  &:hover { background: #4A4F5D; }
`;

export const PlatformSelector = ({ onPlatformChange, onSelectionChange, messageAggregator }) => {
  const t = useTranslations('PlatformSelector');
  const [platformsStatus, setPlatformsStatus] = useState({});
  const [selected, setSelected] = useState('all');
  const [open, setOpen] = useState(false);
  const triggerRef = useRef(null);
  const popoverRef = useRef(null);

  const platformDefs = [
    { id: 'all', name: t('platforms.all') },
    { id: '17live', name: t('platforms.17live') },
    { id: 'youtube', name: t('platforms.youtube') },
    { id: 'twitch', name: t('platforms.twitch') },
  ];

  const statusText = (platformId) => {
    if (platformId === 'all') return '';
    const isConnected = platformsStatus[platformId]?.status === 'connected';
    const status = isConnected ? t('status.connected') : t('status.disconnected');
    return t('status.format', {status});
  };

  const selectedLabel = (() => {
    const def = platformDefs.find(d => d.id === selected);
    if (!def) return t('platforms.all');
    if (def.id === 'all') return def.name;
    return `${def.name} ${statusText(def.id)}`;
  })();

  const updateSelection = (value) => {
    setSelected(value);
    setOpen(false);
    // 先通知父组件整体选择变化，便于父组件一次性更新筛选集合
    if (onSelectionChange) {
      onSelectionChange(value);
    }
    if (!onPlatformChange) return;

    if (value === 'all') {
      ['17live', 'youtube', 'twitch'].forEach(p => onPlatformChange(p, true));
    } else {
      ['17live', 'youtube', 'twitch'].forEach(p => onPlatformChange(p, p === value));
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
      console.error(`连接 ${platformId} 失败:`, error);
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
      // 添加所有平台占位（存在则跳过）
      for (const p of ['17live', 'youtube', 'twitch']) {
        try {
          const statusMap = messageAggregator.getPlatformsStatus?.();
          if (!statusMap || !statusMap[p]) {
            await messageAggregator.addPlatform(p, {});
          }
        } catch { }
      }
      // 连接 17live（无需配置）
      await connectPlatform('17live', {});
      if (onPlatformChange) onPlatformChange('17live', true);
      // 标记其他平台为未连线
      setPlatformsStatus(prev => ({
        ...prev,
        youtube: { ...(prev.youtube || {}), status: 'disconnected' },
        twitch: { ...(prev.twitch || {}), status: 'disconnected' },
      }));
      updateSelection('all');
    })();

    return () => {
      messageAggregator.off('status_change', handleStatusChange);
    };
  }, [messageAggregator]);

  // 点击外部关闭 & ESC 关闭
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
        <LabelText>{selectedLabel}</LabelText>
        <RightChevron aria-hidden="true" $open={open} />
      </Trigger>
      {open && (
        <Popover ref={popoverRef}>
          <List>
            {platformDefs.map((p) => (
              <Item key={p.id} onClick={() => updateSelection(p.id)}>
                {p.name} {statusText(p.id)}
              </Item>
            ))}
          </List>
        </Popover>
      )}
    </Wrapper>
  );
};

export default PlatformSelector;