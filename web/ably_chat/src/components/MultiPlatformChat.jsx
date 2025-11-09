"use client";
import React, { useState, useEffect } from 'react';
import { messageAggregator } from '../services/MessageAggregator';
import { PlatformSelector } from './PlatformSelector';
import { convertToChatProps as convert17LiveChatProps } from '../platforms/17live/OneSevenLiveMessage';
import Chat from '@/lib/Chat';

/**
 * 多平台消息显示组件
 * 整合显示来自不同平台的消息
 */

export const MultiPlatformChat = () => {
  const [messages, setMessages] = useState([]); // 原始统一消息格式
  const [activePlatforms, setActivePlatforms] = useState(new Set(['17live', 'youtube', 'twitch']));

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

  // 处理平台选择变化（用于筛选显示的消息）
  const handlePlatformChange = (platformId, isEnabled) => {
    setActivePlatforms(prev => {
      const next = new Set(prev);
      if (isEnabled) next.add(platformId);
      else next.delete(platformId);
      return next;
    });
  };

  // 按选择的平台过滤展示
  const visibleMessages = messages.filter(m => activePlatforms.size === 0 || activePlatforms.has(m.platform));

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

  // 渲染单条消息
  const renderMessageItem = (message, index) => {
    if (message.platform === '17live') {
      const chatProps = convert17LiveChatProps(message, message.metadata?.streamerInfo);
      return (
        <div key={message.id || index} className="flex items-start space-x-2">
          <img src={platformIcon(message.platform)} alt={message.platform} className="w-5 h-5 rounded-full mt-1" />
          <div className="flex-1">
            <Chat {...chatProps} />
          </div>
        </div>
      );
    }

    // YouTube / Twitch 简单文本：username: message content
    const username = message?.author?.displayName || message?.author?.name || '用户';
    const content = message?.content || '';
    return (
      <div key={message.id || index} className="flex items-start space-x-2">
        <img src={platformIcon(message.platform)} alt={message.platform} className="w-5 h-5 rounded-full mt-1" />
        <div className="text-sm"><span className="font-semibold mr-2">{username}:</span>{content}</div>
      </div>
    );
  };

  return (
    <div className="min-h-screen bg-black text-gray-100">
      {/* 顶部选择器 */}
      <div className="p-4 border-b border-gray-800">
        <div className="max-w-md">
          <PlatformSelector onPlatformChange={handlePlatformChange} messageAggregator={messageAggregator} />
        </div>
      </div>

      {/* 消息列表 */}
      <div className="h-[calc(100vh-72px)] overflow-y-auto p-4 space-y-2">
        {visibleMessages.length === 0 ? (
          <div className="flex items-center justify-center h-full text-gray-400">暂无消息</div>
        ) : (
          visibleMessages.map((m, i) => renderMessageItem(m, i))
        )}
      </div>
    </div>
  );
};

export default MultiPlatformChat;