import { EventEmitter } from 'events';
import { fromJS } from 'immutable';
import { OneSevenLivePlatform } from '../platforms/17live/core/OneSevenLivePlatform';
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

    // Local storage config
    this.storageKey = 'obs17live_chat_messages_v1';
    this.maxStored = 1000;
    this.history = [];
    this.currentRoomId = null;
    
    // Message processing interval (ms)
    this.processInterval = 100;
    
    // Timer reference
    this.processTimer = null;
    
    // Platform handler mapping
    this.platformHandlers = {
      '17live': OneSevenLivePlatform,
      'twitch': TwitchPlatform
    };
    
    this.initialize();
  }

  initialize() {
    this.startMessageProcessor();
    this.setupEventListeners();
    this.currentRoomId = this.getCurrentRoomId();
    try { console.log('[MsgAgg] init storageKey', this.storageKey, 'roomId', this.currentRoomId); } catch {}
    this.loadFromStorage();
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

      // Emit message event immediately (single message stream)
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

    // Emit batch of messages and persist history
    if (messagesToProcess.length > 0) {
      // Append to history (dedup by id)
      for (const m of messagesToProcess) {
        const idx = this.history.findIndex((x) => x.id === m.id);
        if (idx === -1) {
          this.history.push(m);
        } else {
          this.history[idx] = m;
        }
      }
      if (this.history.length > this.maxStored) {
        this.history = this.history.slice(-this.maxStored);
      }
      // Persist
      this.saveToStorage();
      // Notify UI
      this.emit('messages_batch', messagesToProcess);
    }
  }

  /**
   * Get current roomID from URL
   */
  getCurrentRoomId() {
    try {
      if (typeof window === 'undefined') return null;
      const params = new URLSearchParams(window.location.search);
      const roomID = params.get('roomID');
      return roomID || null;
    } catch {
      return null;
    }
  }

  /**
   * Load messages from local storage
   */
  loadFromStorage() {
    try {
      if (typeof window === 'undefined' || !window.localStorage) return;
      const raw = window.localStorage.getItem(this.storageKey);
      if (!raw) {
        try { console.log('[MsgAgg] no storage for key', this.storageKey); } catch {}
        return;
      }
      try { console.log('[MsgAgg] load raw length', raw.length); } catch {}
      const parsed = JSON.parse(raw);
      const restored = [];
      if (Array.isArray(parsed)) {
        try { console.log('[MsgAgg] parsed array size', parsed.length); } catch {}
        for (const item of parsed) {
          const id = item && item.id;
          if (!id || this.messageIds.has(id)) continue;
          const content = item && item.content;
          const unified = {
            id,
            platform: item.platform,
            timestamp: item.timestamp || Date.now(),
            content: content && typeof content === 'object' ? fromJS(content) : content,
            aggregatedAt: Date.now(),
          };
          this.messageIds.add(id);
          restored.push(unified);
        }
      } else if (parsed && typeof parsed === 'object' && Array.isArray(parsed.chats)) {
        // Require matching roomId when provided
        const bundleRoomId = parsed.roomId || null;
        try { console.log('[MsgAgg] parsed bundle roomId', bundleRoomId, 'current', this.currentRoomId, 'chats', parsed.chats.length); } catch {}
        if (bundleRoomId && this.currentRoomId && bundleRoomId !== this.currentRoomId) {
          try { console.log('[MsgAgg] skip bundle due to roomId mismatch'); } catch {}
          return;
        }
        for (const chat of parsed.chats) {
          const id = chat && chat.id;
          if (!id || this.messageIds.has(id)) continue;
          const ts = (chat && (chat.sendTime || chat.timestamp)) || (parsed.timestamp || Date.now());
          const unified = {
            id,
            platform: chat.platform || '17live',
            timestamp: ts,
            content: fromJS(chat),
            aggregatedAt: Date.now(),
          };
          this.messageIds.add(id);
          restored.push(unified);
        }
      } else {
        try { console.log('[MsgAgg] parsed unknown format'); } catch {}
        return;
      }
      this.history = restored.slice(-this.maxStored);
      this.history.sort((a, b) => a.timestamp - b.timestamp);
      try { console.log('[MsgAgg] restored count', this.history.length); } catch {}
      if (this.history.length) {
        this.emit('messages_batch', this.history);
        try { console.log('[MsgAgg] emitted batch', this.history.length); } catch {}
      }
    } catch (e) {
      console.error('Failed to load messages from storage:', e);
    }
  }

  /**
   * Save tail messages to local storage
   */
  saveToStorage() {
    try {
      if (typeof window === 'undefined' || !window.localStorage) return;
      let tail = this.history.slice(-this.maxStored);
      // Ensure chronological order
      tail = tail.sort((a, b) => a.timestamp - b.timestamp);
      // Persist all platforms' chats merged under the same roomId (assumed same user)
      const chats = tail
        .filter((m) => m && m.content)
        .map((m) => {
          const content = typeof m.content.get === 'function' ? m.content.toJS() : m.content;
          return {
            ...content,
            platform: m.platform
          };
        });
      const bundle = {
        roomId: this.currentRoomId || '',
        timestamp: Date.now(),
        chats,
      };
      window.localStorage.setItem(this.storageKey, JSON.stringify(bundle));
    } catch (e) {
      console.error('Failed to save messages to storage:', e);
    }
  }

  /**
   * Get persisted history (tail up to limit)
   */
  getHistory(limit = 1000) {
    const n = Math.max(0, Math.min(limit, this.history.length));
    return this.history.slice(this.history.length - n);
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
