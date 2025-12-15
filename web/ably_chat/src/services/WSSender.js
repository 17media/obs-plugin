/**
 * Common WebSocket send function
 * - Can be called from anywhere
 * - Ensures connection automatically (idempotent)
 * - Unified envelope format for messages
 */
import { wsManager } from './WebSocketManager';

/**
 * Send to WS server
 * params: { type, payload, platform, roomID, userID, source, extra }
 */
export async function sendWSMessage({
  type,
  payload = {},
  platform,
  roomID,
  userID,
  source = 'client',
  extra = {},
} = {}) {
  if (!type) {
    throw new Error('sendWSMessage requires a type');
  }
  try {
    // Do not auto-connect. Only send when WS is already configured and open.
    const status = wsManager.getStatus();
    if (!status.configured || status.status !== 'open') {
      console.warn('WS not connected or URL missing, skipping send');
      return false;
    }
    const envelope = {
      source,
      platform,
      roomID,
      userID,
      type,
      payload,
      timestamp: Date.now(),
      ...extra,
    };
    // Send (if not connected it queues, will send after connect)
    wsManager.send(envelope);
    return true;
  } catch (error) {
    console.error('sendWSMessage failed:', error);
    return false;
  }
}

export default sendWSMessage;