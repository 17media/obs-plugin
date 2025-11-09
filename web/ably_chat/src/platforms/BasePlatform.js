/**
 * 平台处理器抽象基类
 * 定义所有平台必须实现的接口
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
   * 连接到平台
   * @abstract
   */
  async connect(config) {
    throw new Error('connect() must be implemented by subclass');
  }

  /**
   * 断开连接
   * @abstract
   */
  async disconnect() {
    throw new Error('disconnect() must be implemented by subclass');
  }

  /**
   * 处理原始消息数据
   * @abstract
   */
  processRawMessage(rawData) {
    throw new Error('processRawMessage() must be implemented by subclass');
  }

  /**
   * 发送消息到平台
   * @abstract
   */
  async sendMessage(message) {
    throw new Error('sendMessage() must be implemented by subclass');
  }

  /**
   * 获取平台状态
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
   * 清空消息队列
   */
  clearQueue() {
    this.messageQueue = [];
  }

  /**
   * 添加消息到队列
   */
  enqueueMessage(message) {
    if (this.messageQueue.length >= this.maxQueueSize) {
      this.messageQueue.shift(); // 移除最老的消息
    }
    this.messageQueue.push(message);
    this.emit('message', message);
  }

  /**
   * 获取平台图标路径
   */
  getPlatformIcon() {
    return `/images/${this.platformId}.svg`;
  }
}