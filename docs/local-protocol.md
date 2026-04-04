# 本地 HTTP / WebSocket 协议（OBS 17Live Plugin）

本文档整理插件与内置 Web 前端（`web/ably_chat`）之间的本地通信协议，覆盖：

- 本地 HTTP server（静态资源 + `/lapi`）
- 本地 WebSocket server（聊天事件总线 + VFF 播放控制）

对应实现主要在：

- [OneSevenLiveCoreManager.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp)
- [OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp)
- [OneSevenLiveWebsocketServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/websocket/OneSevenLiveWebsocketServer.cpp)
- [WsMessage.hpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/websocket/WsMessage.hpp)
- Web 前端侧：[WebSocketManager.js](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/services/WebSocketManager.js)

---

## 1. 本地端口与 URL

### 1.1 端口分配

插件启动时会各自绑定一个本地随机端口（`port=0` 让系统分配）：

- HTTP：`OneSevenLiveHttpServer("localhost", 0, "html/chat", ...)`  
  见 [OneSevenLiveCoreManager::initialize](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L114-L129)
- WebSocket：`OneSevenLiveWebsocketServer("localhost", 0)`  
  见 [OneSevenLiveCoreManager::initialize](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L131-L154)

### 1.2 Chat Room 页面 URL（Dock）

ChatDock 打开的页面 URL 由插件拼接，关键 query 参数：

- `roomID`：17LIVE room id
- `userID`：17LIVE user id
- `ws`：本地 WebSocket 地址（形如 `ws://127.0.0.1:<wsPort>`）

实现见 [handleChatRoomClicked](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L1617-L1633)：

```
http://localhost:<httpPort>/<locale>.html?roomID=<roomID>&userID=<userID>&ws=ws://127.0.0.1:<wsPort>
```

### 1.3 VFF 预览页面 URL（Preview Dock）

预览 Dock URL（用于礼物 VFF 播放）：

实现见 [createPreviewDock](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L1925-L1934)：

```
http://localhost:<httpPort>/vff/?ws=ws://127.0.0.1:<wsPort>
```

---

## 2. 本地 HTTP 协议

HTTP server 基于 cpp-httplib，启动时将 `/` mount 到插件数据目录下的 `html/chat`（安装时的 module data path），实现见：

- mount 与静态 handler：[OneSevenLiveHttpServer::start](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L110-L191)
- 初始化 `html/chat` 根目录：[OneSevenLiveCoreManager::initialize](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L114-L118)

### 2.1 `GET /ping`

- 用途：健康检查
- 返回：`PONG`（text/plain）

实现：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L277-L294)

### 2.2 `GET /csrf-token`

- 用途：返回一个服务器生成的 CSRF token
- 返回：

```json
{ "success": true, "csrf_token": "<token>" }
```

实现：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L296-L321)

备注：当前 `/lapi` 处理逻辑没有强制校验该 token（代码层面仅提供了 token 发放 endpoint）。

### 2.3 `POST /lapi`

用途：本地 JSON RPC（Web 前端在 production 下通过该入口向插件请求数据）。

实现：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L323-L503)

#### 请求

- `Content-Type: application/json`
- body：至少包含 `action: string`

```json
{ "action": "getRoomInfo" }
```

Web 前端调用示例可参考：

- [room.js](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/platforms/17live/api/room.js#L16-L38)
- [gifts.js](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/platforms/17live/api/gifts.js#L31-L66)

#### 支持的 action（当前实现）

- `getAblyToken`：返回 Ably token（roomID 从配置读）  
  分支：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L412-L416)
- `getGifts`：返回 gift 列表（内部带缓存；可能返回 “Gifts loading”）  
  分支：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L416-L435)
- `getGift`：按 `giftID` 返回单个 gift（当 gifts 正在加载会返回 “Gifts loading”）  
  分支：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L435-L463)
- `getRoomInfo`：返回 room 信息  
  分支：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L464-L473)

