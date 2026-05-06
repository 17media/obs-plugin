import React, { useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import styled, { css, keyframes } from 'styled-components';
import { useTranslations } from 'next-intl';
import { CDN_URL } from './constants';
import { getWebpDurationMs } from './webpDuration';

const ENTRY_MS = 1000;
const EXIT_MS = 300;

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
  ${(p) =>
    p.$phase === 'enter'
      ? css`
          animation: ${entryKf} ${ENTRY_MS}ms linear both;
        `
      : p.$phase === 'exit'
        ? css`
            animation: ${exitKf} ${EXIT_MS}ms linear both;
          `
        : css`
            animation: none;
          `}
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
  height: ${(p) => p.$h || '33px'};
  padding: 0 2px;
  border-radius: 999px;
  background: ${(p) => p.$bg || 'transparent'};
  max-width: 320px;
  overflow: hidden;
  margin-left: ${(p) => (typeof p.$ml === 'string' ? p.$ml : '-5px')};
  position: relative;
  z-index: ${(p) => (typeof p.$z === 'number' ? p.$z : 2)};
`;

const BadgeLabelWrap = styled.div`
  display: inline-flex;
  align-items: center;
  gap: 6px;
  min-width: 0;
  max-width: 320px;
`;

const MarqueeViewport = styled.div`
  min-width: 0;
  overflow: hidden;
`;

const marqueeKf = keyframes`
  0% { transform: translateX(var(--marquee-start)); }
  100% { transform: translateX(calc(-1 * var(--marquee-distance))); }
`;

const entryKf = keyframes`
  0% { transform: translateX(140%); opacity: 0; }
  100% { transform: translateX(0); opacity: 1; }
`;

const exitKf = keyframes`
  0% { transform: translateX(0); opacity: 1; }
  20% { transform: translateX(30px); opacity: 1; }
  100% { transform: translateX(-160%); opacity: 0; }
`;

const MarqueeTrack = styled.div`
  display: inline-flex;
  align-items: center;
  gap: var(--marquee-gap, 12px);
  will-change: transform;
  animation: ${(p) => (p.$animate ? marqueeKf : 'none')} var(--marquee-duration, 0ms) linear infinite;
`;

const MarqueeSpacer = styled.span`
  display: inline-block;
  width: var(--marquee-gap, 12px);
`;

const Event14TextRow = styled.div`
  display: inline-flex;
  align-items: center;
  gap: 5px;
  white-space: nowrap;
`;

const MarqueeTextWrap = styled.div`
  min-width: 0;
  max-width: 320px;
  overflow: hidden;
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
      badgeIconSrc: '/enter_animation/shield.png',
    };
  }
  if (animationId === 2) {
    return {
      ...defaultCfg,
      bg: 'linear-gradient(90deg, rgb(240, 6, 197), rgb(245, 72, 125))',
      border: 'rgba(0, 0, 0, 0.12)',
      textColor: 'rgb(240, 6, 197)',
      marqueeTextColor: 'rgb(240, 6, 197)',
      badgeIconSrc: '/enter_animation/diamond.png',
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

function ScrollingText({
  children,
  gapPx = 12,
  speedPxPerSec = 40,
  always = false,
  maxWidthPx = 320,
  padPx = 20,
}) {
  const viewportRef = useRef(null);
  const contentRef = useRef(null);
  const [anim, setAnim] = useState({ enabled: false, viewportW: 0, start: 0, distance: 0, duration: 0 });

  useLayoutEffect(() => {
    const viewport = viewportRef.current;
    const content = contentRef.current;
    if (!viewport || !content) return;

    const measure = () => {
      const contentW = content.scrollWidth || 0;
      const targetViewportW = Math.max(0, Math.min(maxWidthPx, contentW + padPx));
      const viewportW = targetViewportW;
      if ((!always && contentW <= viewportW) || viewportW === 0) {
        setAnim({ enabled: false, viewportW, start: 0, distance: 0, duration: 0 });
        return;
      }
      const start = viewportW;
      const distance = contentW + gapPx;
      const travel = start + distance;
      const duration = Math.max(1200, Math.round((travel / Math.max(1, speedPxPerSec)) * 1000));
      setAnim({ enabled: true, viewportW, start, distance, duration });
    };

    measure();
    const ro = typeof ResizeObserver !== 'undefined' ? new ResizeObserver(measure) : null;
    if (ro) {
      ro.observe(viewport);
      ro.observe(content);
    } else {
      window.addEventListener('resize', measure);
    }
    return () => {
      if (ro) ro.disconnect();
      else window.removeEventListener('resize', measure);
    };
  }, [gapPx, speedPxPerSec, always, maxWidthPx, padPx, children]);

  return (
    <MarqueeViewport ref={viewportRef} style={anim.viewportW ? { width: `${anim.viewportW}px` } : undefined}>
      <MarqueeTrack
        $animate={anim.enabled}
        style={{
          '--marquee-start': `${anim.start}px`,
          '--marquee-distance': `${anim.distance}px`,
          '--marquee-duration': `${anim.duration}ms`,
          '--marquee-gap': `${gapPx}px`,
        }}
      >
        <span ref={contentRef} style={{ display: 'inline-flex', alignItems: 'center', whiteSpace: 'nowrap' }}>
          {children}
        </span>
        {anim.enabled ? (
          <>
            <MarqueeSpacer />
            <span style={{ display: 'inline-flex', alignItems: 'center', whiteSpace: 'nowrap' }}>{children}</span>
          </>
        ) : null}
      </MarqueeTrack>
    </MarqueeViewport>
  );
}

export default function EnterAnimationOverlay({ events, onConsume }) {
  const t = useTranslations('ChatPage');
  const [current, setCurrent] = useState(null);
  const [showAnim, setShowAnim] = useState(false);
  const [phase, setPhase] = useState('idle');
  const phaseRef = useRef('idle');
  const holdMsRef = useRef(1300);
  const phaseTimersRef = useRef([]);
  const currentKeyRef = useRef('');
  const aniLoadedRef = useRef(false);
  const exitTimerRef = useRef(null);
  const failSafeTimerRef = useRef(null);

  useEffect(() => {
    phaseRef.current = phase;
  }, [phase]);

  useEffect(() => {
    return () => {
      if (phaseTimersRef.current.length) {
        phaseTimersRef.current.forEach((id) => clearTimeout(id));
        phaseTimersRef.current = [];
      }
      if (exitTimerRef.current) {
        clearTimeout(exitTimerRef.current);
        exitTimerRef.current = null;
      }
      if (failSafeTimerRef.current) {
        clearTimeout(failSafeTimerRef.current);
        failSafeTimerRef.current = null;
      }
    };
  }, []);

  const scheduleExitAfter = (ms) => {
    if (exitTimerRef.current) clearTimeout(exitTimerRef.current);
    exitTimerRef.current = setTimeout(() => {
      setShowAnim(false);
      setPhase('exit');
    }, Math.max(0, Number(ms || 0)));
  };

  useEffect(() => {
    if (current || !events || events.length === 0) return;

    const next = events[0];
    currentKeyRef.current = String(next?.id || '');
    setCurrent(next);
    onConsume?.(next);
    setShowAnim(false);
    setPhase('enter');
    aniLoadedRef.current = false;
    if (phaseTimersRef.current.length) {
      phaseTimersRef.current.forEach((id) => clearTimeout(id));
      phaseTimersRef.current = [];
    }
    if (exitTimerRef.current) {
      clearTimeout(exitTimerRef.current);
      exitTimerRef.current = null;
    }
    if (failSafeTimerRef.current) {
      clearTimeout(failSafeTimerRef.current);
      failSafeTimerRef.current = null;
    }

    const holdMs =
      next?.content?.getIn?.(['enterAnimation', 'durationMs']) ||
      next?.content?.getIn?.(['enterAnimation', 'duration']) ||
      1300;
    holdMsRef.current = Number(holdMs || 1300);

    const src =
      normalizeAssetSrc(
        next?.content?.getIn?.(['enterAnimation', 'assetSrc']) ||
        next?.content?.getIn?.(['enterAnimation', 'localSrc']) ||
        next?.content?.getIn?.(['enterAnimation', 'src']) ||
        next?.content?.getIn?.(['enterAnimation', 'asset']) ||
        next?.content?.getIn?.(['enterAnimation', 'fileName']) ||
        next?.content?.getIn?.(['enterAnimation', 'file'])
      ) || '';

    phaseTimersRef.current.push(
      setTimeout(() => {
        setPhase('hold');
        setShowAnim(Boolean(src));
        if (!src) {
          scheduleExitAfter(holdMsRef.current);
          return;
        }

        const keyAtStart = currentKeyRef.current;
        getWebpDurationMs(src).then((ms) => {
          if (!ms) return;
          if (currentKeyRef.current !== keyAtStart) return;
          holdMsRef.current = ms;
          if (aniLoadedRef.current && phaseRef.current === 'hold') {
            scheduleExitAfter(holdMsRef.current);
          }
        });

        failSafeTimerRef.current = setTimeout(() => {
          setShowAnim(false);
          setPhase('exit');
        }, Math.max(holdMsRef.current + 2000, 8000));
      }, ENTRY_MS)
    );
  }, [current, events, onConsume]);

  useEffect(() => {
    if (phase !== 'exit') return;
    phaseTimersRef.current.push(
      setTimeout(() => {
        setCurrent(null);
        setPhase('idle');
      }, EXIT_MS)
    );
  }, [phase]);

  const view = useMemo(() => {
    if (!current) return null;
    const content = current.content;
    const enterAnimation = content?.get ? content.get('enterAnimation')?.toJS?.() : null;
    const displayName = content?.get ? content.get('displayName') : '';
    const picture = content?.get ? content.get('picture') : '';
    const animationId = Number(enterAnimation?.animationId || enterAnimation?.animation || 0);
    const badgeKey = enterAnimation?.badgeKey || enterAnimation?.textKey || '';
    const marqueeKey = enterAnimation?.marqueeKey || '';
    const rawLevel = enterAnimation?.level ?? (content?.get ? content.get('level') : undefined);
    const rawMLevel = enterAnimation?.mLevel ?? (content?.get ? content.get('mLevel') : undefined);
    const level =
      typeof rawLevel === 'number' ? rawLevel : typeof rawLevel === 'string' && rawLevel.trim() ? Number(rawLevel) : undefined;
    const mLevel =
      typeof rawMLevel === 'number' ? rawMLevel : typeof rawMLevel === 'string' && rawMLevel.trim() ? Number(rawMLevel) : undefined;
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
      const effectiveKey = marqueeKey || (animationId === 6 ? '' : 'enter_is_here');
      if (!effectiveKey) return '';
      try {
        return t(effectiveKey, { name });
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

    if (!name) return null;

    const onAniLoaded = () => {
      if (!showAnim) return;
      if (phase !== 'hold') return;
      aniLoadedRef.current = true;
      if (failSafeTimerRef.current) {
        clearTimeout(failSafeTimerRef.current);
        failSafeTimerRef.current = null;
      }
      scheduleExitAfter(holdMsRef.current);
    };

    return (
      <Wrapper>
        <Animated $phase={phase}>
          <BadgeContainer>
            {showAnim && src ? (
              <AniLayer>
                <AniImage src={src} alt="" onLoad={onAniLoaded} />
              </AniLayer>
            ) : null}
            <Card $bg={cfg.bg} $border={cfg.border} $color={cfg.textColor}>
              <BadgeRow>
                <AvatarBadgeGroup>
                  <Avatar style={avatarUrl ? { backgroundImage: `url(${avatarUrl})` } : undefined} />
                  {isEvent14 && (safeBadgeLabel || safeMarqueeText) ? (
                    <Marquee $bg={cfg.marqueeBg} $h="22px">
                      <ScrollingText gapPx={12} speedPxPerSec={40} always>
                        <Event14TextRow>
                          {safeBadgeLabel ? (
                            <Text $fontSize={eventFontSize} $lineHeight={eventLineHeight} $color={enterAnimation?.eventNameColor}>
                              {safeBadgeLabel}
                            </Text>
                          ) : null}
                          {safeMarqueeText ? (
                            <Text $fontSize={eventFontSize} $lineHeight={eventLineHeight} $color={enterAnimation?.eventTextColor}>
                              {safeMarqueeText}
                            </Text>
                          ) : null}
                        </Event14TextRow>
                      </ScrollingText>
                    </Marquee>
                  ) : safeBadgeLabel ? (
                    <>
                      {animationId === 1 || animationId === 2 ? (
                        <Marquee $bg={cfg.marqueeBg} $h="22px">
                          {cfg.badgeIconSrc ? <BadgeIcon $size="22px" src={cfg.badgeIconSrc} alt="" /> : null}
                          <ScrollingText gapPx={12} speedPxPerSec={40} always>
                            <Text $fontSize="12px" $lineHeight="22px">
                              {safeBadgeLabel}
                            </Text>
                          </ScrollingText>
                        </Marquee>
                      ) : (
                        <BadgeLabelWrap>
                          {cfg.badgeIconSrc ? <BadgeIcon $size="22px" src={cfg.badgeIconSrc} alt="" /> : null}
                          <ScrollingText gapPx={12} speedPxPerSec={40} always>
                            <Text $fontSize="12px" $lineHeight="22px">
                              {safeBadgeLabel}
                            </Text>
                          </ScrollingText>
                        </BadgeLabelWrap>
                      )}
                    </>
                  ) : null}
                </AvatarBadgeGroup>
                {!isEvent14 && safeMarqueeText ? (
                  <MarqueeTextWrap>
                    <ScrollingText gapPx={12} speedPxPerSec={40} always>
                      <Text $fontSize="12px" $lineHeight="22px" $color="#ffffff">
                        {safeMarqueeText}
                      </Text>
                    </ScrollingText>
                  </MarqueeTextWrap>
                ) : null}
              </BadgeRow>
            </Card>
          </BadgeContainer>
        </Animated>
      </Wrapper>
    );
  }, [current, showAnim, phase, t]);

  return view;
}
