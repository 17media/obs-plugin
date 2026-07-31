'use client';
import styled from 'styled-components';
import EnterAnimationOverlay from '@/lib/EnterAnimationOverlay';
import { useEffect, useRef, useState } from 'react';
import { wsManager } from '@/services/WebSocketManager';
import { messageAggregator } from '@/services/MessageAggregator';
import { MsgType_ENTER_ANIMATION } from '@/lib/constants';

const Container = styled.div`
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  position: relative;
  background: transparent;
`;

export default function EnterAnimationPage() {
  const [enterAnimations, setEnterAnimations] = useState([]);
  const anim14LogEnabledRef = useRef(false);

  useEffect(() => {
    if (process.env.NODE_ENV !== 'development') return;
    if (typeof window === 'undefined') return;

    const params = new URLSearchParams(window.location.search);
    const wsParam = params.get('ws');
    if (wsParam && wsParam.trim()) return;

    if (!messageAggregator) return;

    (async () => {
      try {
        const mockAnimParam = params.get('mockAnim');
        const mockAnimId = mockAnimParam ? Number(mockAnimParam) : 0;
        const config = mockAnimId ? { devMockEnterAnimationId: mockAnimId } : {};

        const statusMap = messageAggregator.getPlatformsStatus?.();
        if (!statusMap || !statusMap['17live']) {
          await messageAggregator.addPlatform('17live', config);
        }
        await messageAggregator.connectPlatform('17live', config);
      } catch (err) {
        console.error('Failed to connect 17live platform for enter animation page:', err);
      }
    })();
  }, []);

  useEffect(() => {
    if (typeof window === 'undefined') return;
    const params = new URLSearchParams(window.location.search);
    const v = params.get('logAnim14') || params.get('debugAnim14') || '';
    anim14LogEnabledRef.current =
      process.env.NODE_ENV === 'development' ||
      v === '1' ||
      v.toLowerCase() === 'true' ||
      v.toLowerCase() === 'yes';
  }, []);

  useEffect(() => {
    if (typeof window === 'undefined') return;
    const params = new URLSearchParams(window.location.search);
    const wsParam = params.get('ws');

    if (wsParam && wsParam.trim()) {
      console.log('WebSocket: found `ws` parameter, attempting to connect...');
      wsManager.connect().catch(console.error);
    }
  }, []);

  useEffect(() => {
    if (!messageAggregator) return;
    const handleSingleMessage = (m) => {
      if (!m) return;
      if (m?.platform !== '17live') return;
      if (m?.content?.get?.('messageType') !== MsgType_ENTER_ANIMATION) return;
      const animationId = Number(
        m?.content?.getIn?.(['enterAnimation', 'animationId']) ||
          m?.content?.getIn?.(['enterAnimation', 'animation']) ||
          0
      );
      if (animationId === 14 && anim14LogEnabledRef.current) {
        try {
          const ea = m?.content?.get?.('enterAnimation');
          const eaJs = ea?.toJS?.() || null;
          const notif = ea?.get ? ea.get('eventNotifMsg') : eaJs?.eventNotifMsg;
          const notifJs = notif?.toJS?.() || notif || null;
          const contentJs = m?.content?.toJS?.() || null;
          // console.log('[enter_animation][anim14] received', {
          //   id: m?.id,
          //   createdAt: m?.createdAt || m?.timestamp || m?.time || null,
          //   animationId,
          //   durationMs: eaJs?.durationMs ?? ea?.get?.('durationMs') ?? null,
          //   assetSrc: eaJs?.assetSrc ?? ea?.get?.('assetSrc') ?? null,
          //   eventAnimationID: eaJs?.eventAnimationID ?? ea?.get?.('eventAnimationID') ?? null,
          //   eventTextSize: eaJs?.eventTextSize ?? ea?.get?.('eventTextSize') ?? null,
          //   eventTextColor: eaJs?.eventTextColor ?? ea?.get?.('eventTextColor') ?? null,
          //   eventGradientFrom: eaJs?.eventGradientFrom ?? ea?.get?.('eventGradientFrom') ?? null,
          //   eventGradientTo: eaJs?.eventGradientTo ?? ea?.get?.('eventGradientTo') ?? null,
          //   notifAnimationID: notifJs?.animationID ?? null,
          //   templateURL: notifJs?.templateURL ?? null,
          //   icouURL: notifJs?.icouURL ?? null,
          //   nameTokenKey: notifJs?.name?.key ?? null,
          //   descTokenKey: notifJs?.descriptionToken?.key ?? null,
          //   hasNotif: Boolean(notifJs),
          //   hasTemplateURL: Boolean(notifJs?.templateURL),
          //   fullNormalizedContent: contentJs,
          //   fullNormalizedEnterAnimation: eaJs,
          // });
        } catch (err) {
          // console.warn('[enter_animation][anim14] log failed', err);
        }
      }
      setEnterAnimations((prev) => {
        if (prev.some((x) => x?.id === m.id)) return prev;
        return [...prev, m].slice(-20);
      });
    };
    
    messageAggregator.on('message', handleSingleMessage);
    return () => {
      messageAggregator.off('message', handleSingleMessage);
    };
  }, []);

  return (
    <Container>
      <EnterAnimationOverlay
        events={enterAnimations}
        onConsume={(evt) => {
          if (!evt?.id) return;
          setEnterAnimations((prev) => prev.filter((x) => x?.id !== evt.id));
        }}
      />
    </Container>
  );
}
