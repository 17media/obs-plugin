## 環境需求

Node JS 請用 v20 以上

## ENABLE_YOUTUBE

- 預設為關閉
- 只有在明確設定 `ENABLE_YOUTUBE=true` 時，才會顯示 YouTube channel 與啟用前端 YouTube 平台處理
- 若未設定或不是 `true`，會維持關閉

## 初始化

```bash
npm install
```

## 本機開發

```bash
npm run dev
```

若要在本機開發時開啟 YouTube：

```bash
ENABLE_YOUTUBE=true npm run dev
```

## 正式建置

預設建置：

```bash
npm run build
```

開啟 YouTube 的建置：

```bash
ENABLE_YOUTUBE=true npm run build
```

## 檔案說明

- `/src/app`
  - 範例網站主入口，可以直接參考 `Ably.jsx`，裡面描述怎麼訂閱 Ably 服務，和如何引入聊天樣式元件。
  - `config.js` 用來調整 demo 專案所需變數
    - `roomID` 直播間 ID，可於 login api 得到
    - `userID` 主播 ID，可於 login api 得到
- `/src/lib`
  - 聊天樣式元件
- `/src/util`
  - 提供 `Ably.jsx` 調用的 util function。
  - `getAblyDecodeData.js` 用來解析 Ably 中的加密訊息。
  - `getChatProps.js` 用來重組解密後的資料結構，提供聊天資料給聊天樣式元件用。
