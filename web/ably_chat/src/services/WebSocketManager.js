/**
 * Unified WebSocket connection manager
 * - Centralizes WebSocket lifecycle, reconnection, and messaging
 * - Re-emits events via EventEmitter for subscribers
 */
import { EventEmitter } from 'events';

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
   * Connect (idempotent) ONLY when `ws` query param exists.
   * If the param is missing, no connection attempt is made.
   */
  async connect() {
    // If already open/connecting, skip
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      return this.url;
    }

    this.isClosing = false;
    this.reconnectAttempts = 0;

    // Resolve URL from `ws` query param only
    if (typeof window === 'undefined') {
      this.url = null;
      return null;
    }

    let resolvedUrl = null;
    try {
      const params = new URLSearchParams(window.location.search);
      const raw = params.get('ws');
      if (!raw || !raw.trim()) {
        this.url = null;
        return null;
      }
      try {
        resolvedUrl = new URL(raw.trim(), window.location.origin).toString();
      } catch {
        resolvedUrl = raw.trim();
      }
    } catch {
      this.url = null;
      return null;
    }

    this.url = resolvedUrl;
    this._open();
    return this.url;
  }

  /** Whether a WS URL has been configured (via query param) */
  hasConfiguredURL() {
    return !!this.url;
  }

  _open() {
    // Only attempt open if a URL is configured
    if (!this.url || this.isClosing) {
      return;
    }
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
    if (this.isClosing || !this.url) return;
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
    // Only queue while an actual connection lifecycle is in progress
    if (this.ws && (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.CLOSING)) {
      this.messageQueue.push(data);
    }
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
      configured: !!this.url,
      state,
      status: map[state],
      reconnectAttempts: this.reconnectAttempts,
      queued: this.messageQueue.length,
    };
  }
}

export const wsManager = new WebSocketManager();
export default wsManager;