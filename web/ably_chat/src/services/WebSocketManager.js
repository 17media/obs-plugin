/**
 * Unified WebSocket connection manager
 * - Centralizes WebSocket lifecycle, reconnection, and messaging
 * - Re-emits events via EventEmitter for subscribers
 */
import { EventEmitter } from 'events';
import { getWebSocketServerURL } from '@/api';

class WebSocketManager extends EventEmitter {
  constructor() {
    super();
    this.ws = null;
    this.url = null;
    this.isClosing = false;
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 10;
    this.baseReconnectDelayMs = 1000;
    this.messageQueue = [];
  }

  /**
   * Connect (idempotent). Resolves URL from:
   * 1) explicit `url` arg
   * 2) `ws` query param
   * 3) server API `getWebSocketServerURL()`
   */
  async connect({ url } = {}) {
    // If already open/connecting, skip
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      return this.url;
    }

    this.isClosing = false;
    this.reconnectAttempts = 0;

    // Resolve URL
    let resolvedUrl = url;
    if (!resolvedUrl && typeof window !== 'undefined') {
      try {
        const params = new URLSearchParams(window.location.search);
        const wsParam = params.get('ws');
        if (wsParam && wsParam.trim()) {
          try {
            resolvedUrl = new URL(wsParam.trim(), window.location.origin).toString();
          } catch {
            resolvedUrl = wsParam.trim();
          }
        }
      } catch {
        // ignore
      }
    }
    if (!resolvedUrl) {
      resolvedUrl = await getWebSocketServerURL();
    }

    this.url = resolvedUrl;
    this._open();
    return this.url;
  }

  _open() {
    try {
      this.ws = new WebSocket(this.url);
    } catch (err) {
      this.emit('error', err);
      this._scheduleReconnect();
      return;
    }

    this.ws.onopen = () => {
      this.emit('open', { url: this.url });
      // Flush queued messages
      while (this.messageQueue.length && this.ws && this.ws.readyState === WebSocket.OPEN) {
        const msg = this.messageQueue.shift();
        try {
          this.ws.send(msg);
        } catch (e) {
          // If send fails, re-queue and break to avoid tight loop
          this.messageQueue.unshift(msg);
          break;
        }
      }
    };

    this.ws.onmessage = (event) => {
      let data = event.data;
      try {
        data = JSON.parse(event.data);
      } catch {
        // keep as text
      }
      this.emit('message', data);
    };

    this.ws.onerror = (err) => {
      this.emit('error', err);
    };

    this.ws.onclose = () => {
      this.emit('close', { url: this.url });
      if (!this.isClosing) {
        this._scheduleReconnect();
      }
    };
  }

  _scheduleReconnect() {
    if (this.isClosing) return;
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      this.emit('error', new Error('Max WebSocket reconnect attempts reached'));
      return;
    }
    const delay = this.baseReconnectDelayMs * Math.pow(2, this.reconnectAttempts);
    this.reconnectAttempts += 1;
    setTimeout(() => {
      this._open();
    }, Math.min(delay, 15000));
  }

  /** Send JSON-serializable payload or string */
  send(payload) {
    const data = typeof payload === 'string' ? payload : JSON.stringify(payload);
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      try {
        this.ws.send(data);
        return true;
      } catch (e) {
        this.emit('error', e);
        return false;
      }
    }
    // Queue while connecting/reconnecting
    this.messageQueue.push(data);
    return false;
  }

  /** Close connection and stop reconnection */
  close() {
    this.isClosing = true;
    if (this.ws) {
      try {
        this.ws.close();
      } catch {
        // ignore
      }
      this.ws = null;
    }
  }

  getStatus() {
    const state = this.ws ? this.ws.readyState : WebSocket.CLOSED;
    const map = {
      [WebSocket.CONNECTING]: 'connecting',
      [WebSocket.OPEN]: 'open',
      [WebSocket.CLOSING]: 'closing',
      [WebSocket.CLOSED]: 'closed',
    };
    return {
      url: this.url,
      state,
      status: map[state],
      reconnectAttempts: this.reconnectAttempts,
      queued: this.messageQueue.length,
    };
  }
}

export const wsManager = new WebSocketManager();
export default wsManager;