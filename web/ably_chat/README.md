## 環境需求

Node JS 請用 v20 以上

## 初始化

```bash
npm install
```

## 本機開發

```bash
npm run dev
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