#### 返回

- 成功：直接返回对应 API 的 JSON 结果（不额外包一层 success 字段）  
  见写回逻辑：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L491-L495)
- 失败：返回 `{ "success": false, "error": "..." }`（JSON）  
  例如：缺 action、JSON parse error、unsupported action、rate limit 等  
  见各类错误返回分支：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L368-L489)

### 2.4 静态资源

- `GET /.*`：静态文件分发（`/` 会映射成 `/index.html`），带简单路径安全校验与限流  
  见：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L121-L190)
- `GET /`：显式返回 `index.html`（同样有限流/路径校验）  
  见：[OneSevenLiveHttpServer.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L192-L258)

---

## 3. 本地 WebSocket 协议

Web 前端只会在 URL query 里存在 `ws=` 时连接（否则完全不连接），实现见：

- [WebSocketManager.connect](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/services/WebSocketManager.js#L21-L61)

### 3.1 消息格式（ChatDock 统一信封）

对“聊天事件”这条链路，C++ 侧定义了统一消息信封 `WsMessage`：

```json
{ "type": "<string>", "payload": { } }
```

实现见：[WsMessage.hpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/websocket/WsMessage.hpp#L18-L39)

解析规则要点：

- `type` 必须是 string
- `payload` 必须是 object（否则会被置为空 object）

### 3.2 Web → 插件（入站）

入站消息由 [handleWebsocketMessage](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L363-L405) 处理，目前实际处理的只有两类：

#### 3.2.1 注册 ChatDock

Web 端在 `onopen` 时会发送注册消息：

```json
{ "type": "action", "payload": { "type": "register_chatdock" } }
```

实现：

- 发送：[WebSocketManager.js](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/services/WebSocketManager.js#L81-L97)
- 处理：[OneSevenLiveCoreManager.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L378-L388)

效果：服务器将该连接登记为 ChatDock client，并把历史队列 flush 给该连接（见 `flushChatEventQueue`）。

#### 3.2.2 上报 Ably 聊天消息（回灌）

插件支持接收一条 `ably_chat_message` 并转交 `OneSevenLiveChatMessageHandler` 走统一解析：

```json
{
  "type": "ably_chat_message",
  "payload": {
    "roomID": "<string>",
    "data": "<string>"
  }
}
```

处理逻辑见：[OneSevenLiveCoreManager.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L389-L404)

备注：Web 侧的 [WSSender.js](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/services/WSSender.js#L13-L49) 默认 envelope 把 `roomID/userID` 放在顶层，不在 `payload` 内；若要触发上述回灌逻辑，需要把 `roomID/data` 放入 `payload`。

### 3.3 插件 → Web（出站：ChatDock 事件）

插件通过 `OneSevenLiveCoreManager::enqueueOrBroadcastChatEvent` 给 ChatDock 发送消息：

- 若已登记 chatDockClientId：直接 `sendMessageToClient(chatDockClientId, WsMessage{type,payload}.dump())`
- 否则：进入队列，等待注册后 flush

实现见：

- enqueue/broadcast：[OneSevenLiveCoreManager.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L675-L698)
- flush：[OneSevenLiveCoreManager.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L700-L726)

已观察到的 `type` 与 `payload`：

#### 3.3.1 Twitch

- `twitch_chat_connected`

```json
{ "type": "twitch_chat_connected", "payload": { "username": "<string>", "status": "connected|break" } }
```

实现：[OneSevenLiveTwitchChatClient.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/twitch/OneSevenLiveTwitchChatClient.cpp#L238-L291)

- `twitch_chat_message`

```json
{ "type": "twitch_chat_message", "payload": { "raw": "<IRC line>" } }
```

实现：[OneSevenLiveTwitchChatClient.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/twitch/OneSevenLiveTwitchChatClient.cpp#L223-L236)

Web 侧会把 `payload.raw` 解析成简单的 `{channel, username, message...}` 再进入统一消息管线，见 [WebSocketManager.js](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/services/WebSocketManager.js#L123-L163)。

#### 3.3.2 YouTube

- `youtube_chat_connected`

```json
{ "type": "youtube_chat_connected", "payload": { "status": "connected|break" } }
```

实现：[OneSevenLiveYouTubeChatClient.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/youtube/OneSevenLiveYouTubeChatClient.cpp#L638-L646)

- `youtube_chat_message`（结构化 JSON）

`payload` 由 `YouTubeChatMessage` 序列化得到，字段见 `toJson`：

- `kind / etag / id`
- `snippet`: `type/liveChatId/authorChannelId/publishedAt/displayMessage/textMessageDetails/messageId`
- `authorDetails`: `channelId/displayName/profileImageUrl/isVerified/isChatOwner/isChatSponsor/isChatModerator`

实现：[OneSevenLiveYouTubeChatClient.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/youtube/OneSevenLiveYouTubeChatClient.cpp#L37-L65)（toJson）与 [L595-L601](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/youtube/OneSevenLiveYouTubeChatClient.cpp#L595-L601)（发送）。

#### 3.3.3 17LIVE（Ably）

- `ably_chat_connected`

```json
{ "type": "ably_chat_connected", "payload": { "status": "connected|break", "error": "<string?>"} }
```

实现示例：[OneSevenLiveAblyChatClient.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/api/OneSevenLiveAblyChatClient.cpp#L144-L160)

- `ably_chat_message`

`payload` 为解码后的 Ably 消息 JSON（包含 `type` 数字等字段），由 `OneSevenLiveChatMessageHandler` 路由：

实现：[OneSevenLiveChatMessageHandler.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/chat/OneSevenLiveChatMessageHandler.cpp#L78-L91)

Web 侧会把 `{type,payload}` 透传给 17live 平台处理，见 [WebSocketManager.js](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/services/WebSocketManager.js#L163-L177)。

### 3.4 插件 → Web（广播：VFF 播放控制，非 WsMessage 信封）

礼物播放（VFF）走的是“直接广播 JSON 字符串”，不是 `WsMessage` 信封：

- 广播点：`ws->broadcastMessage(playData.dump())`  
  见 [OneSevenLiveChatMessageHandler.cpp](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/chat/OneSevenLiveChatMessageHandler.cpp#L131-L167)

消息 schema（示例）：

```json
{
  "type": "play_vff",
  "vffURL": "<string>",
  "vffJson": "<string>",
  "compositeData": { "<tag>": "<imageURL>" }
}
```

该消息预期由 `/vff/` 页面消费（页面 URL 见 1.3）。

---

## 4. 常见问题与排障提示

- **Web 页面打开 404 / 白屏**
  - 先确认 `data/html/chat/index.html` 存在（前端产物是否构建/复制）。
  - HTTP server 的 base_dir 来源于 `obs_get_module_data_path()/html/chat`，见 [OneSevenLiveCoreManager::initialize](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L114-L118) 与 [OneSevenLiveHttpServer::OneSevenLiveHttpServer](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveHttpServer.cpp#L58-L77)。

- **WebSocket 没连接上**
  - Web 侧只有在 URL query 中存在 `ws=` 才会连接，见 [WebSocketManager.connect](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/web/ably_chat/src/services/WebSocketManager.js#L21-L61)。
  - 插件拼接的 `ws` 使用 `ws://127.0.0.1:<port>`，见 [handleChatRoomClicked](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L1627-L1633)。

- **没有收到聊天消息**
  - 确认 Web 在 `onopen` 发送了 `register_chatdock`，否则插件会把事件缓存到队列里不发出（见 [handleWebsocketMessage](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L378-L388) 与 [enqueueOrBroadcastChatEvent](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/src/17live/OneSevenLiveCoreManager.cpp#L675-L698)）。

