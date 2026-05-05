import React, { useEffect, useMemo, useRef, useState } from 'react';
import styled, { keyframes } from 'styled-components';
import { useTranslations } from 'next-intl';
import { CDN_URL } from './constants';

const Wrapper = styled.div`
  position: absolute;
  left: 16px;
  right: 16px;
  bottom: 48px;
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
  padding: 2px;
  background: ${(p) => p.$bg || '#ffffff'};
  border-radius: 999px;
  max-width: 100%;
  border: 1px solid ${(p) => p.$border || 'rgba(0, 0, 0, 0.18)'};
  overflow: hidden;
  color: ${(p) => p.$color || '#000000'};
`;

const BadgeRow = styled.div`
  display: inline-flex;
  align-items: center;
  gap: 12px;
  min-width: 0;
`;

const Avatar = styled.div`
  width: 33px;
  height: 33px;
  border-radius: 50%;
  flex-shrink: 0;
  position: relative;
  z-index: 1;
  background-color: rgba(255, 255, 255, 0.2);
  background-size: cover;
  background-position: center;
  border: 1px solid rgba(0, 0, 0, 0.18);
`;

const BadgeIcon = styled.img`
  width: ${(p) => p.$size || '33px'};
  height: ${(p) => p.$size || '33px'};
  object-fit: contain;
  flex-shrink: 0;
`;

const Text = styled.div`
  display: inline-flex;
  align-items: center;
  font-size: ${(p) => p.$fontSize || '18px'};
  line-height: ${(p) => p.$lineHeight || '33px'};
  color: ${(p) => p.$color || 'inherit'};
  margin-left: ${(p) => p.$ml || '0'};
  white-space: nowrap;
  flex-shrink: 0;
`;

const AvatarBadgeGroup = styled.div`
  display: inline-flex;
  align-items: center;
  min-width: 0;
`;

const Marquee = styled.div`
  display: inline-flex;
  align-items: center;
  gap: 6px;
  height: 33px;
  padding: 0 2px;
  border-radius: 999px;
  background: ${(p) => p.$bg || 'transparent'};
  max-width: 320px;
  overflow: hidden;
  text-overflow: ellipsis;
  margin-left: -5px;
  position: relative;
  z-index: 2;
`;

const AniImage = styled.img`
  display: block;
  width: auto;
  height: auto;
  max-width: none;
  max-height: none;
`;

const BadgeContainer = styled.div`
  position: relative;
  display: inline-flex;
  align-items: flex-end;
`;

const AniLayer = styled.div`
  position: absolute;
  left: 0;
  bottom: calc(100% + 4px);
  transform: none;
`;

function normalizeAssetSrc(raw) {
  if (!raw) return '';
  if (raw.startsWith('http://') || raw.startsWith('https://') || raw.startsWith('/')) return raw;
  return `/enter_animation/${raw}`;
}

function getBadgeRenderConfig(animationId) {
  const defaultCfg = {
    bg: '#ffffff',
    border: 'rgba(0, 0, 0, 0.18)',
    textColor: '#000000',
    marqueeBg: '#ffffff',
    marqueeTextColor: '#000000',
    badgeIconSrc: '',
  };

  if (animationId === 1) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(88, 252, 255), rgb(196, 172, 255), rgb(255, 179, 244))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
      marqueeBg: 'rgb(255, 138, 212)',
      marqueeTextColor: '#ffffff',
    };
  }
  if (animationId === 2) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(240, 6, 197), rgb(245, 72, 125))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: 'rgb(240, 6, 197)',
      marqueeTextColor: 'rgb(240, 6, 197)',
    };
  }
  if (animationId === 3) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(246, 105, 108), rgb(246, 147, 85))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
    };
  }
  if (animationId === 4) {
    return {
      ...defaultCfg,
      bg: 'rgb(255, 104, 249)',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
    };
  }
  if (animationId === 5) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(247, 80, 188), rgb(158, 123, 255))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
    };
  }
  if (animationId === 6) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(255, 209, 0), rgb(246, 105, 108))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
    };
  }
  if ((animationId >= 7 && animationId <= 10) || animationId === 15) {
    return {
      ...defaultCfg,
      bg: 'rgb(21, 144, 63)',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
    };
  }
  if (animationId === 12) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(255, 255, 255), rgb(176, 196, 209))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: 'rgb(26, 37, 65)',
      marqueeBg: 'rgb(231, 231, 231)',
      marqueeTextColor: 'rgb(26, 37, 65)',
      badgeIconSrc: '/enter_animation/igMlevelSettingBallerMiddle@3x.png',
    };
  }
  if (animationId === 13) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(255, 248, 230), rgb(249, 199, 127))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: 'rgb(0, 0, 0)',
      marqueeBg: 'rgb(254, 239, 201)',
      marqueeTextColor: 'rgb(0, 0, 0)',
      badgeIconSrc: '/enter_animation/igMlevelSettingBallerHigh@3x.png',
    };
  }
  if (animationId === 16) {
    return {
      ...defaultCfg,
      bg: 'rgb(51, 206, 176)',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
    };
  }
  if (animationId === 17) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(255, 242, 20), rgb(177, 131, 255))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: '#ffffff',
    };
  }

  return defaultCfg;
}

