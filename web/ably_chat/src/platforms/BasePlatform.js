/**
 * Platform handler abstract base class
 * Defines interfaces all platforms must implement
 */

import { EventEmitter } from 'events';

export class BasePlatform extends EventEmitter {
  constructor(platformId, platformName) {
    super();
    this.platformId = platformId;
    this.platformName = platformName;
    this.isConnected = false;
    this.messageQueue = [];
    this.maxQueueSize = 1000;
  }

  /**
   * Connect to the platform
   * @abstract
   */
  async connect(config) {
    throw new Error('connect() must be implemented by subclass');
  }

  /**
   * Disconnect
   * @abstract
   */
  async disconnect() {
    throw new Error('disconnect() must be implemented by subclass');
  }

  /**
   * Process raw message data
   * @abstract
   */
  processRawMessage(rawData) {
    throw new Error('processRawMessage() must be implemented by subclass');
  }

  /**
   * Send a message to the platform
   * @abstract
   */
  async sendMessage(message) {
    throw new Error('sendMessage() must be implemented by subclass');
  }

  /**
   * Get platform status
   */
  getStatus() {
    return {
      platformId: this.platformId,
      platformName: this.platformName,
      isConnected: this.isConnected,
      queueSize: this.messageQueue.length
    };
  }

  /**
   * Clear message queue
   */
  clearQueue() {
    this.messageQueue = [];
  }

  /**
   * Add message to queue
   */
  enqueueMessage(message) {
    if (this.messageQueue.length >= this.maxQueueSize) {
      this.messageQueue.shift(); // Remove the oldest message
    }
    this.messageQueue.push(message);
    this.emit('message', message);
  }

  /**
   * Get platform icon path
   */
  getPlatformIcon() {
    return `/images/${this.platformId}.svg`;
  }
}