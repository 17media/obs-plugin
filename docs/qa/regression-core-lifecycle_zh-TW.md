# 核心生命週期回歸檢查清單

此清單用於驗證針對 CoreRuntime/AuthSessionService/LocalGatewayService/ChatBridgeService/DockOrchestrator 的重構後，行為仍維持不變。

## 先決條件

- OBS 可正常啟動並載入外掛。
- 建置設定：`macos-user` preset（或同等）。
- 若需驗證登入/開播等流程，需可連線到 17Live API 端點。

## Smoke

- 啟動 OBS，確認外掛載入且不會崩潰。
- 開啟 OBS 後等待 10 秒再關閉 OBS；確認退出時不會卡住/無限等待。

## 本地服務 + ChatDock

- 從選單開啟 Chat Room dock。
- 確認頁面載入並連上本地 WS（ChatDock 完成註冊）。
- 關閉 Chat Room dock（dock 應該隱藏而非銷毀）。
- 重新開啟 Chat Room dock，確認可重新連線並持續收到事件。

本地網關 + 聊天橋行為參考：

- [local-gateway-chat-bridge.md](file:///Users/zhuyu/workspace/mk/17live/dev/obs-17live/docs/qa/local-gateway-chat-bridge.md)

## Dock 生命週期

- 透過選單切換各 dock：
  - Streaming
  - Live List
  - Rock Zone
  - Multi-RTMP
  - Preview
  - Chat Room
- 關閉所有 docks（透過外掛 shutdown 或手動關閉 OBS）。
- 重新啟動 OBS，確認登入後可正確還原先前顯示的 docks。

## 登入 / 登出

- 登入成功：
  - 確認選單顯示已登入使用者名稱。
  - 確認可開啟 Streaming dock。
- 非開播狀態登出：
  - 確認 docks 會關閉。
  - 確認本地 token/config 會被清除（符合預期）。
- 開播中登出：
  - 確認會出現警告對話框。
  - 選擇「No」應保持登入狀態不變。
  - 選擇「Yes」應停止開播後再登出。

## Chat Queue 行為

- Chat Room dock 關閉時：
  - 觸發 chat events（YouTube/Twitch/Ably），確認不會崩潰。
  - 重新開啟 Chat Room dock，確認佇列事件會 flush。
- Chat Room dock 開啟時：
  - 確認事件傳遞沒有明顯延遲。
- chat events 持續流動時關閉 OBS，確認可乾淨 shutdown。

- 斷線 → 重連 → flush：
  - 關閉 Chat Room dock 或強制 WS 斷線，然後觸發 chat events（佇列持續累積）。
  - 重連 Chat Room dock，確認完成 register 並將佇列事件 flush。

## Shutdown 冪等性

- 啟動後立刻關閉 OBS（未登入）。
- 開啟多個 docks 後關閉 OBS。
- 登入流程進行中（登入對話框開啟）時關閉 OBS。
- 開播中關閉 OBS（自動關閉/手動關閉路徑）。

