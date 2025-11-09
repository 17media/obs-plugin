"use client";
import React, { useState, useEffect } from 'react';
import { messageAggregator } from '../services/MessageAggregator';
import { PlatformSelector } from './PlatformSelector';
import { convertToChatProps as convert17LiveChatProps } from '../platforms/17live/OneSevenLiveMessage';
import { convertToChatProps as convertYouTubeChatProps } from '../platforms/youtube/YouTubeMessage';
import { convertToChatProps as convertTwitchChatProps } from '../platforms/twitch/TwitchMessage';
import Chat from '@/lib/Chat';
import { Settings, MessageSquare, Users, Filter } from 'lucide-react';

/**
 * 多平台消息显示组件
 * 整合显示来自不同平台的消息
 */

export const MultiPlatformChat = ({ roomId, userId }) => {
  const [messages, setMessages] = useState([]);
  const [activePlatforms, setActivePlatforms] = useState(new Set());
  const [showPlatformSelector, setShowPlatformSelector] = useState(false);
  const [filter, setFilter] = useState({
    platforms: new Set(['17live', 'youtube', 'twitch']),
    messageTypes: new Set(['comment', 'gift', 'join', 'subscription', 'cheer'])
  });
  const [showFilter, setShowFilter] = useState(false);
  const [stats, setStats] = useState({
    total: 0,
    byPlatform: {},
    byType: {}
  });

  // 平台转换函数映射
  const platformConverters = {
    '17live': convert17LiveChatProps,
    'youtube': convertYouTubeChatProps,
    'twitch': convertTwitchChatProps
  };

  // 监听消息聚合器事件
  useEffect(() => {
    if (!messageAggregator) return;

    const handleMessage = (message) => {
      // 转换消息格式
      const converter = platformConverters[message.platform];
      if (converter) {
        const chatProps = converter(message);
        if (chatProps) {
          setMessages(prev => {
            const newMessages = [...prev, chatProps];
            // 保持消息数量限制
            if (newMessages.length > 500) {
              return newMessages.slice(-500);
            }
            return newMessages;
          });
        }
      }
    };

    const handleMessagesBatch = (messages) => {
      const convertedMessages = messages
        .map(message => {
          const converter = platformConverters[message.platform];
          return converter ? converter(message) : null;
        })
        .filter(Boolean);

      if (convertedMessages.length > 0) {
        setMessages(prev => {
          const newMessages = [...prev, ...convertedMessages];
          if (newMessages.length > 500) {
            return newMessages.slice(-500);
          }
          return newMessages;
        });
      }
    };

    const handleStatsUpdate = () => {
      const messageStats = messageAggregator.getMessageStats();
      setStats(messageStats);
    };

    messageAggregator.on('message', handleMessage);
    messageAggregator.on('messages_batch', handleMessagesBatch);
    messageAggregator.on('message', handleStatsUpdate);

    return () => {
      messageAggregator.off('message', handleMessage);
      messageAggregator.off('messages_batch', handleMessagesBatch);
      messageAggregator.off('message', handleStatsUpdate);
    };
  }, []);

  // 处理平台状态变化
  const handlePlatformChange = (platformId, isEnabled) => {
    setActivePlatforms(prev => {
      const newSet = new Set(prev);
      if (isEnabled) {
        newSet.add(platformId);
      } else {
        newSet.delete(platformId);
      }
      return newSet;
    });
  };

  // 过滤消息
  const filteredMessages = messages.filter(message => {
    // 平台过滤
    if (!filter.platforms.has(message.platform)) {
      return false;
    }
    
    // 消息类型过滤
    if (!filter.messageTypes.has(message.type)) {
      return false;
    }
    
    return true;
  });

  // 清空消息
  const clearMessages = () => {
    setMessages([]);
    messageAggregator.clearMessageQueue();
  };

  // 切换过滤器
  const toggleFilter = (filterType, value) => {
    setFilter(prev => ({
      ...prev,
      [filterType]: new Set(
        prev[filterType].has(value)
          ? [...prev[filterType]].filter(item => item !== value)
          : [...prev[filterType], value]
      )
    }));
  };

  return (
    <div className="multi-platform-chat bg-gray-50 dark:bg-gray-900 min-h-screen">
      {/* 头部控制栏 */}
      <div className="bg-white dark:bg-gray-800 shadow-sm border-b border-gray-200 dark:border-gray-700">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between h-16">
            <div className="flex items-center space-x-4">
              <h1 className="text-xl font-semibold text-gray-900 dark:text-white">
                多平台聊天
              </h1>
              <div className="flex items-center space-x-2 text-sm text-gray-500 dark:text-gray-400">
                <MessageSquare className="w-4 h-4" />
                <span>{filteredMessages.length}</span>
                <Users className="w-4 h-4 ml-2" />
                <span>{activePlatforms.size} 平台</span>
              </div>
            </div>

            <div className="flex items-center space-x-2">
              {/* 过滤器按钮 */}
              <button
                onClick={() => setShowFilter(!showFilter)}
                className={`flex items-center space-x-2 px-3 py-2 rounded-md text-sm font-medium transition-colors ${
                  showFilter
                    ? 'bg-blue-100 text-blue-700 dark:bg-blue-900 dark:text-blue-300'
                    : 'bg-gray-100 text-gray-700 hover:bg-gray-200 dark:bg-gray-700 dark:text-gray-300 dark:hover:bg-gray-600'
                }`}
              >
                <Filter className="w-4 h-4" />
                <span>过滤</span>
              </button>

              {/* 平台管理按钮 */}
              <button
                onClick={() => setShowPlatformSelector(!showPlatformSelector)}
                className={`flex items-center space-x-2 px-3 py-2 rounded-md text-sm font-medium transition-colors ${
                  showPlatformSelector
                    ? 'bg-blue-100 text-blue-700 dark:bg-blue-900 dark:text-blue-300'
                    : 'bg-gray-100 text-gray-700 hover:bg-gray-200 dark:bg-gray-700 dark:text-gray-300 dark:hover:bg-gray-600'
                }`}
              >
                <Settings className="w-4 h-4" />
                <span>平台管理</span>
              </button>

              {/* 清空消息按钮 */}
              <button
                onClick={clearMessages}
                className="px-3 py-2 bg-red-100 text-red-700 hover:bg-red-200 dark:bg-red-900 dark:text-red-300 dark:hover:bg-red-800 rounded-md text-sm font-medium transition-colors"
              >
                清空
              </button>
            </div>
          </div>
        </div>
      </div>

      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6">
        <div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
          {/* 左侧控制面板 */}
          <div className="lg:col-span-1 space-y-6">
            {/* 平台管理 */}
            {showPlatformSelector && (
              <PlatformSelector
                onPlatformChange={handlePlatformChange}
                messageAggregator={messageAggregator}
              />
            )}

            {/* 过滤器 */}
            {showFilter && (
              <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-4">
                <h3 className="font-medium text-gray-800 dark:text-gray-200 mb-4">
                  消息过滤
                </h3>
                
                {/* 平台过滤 */}
                <div className="mb-4">
                  <h4 className="text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                    平台
                  </h4>
                  <div className="space-y-2">
                    {['17live', 'youtube', 'twitch'].map(platform => (
                      <label key={platform} className="flex items-center">
                        <input
                          type="checkbox"
                          checked={filter.platforms.has(platform)}
                          onChange={() => toggleFilter('platforms', platform)}
                          className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                        />
                        <span className="ml-2 text-sm text-gray-600 dark:text-gray-400 capitalize">
                          {platform}
                        </span>
                      </label>
                    ))}
                  </div>
                </div>

                {/* 消息类型过滤 */}
                <div>
                  <h4 className="text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                    消息类型
                  </h4>
                  <div className="space-y-2">
                    {[
                      { key: 'comment', label: '聊天消息' },
                      { key: 'gift', label: '礼物/订阅' },
                      { key: 'join', label: '加入消息' },
                      { key: 'subscription', label: '订阅消息' },
                      { key: 'cheer', label: '欢呼消息' }
                    ].map(type => (
                      <label key={type.key} className="flex items-center">
                        <input
                          type="checkbox"
                          checked={filter.messageTypes.has(type.key)}
                          onChange={() => toggleFilter('messageTypes', type.key)}
                          className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                        />
                        <span className="ml-2 text-sm text-gray-600 dark:text-gray-400">
                          {type.label}
                        </span>
                      </label>
                    ))}
                  </div>
                </div>
              </div>
            )}

            {/* 统计信息 */}
            <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-4">
              <h3 className="font-medium text-gray-800 dark:text-gray-200 mb-4">
                统计信息
              </h3>
              <div className="space-y-3">
                <div className="flex justify-between">
                  <span className="text-sm text-gray-600 dark:text-gray-400">总消息数</span>
                  <span className="text-sm font-medium">{stats.total}</span>
                </div>
                {Object.entries(stats.byPlatform).map(([platform, count]) => (
                  <div key={platform} className="flex justify-between">
                    <span className="text-sm text-gray-600 dark:text-gray-400 capitalize">{platform}</span>
                    <span className="text-sm font-medium">{count}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>

          {/* 右侧消息显示区 */}
          <div className="lg:col-span-3">
            <div className="bg-white dark:bg-gray-800 rounded-lg shadow h-[calc(100vh-12rem)] overflow-hidden">
              <div className="h-full overflow-y-auto p-4 space-y-3">
                {filteredMessages.length === 0 ? (
                  <div className="flex items-center justify-center h-full text-gray-500 dark:text-gray-400">
                    <div className="text-center">
                      <MessageSquare className="w-12 h-12 mx-auto mb-4 opacity-50" />
                      <p>暂无消息</p>
                      <p className="text-sm mt-2">
                        {activePlatforms.size === 0
                          ? '请先连接至少一个平台'
                          : '等待消息中...'
                        }
                      </p>
                    </div>
                  </div>
                ) : (
                  filteredMessages.map((message, index) => (
                    <div key={message.key || index} className="message-item">
                      <Chat {...message} />
                    </div>
                  ))
                )}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default MultiPlatformChat;