function toCssLinearGradient(from, to) {
  if (!from && !to) return '';
  if (from && to) return `linear-gradient(90deg, ${from}, ${to})`;
  return from || to || '';
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
  const [showAnim, setShowAnim] = useState(false);
  const timerRef = useRef(null);
  const animTimersRef = useRef([]);

  useEffect(() => {
    return () => {
      if (timerRef.current) {
        clearTimeout(timerRef.current);
        timerRef.current = null;
      }
      if (animTimersRef.current.length) {
        animTimersRef.current.forEach((id) => clearTimeout(id));
        animTimersRef.current = [];
      }
    };
  }, []);

  useEffect(() => {
    if (current || !events || events.length === 0) return;

    const next = events[0];
    setCurrent(next);
    onConsume?.(next);
    setShowAnim(false);
    if (animTimersRef.current.length) {
      animTimersRef.current.forEach((id) => clearTimeout(id));
      animTimersRef.current = [];
    }

    const holdMs =
      next?.content?.getIn?.(['enterAnimation', 'durationMs']) ||
      next?.content?.getIn?.(['enterAnimation', 'duration']) ||
      1300;
    const totalMs = 1000 + Number(holdMs || 1300) + 300;
    const src =
      normalizeAssetSrc(
        next?.content?.getIn?.(['enterAnimation', 'assetSrc']) ||
        next?.content?.getIn?.(['enterAnimation', 'localSrc']) ||
        next?.content?.getIn?.(['enterAnimation', 'src']) ||
        next?.content?.getIn?.(['enterAnimation', 'asset']) ||
        next?.content?.getIn?.(['enterAnimation', 'fileName']) ||
        next?.content?.getIn?.(['enterAnimation', 'file'])
      ) || '';

    if (src) {
      animTimersRef.current.push(
        setTimeout(() => {
          setShowAnim(true);
        }, 1000)
      );
      animTimersRef.current.push(
        setTimeout(() => {
          setShowAnim(false);
        }, 1000 + Number(holdMs || 1300))
      );
    }

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
    const animationId = Number(enterAnimation?.animationId || enterAnimation?.animation || 0);
    const badgeKey = enterAnimation?.badgeKey || enterAnimation?.textKey || '';
    const marqueeKey = enterAnimation?.marqueeKey || '';
    const level = enterAnimation?.level;
    const mLevel = enterAnimation?.mLevel;
    const name =
      enterAnimation?.displayName ||
      enterAnimation?.nickname ||
      enterAnimation?.userName ||
      displayName ||
      '';

    const isEvent14 = animationId === 14 && enterAnimation?.eventNotifMsg;

    const badgeLabel = (() => {
      if (isEvent14) return enterAnimation?.eventNameText || '';
      if (!badgeKey) return '';
      try {
        return t(badgeKey, { name, level, mLevel });
      } catch {
        return '';
      }
    })();

    const marqueeText = (() => {
      if (isEvent14) return enterAnimation?.eventDescText || '';
      if (!marqueeKey) return '';
      try {
        return t(marqueeKey, { name });
      } catch {
        return '';
      }
    })();
    const hasVisibleText = (s) =>
      typeof s === 'string' && s.replace(/[\s\u200B\uFEFF]/g, '').length > 0;
    const safeBadgeLabel = hasVisibleText(badgeLabel) ? badgeLabel.trim() : '';
    const safeMarqueeText = hasVisibleText(marqueeText) ? marqueeText.trim() : '';

    const src =
      normalizeAssetSrc(
        enterAnimation?.assetSrc ||
        enterAnimation?.localSrc ||
        enterAnimation?.src ||
        enterAnimation?.asset ||
        enterAnimation?.fileName ||
        enterAnimation?.file
      ) || '';

    const baseCfg = getBadgeRenderConfig(animationId);
    const cfg = (() => {
      if (!isEvent14) return baseCfg;
      const g = toCssLinearGradient(enterAnimation?.eventGradientFrom, enterAnimation?.eventGradientTo);
      const border = enterAnimation?.eventStrokeColor ? enterAnimation.eventStrokeColor : baseCfg.border;
      return {
        ...baseCfg,
        border,
        marqueeBg: g || baseCfg.marqueeBg,
      };
    })();
    const avatarUrl = picture ? `${CDN_URL}/${picture}` : '';
    const eventTextSize = Number(enterAnimation?.eventTextSize || 0);
    const eventFontSize = isEvent14 && Number.isFinite(eventTextSize) && eventTextSize > 0 ? `${eventTextSize}px` : '12px';
    const eventLineHeight = isEvent14 ? '22px' : '22px';

    const entryMs = 1000;
    const holdMs = Number(enterAnimation?.durationMs || 1300);
    const exitMs = 300;
    const totalMs = entryMs + holdMs + exitMs;
    const kf = computeBannerKeyframes(entryMs, holdMs, exitMs);
    if (!name) return null;

    return (
      <Wrapper>
        <Animated $kf={kf} $dur={totalMs}>
          <BadgeContainer>
            {showAnim && src ? (
              <AniLayer>
                <AniImage src={src} alt="" />
              </AniLayer>
            ) : null}
            <Card $bg={cfg.bg} $border={cfg.border} $color={cfg.textColor}>
              <BadgeRow>
                <AvatarBadgeGroup>
                  <Avatar style={avatarUrl ? { backgroundImage: `url(${avatarUrl})` } : undefined} />
                  {isEvent14 ? (
                    <>
                      {safeBadgeLabel ? (
                        <Marquee $bg={cfg.marqueeBg}>
                          <Text $fontSize={eventFontSize} $lineHeight={eventLineHeight} $color={enterAnimation?.eventNameColor}>
                            {safeBadgeLabel}
                          </Text>
                        </Marquee>
                      ) : null}
                      {safeMarqueeText ? (
                        <Text
                          $fontSize={eventFontSize}
                          $lineHeight={eventLineHeight}
                          $color={enterAnimation?.eventTextColor}
                          $ml={safeBadgeLabel ? '5px' : '0'}
                        >
                          {safeMarqueeText}
                        </Text>
                      ) : null}
                    </>
                  ) : safeBadgeLabel ? (
                    <Marquee $bg={cfg.marqueeBg}>
                      {cfg.badgeIconSrc ? <BadgeIcon $size="22px" src={cfg.badgeIconSrc} alt="" /> : null}
                      <Text $fontSize="12px" $lineHeight="22px">
                        {safeBadgeLabel}
                      </Text>
                    </Marquee>
                  ) : null}
                </AvatarBadgeGroup>
                {!isEvent14 && safeMarqueeText ? <Text $color={cfg.marqueeTextColor}>{safeMarqueeText}</Text> : null}
              </BadgeRow>
            </Card>
          </BadgeContainer>
        </Animated>
      </Wrapper>
    );
  }, [current, showAnim, t]);

  return view;
}
