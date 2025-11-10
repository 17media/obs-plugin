'use client';
import styled from 'styled-components';
import MultiPlatformChat from '@/components/MultiPlatformChat';
import { useEffect } from 'react';
import { wsManager } from '@/services/WebSocketManager';

const PageContainer = styled.div`
  min-height: 100vh;
  background-color: #f9fafb;
  
  &.dark {
    background-color: #111827;
  }
`;

export default function Home({params}) {
  useEffect(() => {
    if (typeof window === 'undefined') return;
    const params = new URLSearchParams(window.location.search);
    const wsParam = params.get('ws');

    if (wsParam && wsParam.trim()) {
      console.log('WebSocket: found `ws` parameter, attempting to connect...');
      const onOpen = ({ url }) => console.log('WebSocket connected:', url);
      const onClose = ({ url }) => console.log('WebSocket closed:', url);
      const onError = (err) => console.error('WebSocket error:', err);

      wsManager.on('open', onOpen);
      wsManager.on('close', onClose);
      wsManager.on('error', onError);

      wsManager.connect().then((url) => {
        console.log('WebSocket connect attempt started:', url);
      }).catch((err) => {
        console.error('WebSocket connect failed:', err);
      });

      return () => {
        wsManager.off('open', onOpen);
        wsManager.off('close', onClose);
        wsManager.off('error', onError);
      };
    } else {
      console.log('WebSocket: `ws` parameter missing, connection not started.');
    }
  }, []);

  return (
    <PageContainer>
      <MultiPlatformChat />
    </PageContainer>
  );
}
