import { EventEmitter } from 'events';
import { OneSevenLivePlatform } from '../platforms/17live/core/OneSevenLivePlatform';
import { YouTubePlatform } from '../platforms/youtube/core/YouTubePlatform';
import { TwitchPlatform } from '../platforms/twitch/core/TwitchPlatform';
import { sendWSMessage } from './WSSender';

/**
 * Unified message aggregation manager
 * Manages multiple platform message streams with unified processing and dispatch
 */
export class MessageAggregator extends EventEmitter {
  constructor() {
    super();
    
    // Platform instance map
    this.platforms = new Map();
    
    // Active platform set
    this.activePlatforms = new Set();
    
    // Message queue (sorted by timestamp)
    this.messageQueue = [];
    
    // Message deduplication set (based on message ID)
    this.messageIds = new Set();
    
    // Max message queue length
    this.maxQueueSize = 1000;
    
    // Message processing interval (ms)
    this.processInterval = 100;
    
    // Timer reference
    this.processTimer = null;
    
    // Platform handler mapping
    this.platformHandlers = {
      '17live': OneSevenLivePlatform,
      'youtube': YouTubePlatform,
      'twitch': TwitchPlatform
    };
    
    this.initialize();
  }

  initialize() {
    // Start message processing timer
    this.startMessageProcessor();
    
    // Setup listeners for platform events
    this.setupEventListeners();
  }

  setupEventListeners() {
    // Listen for platform connection state changes
    this.on('platform_connected', this.handlePlatformConnected.bind(this));
    this.on('platform_disconnected', this.handlePlatformDisconnected.bind(this));
    this.on('platform_error', this.handlePlatformError.bind(this));
  }

  /**
   * Add platform
   */
  async addPlatform(platformId, config) {
    try {
      if (this.platforms.has(platformId)) {
        throw new Error(`Platform ${platformId} already exists`);
      }

      const PlatformClass = this.platformHandlers[platformId];
      if (!PlatformClass) {
        throw new Error(`Unsupported platform: ${platformId}`);
      }

      // Create platform instance
      const platform = new PlatformClass();
      
      // Listen for platform message events
      platform.on('message', (message) => {
        this.handlePlatformMessage(platformId, message);
      });

      platform.on('connected', (data) => {
        this.emit('platform_connected', { platformId, data });
      });

      platform.on('disconnected', (data) => {
        this.emit('platform_disconnected', { platformId, data });
      });

      platform.on('error', (error) => {
        this.emit('platform_error', { platformId, error });
      });

      // Add to platform map
      this.platforms.set(platformId, platform);

      // Auto-connect if config is provided
      if (config && Object.keys(config).length > 0) {
        await this.connectPlatform(platformId, config);
      }

      console.log(`Platform ${platformId} added`);
      
    } catch (error) {
      console.error(`Failed to add platform ${platformId}:`, error);
      throw error;
    }
  }

