"use client";
import React, { useState, useEffect } from 'react';
import { useTranslations } from 'next-intl';
import styled from 'styled-components';
import { messageAggregator } from '../services/MessageAggregator';
import { PlatformSelector } from './PlatformSelector';
import Chat from '@/lib/Chat';
import { getChatProps } from '@/platforms/17live/util/getChatProps';
import { fromJS } from 'immutable';

/**
 * 多平台消息显示组件
 * 整合显示来自不同平台的消息
 */

const Container = styled.div`
  min-height: 100vh;
  background-color: #000000;
  color: #f3f4f6;
`;

const Header = styled.div`
  padding: 1rem;
  border-bottom: 1px solid #1f2937;
`;

const HeaderContent = styled.div`
  max-width: 28rem;
`;

const MessageList = styled.div`
  height: calc(100vh - 72px);
  overflow-y: auto;
  padding: 1rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  /* 移除内部列表的默认圆点与内边距 */
  & ul,
  & ol {
    list-style: none;
    margin: 0;
    padding-left: 0;
  }
`;

const EmptyState = styled.div`
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 40px 20px;
  color: #A1A9B6;
  font-size: 14px;
`;

const EmptyIcon = styled.img`
  width: 20px;
  height: 20px;
  margin-right: 8px;
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

const SimpleMessage = styled.div`
  font-size: 0.875rem;
`;

const Username = styled.span`
  font-weight: 600;
  margin-right: 0.5rem;
`;

export const MultiPlatformChat = () => {
  const [messages, setMessages] = useState([]); // 原始统一消息格式
  const [selectedPlatform, setSelectedPlatform] = useState('all');
  const [filteredMessages, setFilteredMessages] = useState([]);
  const t = useTranslations('ChatPage');

  // 监听消息聚合器事件（直接使用统一消息格式）
  useEffect(() => {
    if (!messageAggregator) return;

    const handleMessage = (message) => {
      setMessages(prev => {
        const next = [...prev, message];
        return next.length > 1000 ? next.slice(-1000) : next;
      });
    };

    const handleMessagesBatch = (batch) => {
      if (!batch || batch.length === 0) return;
      setMessages(prev => {
        const next = [...prev, ...batch];
        return next.length > 1000 ? next.slice(-1000) : next;
      });
    };

    messageAggregator.on('message', handleMessage);
    messageAggregator.on('messages_batch', handleMessagesBatch);

    return () => {
      messageAggregator.off('message', handleMessage);
      messageAggregator.off('messages_batch', handleMessagesBatch);
    };
  }, []);

  const handleSelectionChange = (value) => {
    setSelectedPlatform(value);
  };

  // 依据选择的平台过滤显示
  useEffect(() => {
    const next = messages.filter(m => selectedPlatform === 'all' || m.platform === selectedPlatform);
    setFilteredMessages(next);
  }, [messages, selectedPlatform]);

  // 平台图标映射
  const platformIcon = (platform) => {
    switch (platform) {
      case '17live':
        return '/images/17live.svg';
      case 'youtube':
        return '/images/youtube.svg';
      case 'twitch':
        return '/images/twitch.svg';
      default:
        return '/images/17live.svg';
    }
  };

  // 平台消息统一：直接使用 content 作为 Immutable 对象

  // 渲染单条消息（统一用 Chat + 平台图标）
  const renderMessageItem = (message, index) => {
    const immutableChat = message.content; // 平台已生成 Immutable 内容
    const chatProps = getChatProps(immutableChat);
    return (
      <MessageItem key={message.id || index}>
        <PlatformIcon
          src={platformIcon(message.platform)}
          alt={message.platform}
        />
        <MessageContent>
          <Chat {...chatProps} />
        </MessageContent>
      </MessageItem>
    );
  };

  return (
    <Container>
      {/* 顶部选择器 */}
      <Header>
        <HeaderContent>
          <PlatformSelector onSelectionChange={handleSelectionChange} messageAggregator={messageAggregator} />
        </HeaderContent>
      </Header>

      {/* 消息列表 */}
      <MessageList>
        {filteredMessages.length === 0 ? (
          <EmptyState>
            <EmptyIcon src="/images/exclaimark.svg" alt="" />
            <span>{t('EMPTY_CHAT_MESSAGE')}</span>
          </EmptyState>
        ) : (
          filteredMessages.map((m, i) => renderMessageItem(m, i))
        )}
      </MessageList>
    </Container>
  );
};

export default MultiPlatformChat;