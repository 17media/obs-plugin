import React from 'react';
import { Twitch } from 'lucide-react';

/**
 * Twitch消息UI组件
 * 将统一消息格式转换为Twitch特定的UI显示
 */

export const TwitchMessage = ({ message }) => {
  const getMessageStyle = () => {
    switch (message.type) {
      case 'comment':
        return 'twitch-chat-message';
      case 'subscription':
        return 'twitch-subscription-message';
      case 'resub':
        return 'twitch-resub-message';
      case 'cheer':
        return 'twitch-cheer-message';
      case 'join':
        return 'twitch-join-message';
      default:
        return 'twitch-default-message';
    }
  };

  const getAuthorBadges = () => {
    if (!message.author?.badges) return [];
    
    return message.author.badges.map(badge => {
      let badgeText = '';
      let badgeColor = '';
      
      switch (badge.name) {
        case 'broadcaster':
          badgeText = '主播';
          badgeColor = 'bg-red-500';
          break;
        case 'moderator':
          badgeText = '房管';
          badgeColor = 'bg-green-500';
          break;
        case 'vip':
          badgeText = 'VIP';
          badgeColor = 'bg-blue-500';
          break;
        case 'subscriber':
          badgeText = '订阅';
          badgeColor = 'bg-purple-500';
          break;
        case 'premium':
          badgeText = 'Prime';
          badgeColor = 'bg-blue-400';
          break;
        case 'turbo':
          badgeText = 'Turbo';
          badgeColor = 'bg-purple-400';
          break;
        default:
          badgeText = badge.name;
          badgeColor = 'bg-gray-500';
      }
      
      return {
        text: badgeText,
        color: badgeColor
      };
    });
  };

  const renderContent = () => {
    switch (message.type) {
      case 'comment':
        return (
          <div className="flex items-start space-x-2">
            <Twitch className="w-4 h-4 text-purple-500 mt-1 flex-shrink-0" />
            <div className="flex-1">
              <div className="flex items-center space-x-1 mb-1">
                {getAuthorBadges().map((badge, index) => (
                  <span 
                    key={index}
                    className={`px-1 py-0.5 text-xs text-white rounded ${badge.color}`}
                  >
                    {badge.text}
                  </span>
                ))}
                <span 
                  className="font-medium text-sm"
                  style={{ color: message.author?.color || '#9146FF' }}
                >
                  {message.author?.displayName || message.author?.name}
                </span>
              </div>
              <div className="text-sm text-gray-800 dark:text-gray-200">
                {message.content}
              </div>
            </div>
          </div>
        );
        
      case 'subscription':
        return (
          <div className="flex items-center space-x-2 bg-purple-50 dark:bg-purple-900/20 p-2 rounded">
            <Twitch className="w-4 h-4 text-purple-500" />
            <div className="flex-1">
              <div className="text-sm text-purple-600 dark:text-purple-400">
                🎉 {message.content}
              </div>
            </div>
          </div>
        );
        
      case 'resub':
        return (
          <div className="flex items-center space-x-2 bg-purple-100 dark:bg-purple-900/30 p-2 rounded">
            <Twitch className="w-4 h-4 text-purple-600" />
            <div className="flex-1">
              <div className="text-sm text-purple-700 dark:text-purple-300">
                🎊 {message.content}
              </div>
              {message.metadata?.months && (
                <div className="text-xs text-purple-500 dark:text-purple-400">
                  订阅时长: {message.metadata.months} 个月
                </div>
              )}
            </div>
          </div>
        );
        
      case 'cheer':
        return (
          <div className="flex items-center space-x-2 bg-blue-50 dark:bg-blue-900/20 p-2 rounded">
            <Twitch className="w-4 h-4 text-blue-500" />
            <div className="flex-1">
              <div className="text-sm text-blue-600 dark:text-blue-400">
                💎 {message.content}
              </div>
              {message.metadata?.bits && (
                <div className="text-xs text-blue-500 dark:text-blue-400">
                  Bits: {message.metadata.bits}
                </div>
              )}
            </div>
          </div>
        );
        
      case 'join':
        return (
          <div className="flex items-center space-x-2 text-gray-500 dark:text-gray-400 text-xs">
            <Twitch className="w-3 h-3" />
            <span>{message.content}</span>
          </div>
        );
        
      default:
        return (
          <div className="flex items-center space-x-2">
            <Twitch className="w-4 h-4 text-purple-500" />
            <span className="text-sm">{message.content}</span>
          </div>
        );
    }
  };

  return (
    <div className={`twitch-message ${getMessageStyle()} p-2 rounded transition-colors`}>
      {renderContent()}
    </div>
  );
};

/**
 * 将统一消息格式转换为Chat组件需要的props格式
 */
export const convertToChatProps = (message) => {
  const baseProps = {
    key: message.id,
    type: message.type,
    platform: 'twitch',
    timestamp: message.timestamp,
    content: message.content
  };

  switch (message.type) {
    case 'comment':
      return {
        ...baseProps,
        type: 'comment',
        user: {
          name: message.author?.displayName || message.author?.name,
          avatar: message.author?.avatar,
          color: message.author?.color || '#9146FF',
          badges: message.author?.badges || []
        },
        message: message.content,
        emotes: message.metadata?.emotes
      };
      
    case 'subscription':
      return {
        ...baseProps,
        type: 'gift',
        user: {
          name: message.author?.displayName || message.author?.name
        },
        gift: {
          name: '订阅',
          count: 1
        },
        message: message.content
      };
      
    case 'resub':
      return {
        ...baseProps,
        type: 'gift',
        user: {
          name: message.author?.displayName || message.author?.name
        },
        gift: {
          name: '重新订阅',
          count: message.metadata?.months || 1
        },
        message: message.content
      };
      
    case 'cheer':
      return {
        ...baseProps,
        type: 'gift',
        user: {
          name: message.author?.displayName || message.author?.name
        },
        gift: {
          name: 'Bits',
          count: message.metadata?.bits || 0
        },
        message: message.content
      };
      
    case 'join':
      return {
        ...baseProps,
        type: 'join',
        user: {
          name: message.author?.name
        },
        message: message.content
      };
      
    default:
      return {
        ...baseProps,
        type: 'comment',
        user: {
          name: message.author?.displayName || message.author?.name || 'Twitch用户'
        },
        message: message.content
      };
  }
};

export default TwitchMessage;