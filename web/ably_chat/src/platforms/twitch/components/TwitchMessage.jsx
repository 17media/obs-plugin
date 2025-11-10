import React, { memo } from 'react';
import styled from 'styled-components';
import { Twitch } from 'lucide-react';

/**
 * Twitch message UI component
 * Converts the unified message format into Twitch-specific UI
 */

const MessageContainer = styled.div`
  padding: 0.5rem;
  border-radius: 0.25rem;
  transition: background-color 0.2s ease;
`;

const MessageRow = styled.div`
  display: flex;
  align-items: flex-start;
  gap: 0.5rem;
`;

const MessageContent = styled.div`
  flex: 1;
`;

const AuthorInfo = styled.div`
  display: flex;
  align-items: center;
  gap: 0.25rem;
  margin-bottom: 0.25rem;
`;

const Badge = styled.span`
  padding: 0.125rem 0.25rem;
  font-size: 0.75rem;
  color: white;
  border-radius: 0.125rem;
`;

const Username = styled.span`
  font-weight: 500;
  font-size: 0.875rem;
`;

const MessageText = styled.div`
  font-size: 0.875rem;
  color: ${props => props.dark ? '#e5e7eb' : '#1f2937'};
`;

const SubscriptionContainer = styled.div`
  display: flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.5rem;
  border-radius: 0.25rem;
  background-color: ${props => props.variant === 'subscription' ? (props.dark ? 'rgba(88, 28, 135, 0.2)' : '#faf5ff') : (props.dark ? 'rgba(88, 28, 135, 0.3)' : '#f3e8ff')};
`;

const SubscriptionContent = styled.div`
  flex: 1;
`;

const SubscriptionText = styled.div`
  font-size: 0.875rem;
  color: ${props => props.variant === 'subscription' ? (props.dark ? '#c084fc' : '#9333ea') : (props.dark ? '#d8b4fe' : '#7c3aed')};
`;

const SubscriptionMeta = styled.div`
  font-size: 0.75rem;
  color: ${props => props.variant === 'subscription' ? (props.dark ? '#a855f7' : '#a855f7') : (props.dark ? '#c084fc' : '#9333ea')};
`;

const SimpleRow = styled.div`
  display: flex;
  align-items: center;
  gap: 0.5rem;
  color: ${props => props.dark ? '#9ca3af' : '#6b7280'};
  font-size: ${props => props.small ? '0.75rem' : '0.875rem'};
`;

const SimpleIcon = styled(Twitch)`
  width: ${props => props.small ? '0.75rem' : '1rem'};
  height: ${props => props.small ? '0.75rem' : '1rem'};
  color: ${props => props.color === 'purple' ? '#a855f7' : props.color === 'blue' ? '#3b82f6' : '#a855f7'};
  flex-shrink: 0;
`;

export const TwitchMessage = memo(({ message }) => {
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
          badgeText = 'Broadcaster';
          badgeColor = 'bg-red-500';
          break;
        case 'moderator':
          badgeText = 'Moderator';
          badgeColor = 'bg-green-500';
          break;
        case 'vip':
          badgeText = 'VIP';
          badgeColor = 'bg-blue-500';
          break;
        case 'subscriber':
          badgeText = 'Subscriber';
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
    const isDark = document.documentElement.classList.contains('dark');
    
    switch (message.type) {
      case 'comment':
        return (
          <MessageRow>
            <SimpleIcon small color="purple" />
            <MessageContent>
              <AuthorInfo>
                {getAuthorBadges().map((badge, index) => (
                  <Badge 
                    key={index}
                    style={{ backgroundColor: badge.color }}
                  >
                    {badge.text}
                  </Badge>
                ))}
                <Username style={{ color: message.author?.color || '#9146FF' }}>
                  {message.author?.displayName || message.author?.name}
                </Username>
              </AuthorInfo>
              <MessageText dark={isDark}>
                {message.content}
              </MessageText>
            </MessageContent>
          </MessageRow>
        );
        
      case 'subscription':
        return (
          <SubscriptionContainer variant="subscription" dark={isDark}>
            <SimpleIcon color="purple" />
            <SubscriptionContent>
              <SubscriptionText variant="subscription" dark={isDark}>
                🎉 {message.content}
              </SubscriptionText>
            </SubscriptionContent>
          </SubscriptionContainer>
        );
        
      case 'resub':
        return (
          <SubscriptionContainer variant="resub" dark={isDark}>
            <SimpleIcon color="purple" />
            <SubscriptionContent>
              <SubscriptionText variant="resub" dark={isDark}>
                🎊 {message.content}
              </SubscriptionText>
              {message.metadata?.months && (
                <SubscriptionMeta variant="resub" dark={isDark}>
                  Subscription duration: {message.metadata.months} months
                </SubscriptionMeta>
              )}
            </SubscriptionContent>
          </SubscriptionContainer>
        );
        
      case 'cheer':
        return (
          <SubscriptionContainer variant="cheer" dark={isDark}>
            <SimpleIcon color="blue" />
            <SubscriptionContent>
              <SubscriptionText variant="cheer" dark={isDark}>
                💎 {message.content}
              </SubscriptionText>
              {message.metadata?.bits && (
                <SubscriptionMeta variant="cheer" dark={isDark}>
                  Bits: {message.metadata.bits}
                </SubscriptionMeta>
              )}
            </SubscriptionContent>
          </SubscriptionContainer>
        );
        
      case 'join':
        return (
          <SimpleRow dark={isDark} small>
            <SimpleIcon small />
            <span>{message.content}</span>
          </SimpleRow>
        );
        
      default:
        return (
          <SimpleRow>
            <SimpleIcon color="purple" />
            <span>{message.content}</span>
          </SimpleRow>
        );
    }
  };

  return (
    <MessageContainer className={`twitch-message ${getMessageStyle()}`}>
      {renderContent()}
    </MessageContainer>
  );
};

/**
 * Convert the unified message format to Chat component props
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
          name: 'Subscription',
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
          name: 'Resubscription',
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
          name: message.author?.displayName || message.author?.name || 'Twitch User'
        },
        message: message.content
      };
  }
};

export default TwitchMessage;