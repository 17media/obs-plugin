import React, { useEffect, useMemo, useRef, useState } from 'react';
import styled, { keyframes } from 'styled-components';
import { useTranslations } from 'next-intl';
import { CDN_URL } from './constants';

const Wrapper = styled.div`
  position: absolute;
  left: 16px;
  right: 16px;
  bottom: 16px;
  display: flex;
  justify-content: flex-start;
  pointer-events: none;
  z-index: 50;
`;

const Animated = styled.div`
  will-change: transform, opacity;
  animation: ${(p) => p.$kf} ${(p) => p.$dur}ms linear both;
`;

const Card = styled.div`
  display: inline-flex;
  align-items: center;
  gap: 10px;
  padding: 8px 10px;
  background: rgba(0, 0, 0, 0.72);
  border-radius: 999px;
  max-width: 100%;
  border: 1px solid rgba(255, 255, 255, 0.35);
  overflow: hidden;
`;

const BadgeRow = styled.div`
  display: inline-flex;
  align-items: center;
  gap: 6px;
  min-width: 0;
`;

const Avatar = styled.div`
  width: 22px;
  height: 22px;
  border-radius: 50%;
  flex-shrink: 0;
  background-color: rgba(255, 255, 255, 0.2);
  background-size: cover;
  background-position: center;
  border: 1px solid rgba(255, 255, 255, 0.35);
`;

const Pill = styled.div`
  display: inline-flex;
  align-items: center;
  height: 22px;
  padding: 0 10px;
  border-radius: 999px;
  font-size: 12px;
  line-height: 22px;
  color: #ffffff;
  white-space: nowrap;
  flex-shrink: 0;
`;

const LevelPill = styled(Pill)`
  background: rgba(255, 255, 255, 0.15);
`;

const NamePill = styled(Pill)`
  background: rgba(0, 0, 0, 0.72);
  border: 1px solid rgba(255, 255, 255, 0.25);
  max-width: 220px;
  overflow: hidden;
  text-overflow: ellipsis;
`;

const AniImage = styled.img`
  width: 160px;
  height: 56px;
  object-fit: contain;
`;

function normalizeAssetSrc(raw) {
  if (!raw) return '';
  if (raw.startsWith('http://') || raw.startsWith('https://') || raw.startsWith('/')) return raw;
  return `/enter_animation/${raw}`;
}

function buildBadgeLabel(t, enterAnimation) {
  const key = enterAnimation?.textKey || enterAnimation?.key;
  const mLevel = enterAnimation?.mLevel;
  const level = enterAnimation?.level;
  const name =
    enterAnimation?.displayName ||
    enterAnimation?.nickname ||
    enterAnimation?.userName ||
    '';

  if (key === 'mlevel_entry_notice_subscription' && typeof mLevel === 'number') {
    return `${t(key)} ${mLevel}`;
  }
  if ((key === 'LV%@' || key === 'LV%40') && typeof level === 'number') {
    return t('LV%@', { level });
  }
  if (typeof key === 'string' && key.trim()) {
    try {
      return t(key, { name, level, mLevel });
    } catch {
      return '';
    }
  }
  return '';
}

function computeBannerKeyframes(entryMs, holdMs, exitMs) {
  const total = entryMs + holdMs + exitMs;
  const p1 = Math.max(0, Math.min(100, (entryMs / total) * 100));
  const p2 = Math.max(0, Math.min(100, ((entryMs + holdMs) / total) * 100));
  const pShift = Math.max(0, Math.min(100, ((entryMs + holdMs + Math.min(60, exitMs)) / total) * 100));

  return keyframes`
    0% { transform: translateX(140%); opacity: 0; }
    ${p1}% { transform: translateX(0); opacity: 1; }
    ${p2}% { transform: translateX(0); opacity: 1; }
    ${pShift}% { transform: translateX(30px); opacity: 1; }
    100% { transform: translateX(-160%); opacity: 0; }
  `;
}

export default function EnterAnimationOverlay({ events, onConsume }) {
  const t = useTranslations('ChatPage');
  const [current, setCurrent] = useState(null);
  const timerRef = useRef(null);

  useEffect(() => {
    return () => {
      if (timerRef.current) {
        clearTimeout(timerRef.current);
        timerRef.current = null;
      }
    };
  }, []);

  useEffect(() => {
    if (current || !events || events.length === 0) return;

    const next = events[0];
    setCurrent(next);
    onConsume?.(next);

    const holdMs =
      next?.content?.getIn?.(['enterAnimation', 'durationMs']) ||
      next?.content?.getIn?.(['enterAnimation', 'duration']) ||
      1300;
    const totalMs = 1000 + Number(holdMs || 1300) + 300;

    timerRef.current = setTimeout(() => {
      setCurrent(null);
    }, totalMs);
  }, [current, events, onConsume]);

  const view = useMemo(() => {
    if (!current) return null;
    const content = current.content;
    const enterAnimation = content?.get ? content.get('enterAnimation')?.toJS?.() : null;
    const displayName = content?.get ? content.get('displayName') : '';
    const picture = content?.get ? content.get('picture') : '';
    const src =
      normalizeAssetSrc(
        enterAnimation?.localSrc ||
        enterAnimation?.src ||
        enterAnimation?.asset ||
        enterAnimation?.fileName ||
        enterAnimation?.file
      ) || '';
    const badgeLabel = buildBadgeLabel(t, enterAnimation);
    const avatarUrl = picture ? `${CDN_URL}/${picture}` : '';

    const entryMs = 1000;
    const holdMs = Number(enterAnimation?.durationMs || 1300);
    const exitMs = 300;
    const totalMs = entryMs + holdMs + exitMs;
    const kf = computeBannerKeyframes(entryMs, holdMs, exitMs);
    if (!displayName && !src) return null;

    return (
      <Wrapper>
        <Animated $kf={kf} $dur={totalMs}>
          <Card>
            <BadgeRow>
              <Avatar style={avatarUrl ? { backgroundImage: `url(${avatarUrl})` } : undefined} />
              {badgeLabel ? <LevelPill>{badgeLabel}</LevelPill> : null}
              <NamePill>{displayName}</NamePill>
            </BadgeRow>
            {src ? <AniImage src={src} alt="" /> : null}
          </Card>
        </Animated>
      </Wrapper>
    );
  }, [current, t]);

  return view;
}
