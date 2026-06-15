"use client";
import React, { useState, useEffect, useRef } from 'react';
import { useTranslations } from 'next-intl';
import styled from 'styled-components';
import { messageAggregator } from '../services/MessageAggregator';
import { PlatformSelector } from './PlatformSelector';
import PopoverSelect from './PopoverSelect';
import Chat from '@/lib/Chat';
import { getChatProps } from '@/platforms/17live/util/getChatProps';
import { MsgType_ENTER_ANIMATION } from '@/lib/constants';

/**
 * Multi-platform message display component
 * Aggregates and displays messages from different platforms
 */

const Container = styled.div`
  position: relative;
  min-height: 100vh;
  background-color: #000000;
  color: #f3f4f6;
  --chat-font-size: ${(p) => p.$chatFontSize || '16px'};
  --chat-line-height: ${(p) => p.$chatLineHeight || '24px'};
`;

const Header = styled.div`
  padding: 1rem;
  border-bottom: 1px solid #1f2937;
`;

const HeaderContent = styled.div`
  max-width: 28rem;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
`;

const FontSizeGroup = styled.div`
  display: flex;
  align-items: center;
  gap: 12px;
  flex-shrink: 0;
`;

const FontSizeLabel = styled.span`
  color: #A1A9B6;
  font-family: Inter, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, "Apple Color Emoji", "Segoe UI Emoji";
  font-style: normal;
  font-weight: 400;
  font-size: 14px;
  line-height: 20px;
`;

const MessageList = styled.div`
  height: calc(100vh - 72px);
  overflow-y: auto;
  padding: 1rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  position: relative;
  /* Remove default bullets and padding inside nested lists */
  & ul,
  & ol {
    list-style: none;
    margin: 0;
    padding-left: 0;
  }
`;

const EmptyState = styled.div`
  position: absolute;
  top: 33%;
  left: 50%;
  transform: translate(-50%, -50%);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 0;
  color: #A1A9B6;
  font-size: var(--chat-font-size, 16px);
`;

const EmptyIcon = styled.img`
  width: 80px;
  height: 80px;
  margin-bottom: 12px;
`;

const MessageItem = styled.div`
  display: flex;
  align-items: center;
  gap: 0.5rem;
`;

const PlatformIcon = styled.img`
  width: 1.25rem;
  height: 1.25rem;
  border-radius: 50%;
  flex-shrink: 0;
`;

const MessageContent = styled.div`
  flex: 1;
`;

export const MultiPlatformChat = () => {
  const [messages, setMessages] = useState([]); // Raw unified message format
  const [selectedPlatform, setSelectedPlatform] = useState('all');
  const [filteredMessages, setFilteredMessages] = useState([]);
  const [fontSize, setFontSize] = useState('medium');
  const t = useTranslations('ChatPage');
  const listRef = useRef(null);
  const endRef = useRef(null);

  // Listen to message aggregator events (directly using unified format)
  useEffect(() => {
    if (!messageAggregator) return;
    const initial = typeof messageAggregator.getHistory === 'function' ? messageAggregator.getHistory(1000) : [];
    if (initial && initial.length) {
      const normal = initial.filter(
        (m) => !(m?.platform === '17live' && m?.content?.get?.('messageType') === MsgType_ENTER_ANIMATION)
      );

      if (normal.length) {
        setMessages(prev => {
          const next = [...prev, ...normal];
          return next.length > 1000 ? next.slice(-1000) : next;
        });
      }
    }
    // Consume only batch events to avoid duplicate inserts
    const handleMessagesBatch = (batch) => {
      if (!batch || batch.length === 0) return;
      const normal = batch.filter(
        (m) => !(m?.platform === '17live' && m?.content?.get?.('messageType') === MsgType_ENTER_ANIMATION)
      );
      if (normal.length) {
        setMessages(prev => {
          const next = [...prev, ...normal];
          return next.length > 1000 ? next.slice(-1000) : next;
        });
      }
    };
    messageAggregator.on('messages_batch', handleMessagesBatch);

    return () => {
      messageAggregator.off('messages_batch', handleMessagesBatch);
    };
  }, []);

  const handleSelectionChange = (value) => {
    setSelectedPlatform(value);
  };

  const fontSizeDefs = [
    { id: 'small', name: t('FONT_SIZE_SMALL') },
    { id: 'medium', name: t('FONT_SIZE_MEDIUM') },
    { id: 'large', name: t('FONT_SIZE_LARGE') },
  ];

  // Filter messages based on selected platform
  useEffect(() => {
    const next = messages.filter(m => selectedPlatform === 'all' || m.platform === selectedPlatform);
    setFilteredMessages(next);
  }, [messages, selectedPlatform]);

  // Scroll to the latest message when content exceeds the viewport
  useEffect(() => {
    const el = listRef.current;
    if (!el) return;
    const exceedsViewport = el.scrollHeight > el.clientHeight;
    if (exceedsViewport) {
      el.scrollTop = el.scrollHeight;
      // Alternatively, ensure the sentinel is visible
      endRef.current?.scrollIntoView({ behavior: 'auto', block: 'end' });
    }
  }, [filteredMessages]);

  // Platform icon mapping
  const platformIcon = (platform) => {
    switch (platform) {
      case '17live':
        return '/images/17live.svg';
      case 'twitch':
        return '/images/twitch.svg';
      case 'youtube':
        return '/images/youtube.svg';
      default:
        return '/images/17live.svg';
    }
  };

  // Platform message unification: directly use content as Immutable object

  // Render a single message (Chat component + platform icon)
  const renderMessageItem = (message, index) => {
    const immutableChat = message.content; // Immutable content generated by platform
    const chatProps = getChatProps(immutableChat);
    const { key: _key, ...safeChatProps } = chatProps || {};
    return (
      <MessageItem key={message.id || index}>
        <PlatformIcon
          src={platformIcon(message.platform)}
          alt={message.platform}
        />
        <MessageContent>
          <Chat {...safeChatProps} platform={message.platform} />
        </MessageContent>
      </MessageItem>
    );
  };

  return (
    <Container
      $chatFontSize={fontSize === 'small' ? '12px' : fontSize === 'large' ? '20px' : '16px'}
      $chatLineHeight={fontSize === 'small' ? '21px' : fontSize === 'large' ? '28px' : '24px'}
    >
      {/* Top selector */}
      <Header>
        <HeaderContent>
          <PlatformSelector onSelectionChange={handleSelectionChange} messageAggregator={messageAggregator} />
          <FontSizeGroup>
            <FontSizeLabel>{t('FONT_SIZE_LABEL')}</FontSizeLabel>
            <PopoverSelect
              ariaLabel={t('FONT_SIZE_LABEL')}
              options={fontSizeDefs}
              value={fontSize}
              onChange={setFontSize}
              getOptionValue={(o) => o.id}
              getOptionLabel={(o) => o.name}
              minWidth="72px"
              maxWidth="72px"
              width="72px"
            />
          </FontSizeGroup>
        </HeaderContent>
      </Header>

      {/* Message list */}
      <MessageList ref={listRef}>
        {filteredMessages.length === 0 ? (
          <EmptyState>
            <EmptyIcon src="/images/chat.svg" alt="" />
            <span>{t('EMPTY_CHAT_MESSAGE')}</span>
          </EmptyState>
        ) : (
          filteredMessages.map((m, i) => renderMessageItem(m, i))
        )}
        <div ref={endRef} />
      </MessageList>
    </Container>
  );
};

export default MultiPlatformChat;
