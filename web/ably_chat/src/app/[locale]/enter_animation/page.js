'use client';
import styled from 'styled-components';
import EnterAnimationOverlay from '@/lib/EnterAnimationOverlay';
import { useEffect, useState } from 'react';
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
    const wsParam = params.get('ws');

    if (wsParam && wsParam.trim()) {
      console.log('WebSocket: found `ws` parameter, attempting to connect...');
      wsManager.connect().catch(console.error);
    }
  }, []);

  useEffect(() => {
    if (!messageAggregator) return;
    const initial = typeof messageAggregator.getHistory === 'function' ? messageAggregator.getHistory(1000) : [];
    if (initial && initial.length) {
      const anim = initial.filter(
        (m) => m?.platform === '17live' && m?.content?.get?.('messageType') === MsgType_ENTER_ANIMATION
      );
      if (anim.length) {
        setEnterAnimations((prev) => [...prev, ...anim].slice(-20));
      }
    }
    
    const handleMessagesBatch = (batch) => {
      if (!batch || batch.length === 0) return;
      const anim = batch.filter(
        (m) => m?.platform === '17live' && m?.content?.get?.('messageType') === MsgType_ENTER_ANIMATION
      );
      if (anim.length) {
        setEnterAnimations((prev) => [...prev, ...anim].slice(-20));
      }
    };

    const handleSingleMessage = (m) => {
      if (!m) return;
      if (m?.platform !== '17live') return;
      if (m?.content?.get?.('messageType') !== MsgType_ENTER_ANIMATION) return;
      setEnterAnimations((prev) => {
        if (prev.some((x) => x?.id === m.id)) return prev;
        return [...prev, m].slice(-20);
      });
    };
    
    messageAggregator.on('messages_batch', handleMessagesBatch);
    messageAggregator.on('message', handleSingleMessage);
    return () => {
      messageAggregator.off('messages_batch', handleMessagesBatch);
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