  /**
   * Remove platform
   */
  async removePlatform(platformId) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`Platform ${platformId} does not exist`);
      }

      // Disconnect
      if (platform.isConnected) {
        await platform.disconnect();
      }

      // Remove event listeners
      platform.removeAllListeners();

      // Delete from map
      this.platforms.delete(platformId);
      this.activePlatforms.delete(platformId);

      console.log(`Platform ${platformId} removed`);
      
    } catch (error) {
      console.error(`Failed to remove platform ${platformId}:`, error);
      throw error;
    }
  }

  /**
   * Connect platform
   */
  async connectPlatform(platformId, config) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`Platform ${platformId} does not exist`);
      }

      if (platform.isConnected) {
        console.log(`Platform ${platformId} already connected`);
        return;
      }

      await platform.connect(config);
      this.activePlatforms.add(platformId);
      
      console.log(`Platform ${platformId} connected`);
      
    } catch (error) {
      console.error(`Failed to connect platform ${platformId}:`, error);
      throw error;
    }
  }

  /**
   * Disconnect platform
   */
  async disconnectPlatform(platformId) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`Platform ${platformId} does not exist`);
      }

      if (!platform.isConnected) {
        console.log(`Platform ${platformId} already disconnected`);
        return;
      }

      await platform.disconnect();
      this.activePlatforms.delete(platformId);
      
      console.log(`Platform ${platformId} disconnected`);
      
    } catch (error) {
      console.error(`Failed to disconnect platform ${platformId}:`, error);
      throw error;
    }
  }

  /**
   * Handle platform message
   */
  handlePlatformMessage(platformId, message) {
    try {
      // Message deduplication
      if (this.messageIds.has(message.id)) {
        return;
      }

      // Add platform identifier
      const enrichedMessage = {
        ...message,
        platform: platformId,
        aggregatedAt: Date.now()
      };

      // Add to message queue
      this.messageQueue.push(enrichedMessage);
      this.messageIds.add(message.id);

      // Keep queue length
      if (this.messageQueue.length > this.maxQueueSize) {
        const removedMessage = this.messageQueue.shift();
        this.messageIds.delete(removedMessage.id);
      }

      // Sort by timestamp
      this.messageQueue.sort((a, b) => a.timestamp - b.timestamp);

      // Emit message event immediately
      this.emit('message', enrichedMessage);

      // Forward to WebSocket centrally (on demand)
      try {
        const content = enrichedMessage.content;
        const type = content && typeof content.get === 'function' ? content.get('messageType') : undefined;
        const gift = content && typeof content.get === 'function' ? content.get('gift') : undefined;

        if (gift) {
          const playData = {
            type: 'play_vff',
            vffURL: gift.get('vffURL'),
            vffJson: gift.get('vffJson'),
          };
          sendWSMessage({
            type,
            platform: platformId,
            payload: playData,
          });
        }
      } catch (e) {
        console.error('WS forwarding failed:', e);
      }
      
    } catch (error) {
      console.error('Failed to process platform message:', error);
      this.emit('error', { type: 'message_processing', error, platformId, message });
    }
  }

  /**
   * Handle platform connected event
   */
  handlePlatformConnected({ platformId, data }) {
    console.log(`Platform ${platformId} connected successfully:`, data);
    this.emit('status_change', { 
      platformId, 
      status: 'connected',
      timestamp: Date.now()
    });
  }

  /**
   * Handle platform disconnected event
   */
  handlePlatformDisconnected({ platformId, data }) {
    console.log(`Platform ${platformId} disconnected:`, data);
    this.activePlatforms.delete(platformId);
    this.emit('status_change', { 
      platformId, 
      status: 'disconnected',
      timestamp: Date.now()
    });
  }

  /**
   * Handle platform error event
   */
  handlePlatformError({ platformId, error }) {
    console.error(`Platform ${platformId} error:`, error);
    this.emit('error', { 
      type: 'platform_error',
      platformId, 
      error,
      timestamp: Date.now()
    });
  }

  /**
   * Start message processor
   */
  startMessageProcessor() {
    if (this.processTimer) {
      clearInterval(this.processTimer);
    }

    this.processTimer = setInterval(() => {
      this.processMessageQueue();
    }, this.processInterval);
  }

  /**
   * Process message queue
   */
  processMessageQueue() {
    if (this.messageQueue.length === 0) {
      return;
    }

    const now = Date.now();
    const messagesToProcess = [];

    // Get messages to process (based on timestamp)
    while (this.messageQueue.length > 0) {
      const message = this.messageQueue[0];
      
      // If message timestamp <= now, process it
      if (message.timestamp <= now) {
        messagesToProcess.push(this.messageQueue.shift());
      } else {
        break;
      }
    }

    // Emit batch of messages
    if (messagesToProcess.length > 0) {
      this.emit('messages_batch', messagesToProcess);
    }
  }

  /**
   * Get all platform statuses
   */
  getPlatformsStatus() {
    const status = {};
    
    this.platforms.forEach((platform, platformId) => {
      status[platformId] = {
        platformId,
        platformName: platform.platformName,
        isConnected: platform.isConnected,
        isActive: this.activePlatforms.has(platformId),
        queueSize: platform.messageQueue ? platform.messageQueue.length : 0
      };
    });

    return status;
  }

  /**
   * Get message statistics
   */
  getMessageStats() {
    const stats = {
      total: this.messageQueue.length,
      byPlatform: {},
      byType: {},
      timestamp: Date.now()
    };

    // Stats by platform
    this.messageQueue.forEach(message => {
      const platform = message.platform;
      const type = message.type;

      if (!stats.byPlatform[platform]) {
        stats.byPlatform[platform] = 0;
      }
      stats.byPlatform[platform]++;

      if (!stats.byType[type]) {
        stats.byType[type] = 0;
      }
      stats.byType[type]++;
    });

    return stats;
  }

  /**
   * Clear message queue
   */
  clearMessageQueue() {
    this.messageQueue = [];
    this.messageIds.clear();
    this.emit('queue_cleared', { timestamp: Date.now() });
  }

  /**
   * Send message to specified platform
   */
  async sendMessage(platformId, message) {
    try {
      const platform = this.platforms.get(platformId);
      if (!platform) {
        throw new Error(`Platform ${platformId} does not exist`);
      }

      if (!platform.isConnected) {
        throw new Error(`Platform ${platformId} not connected`);
      }

      await platform.sendMessage(message);
      
    } catch (error) {
      console.error(`Failed to send message to platform ${platformId}:`, error);
      throw error;
    }
  }

  /**
   * Get messages from all active platforms
   */
  getActiveMessages(limit = 100) {
    const activeMessages = this.messageQueue.filter(message => 
      this.activePlatforms.has(message.platform)
    );

    return activeMessages.slice(-limit);
  }

  /**
   * Stop aggregator
   */
  async stop() {
    try {
      // Stop message processor
      if (this.processTimer) {
        clearInterval(this.processTimer);
        this.processTimer = null;
      }

      // Disconnect all platforms
      const disconnectPromises = Array.from(this.platforms.keys()).map(platformId => 
        this.disconnectPlatform(platformId).catch(error => 
          console.error(`Failed to disconnect platform ${platformId}:`, error)
        )
      );

      await Promise.all(disconnectPromises);

      // Clear data
      this.clearMessageQueue();
      this.platforms.clear();
      this.activePlatforms.clear();

      // Remove all event listeners
      this.removeAllListeners();

      console.log('Message aggregator stopped');
      
    } catch (error) {
      console.error('Failed to stop message aggregator:', error);
      throw error;
    }
  }
}

// Create singleton instance
export const messageAggregator = new MessageAggregator();

export default MessageAggregator;
