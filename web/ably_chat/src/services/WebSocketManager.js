/**
 * Unified WebSocket connection manager
 * - Centralizes WebSocket lifecycle, reconnection, and messaging
 * - Re-emits events via EventEmitter for subscribers
 */
import { EventEmitter } from 'events';
import { messageAggregator } from './MessageAggregator';

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
      try {
        this.send({ type: 'action', payload: { type: 'register_chatdock' } });
      } catch {}
    };

    this.ws.onmessage = (event) => {
      const raw = event.data;
      let msg = null;
      try {
        msg = JSON.parse(raw);
      } catch {
        // 非 JSON 消息忽略（仅处理统一协议）
        return;
      }

      const type = msg?.type;
      const payload = msg?.payload;
      if (!type) return;

      // 路由到平台处理：twitch / youtube / 17live
      const routeTo = (platformId, transform) => {
        const platform = messageAggregator.platforms?.get(platformId);
        if (platform && typeof platform.processRawMessage === 'function') {
          const rawPayload = typeof transform === 'function' ? transform(payload) : payload;
          const unified = platform.processRawMessage(rawPayload);
          if (unified) platform.enqueueMessage(unified);
        }
      };

      const parseTwitchPrivmsg = (rawStr) => {
        if (!rawStr || typeof rawStr !== 'string') return null;
        const s = rawStr.replace(/\r\n?$/, '');
        const idxPriv = s.indexOf('PRIVMSG ');
        if (idxPriv < 0) return null;
        const idxHash = s.indexOf('#', idxPriv);
        if (idxHash < 0) return null;
        const idxSpaceAfterChan = s.indexOf(' ', idxHash);
        if (idxSpaceAfterChan < 0) return null;
        const idxMsg = s.indexOf(' :', idxSpaceAfterChan);
        if (idxMsg < 0) return null;
        const channel = s.substring(idxHash + 1, idxSpaceAfterChan);
        const message = s.substring(idxMsg + 2).trim();
        let username = '';
        const u = s.match(/:([^!\s]+)!/);
        if (u) username = u[1];
        return { type: 'chat', channel, username, tags: { 'display-name': username }, message };
      };

      if (type === 'twitch_chat_connected' || type === 'twitch_chat_message') {
        const ensure = () => {
          const platform = messageAggregator.platforms?.get('twitch');
          if (!platform) return messageAggregator.addPlatform('twitch', {}).then(() => messageAggregator.platforms.get('twitch'));
          return Promise.resolve(platform);
        };
        ensure()
          .then((platform) => {
            if (!platform) return;
            if (type === 'twitch_chat_connected') {
              if (typeof platform.handleWsMessage === 'function') {
                platform.handleWsMessage({ type, payload });
              }
            } else if (type === 'twitch_chat_message') {
              const parsed = parseTwitchPrivmsg(payload?.raw);
              if (parsed) {
                routeTo('twitch', () => parsed);
              }
            }
          })
          .catch(() => {});
      } else if (type === 'ably_chat_connected' || type === 'ably_chat_message') {
        const ensure = () => {
          const platform = messageAggregator.platforms?.get('17live');
          if (!platform) return messageAggregator.addPlatform('17live', {}).then(() => messageAggregator.platforms.get('17live'));
          return Promise.resolve(platform);
        };
        ensure()
          .then((platform) => {
            if (!platform) return;
            if (typeof platform.handleWsMessage === 'function') {
              platform.handleWsMessage({ type, payload });
            }
          })
          .catch(() => {});
      } else if (type === 'youtube_chat_connected' || type === 'youtube_chat_message') {
        const ensure = () => {
          const platform = messageAggregator.platforms?.get('youtube');
          if (!platform) return messageAggregator.addPlatform('youtube', {}).then(() => messageAggregator.platforms.get('youtube'));
          return Promise.resolve(platform);
        };
        ensure()
          .then((platform) => {
            if (!platform) return;
            if (type === 'youtube_chat_connected') {
              if (typeof platform.handleWsMessage === 'function') {
                platform.handleWsMessage({ type, payload });
              }
            } else if (type === 'youtube_chat_message') {
              routeTo('youtube', () => payload);
            }
          })
          .catch(() => {});
      }
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
