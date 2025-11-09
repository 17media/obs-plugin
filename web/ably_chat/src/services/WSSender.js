/**
 * 公共 WebSocket 发送函数
 * - 任何地方可直接调用
 * - 自动确保连接（幂等）
 * - 统一封装消息信封格式
 */
import { wsManager } from './WebSocketManager';

/**
 * 发送到 WS 服务端
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
    throw new Error('sendWSMessage 需要提供 type');
  }
  try {
    await wsManager.connect();
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
    // 发送（若未连接则进入队列，连接后自动发送）
    wsManager.send(envelope);
    return true;
  } catch (error) {
    console.error('sendWSMessage 发送失败:', error);
    return false;
  }
}

export default sendWSMessage;