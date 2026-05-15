# 動畫觸發設定操作手冊（obs-17live）

本文檔說明 obs-17live 中「動畫觸發設定」的實際配置內容、操作方式、限制條件，以及本地配置的保存結構，方便開發、測試與排查。

## 功能概覽

「動畫觸發設定」用於讓主播在 OBS Plugin 中：

- 匯入本地影片或圖片作為播放素材
- 設定直式與橫式直播時的播放位置與大小
- 設定最多 5 條觸發條件
- 在直播中於條件達成後，自動排隊播放對應動畫
- 在設定階段手動預覽單一素材或位置效果

入口位於 17LIVE 選單中的「動畫觸發設定」。

## 配置保存位置

配置由 `OneSevenLiveConfigManager` 負責讀寫。

- 本地檔案：`~/.17Live/config.ini`
- section：`[OneSevenLive]`
- key：`CustomizedCartoonsConfigV1`
- 格式：JSON 字串

匯入的媒體檔案會複製到：

- `~/.17Live/customized_cartoons/`

這樣可以避免使用者移動或刪除原始檔案後造成設定失效。

## 配置結構

整體配置是一個 JSON object，目前主要包含三個區塊：

- `media`：媒體列表
- `rules`：觸發條件列表
- `position`：播放位置設定

範例：

```json
{
  "media": [
    {
      "id": "0d51d4c1f2c84f7ab9d7a6a8f6ef1234",
      "name": "gift.mp4",
      "path": "/Users/foo/.17Live/customized_cartoons/0d51d4c1_gift.mp4",
      "type": "video",
      "displaySec": 5
    }
  ],
  "rules": [
    {
      "id": "42d8f645fa7f42cda2a8d8f1971ab234",
      "name": "條件 1",
      "mediaId": "0d51d4c1f2c84f7ab9d7a6a8f6ef1234",
      "engageType": "GIFT_AMOUNT_MILESTONE",
      "points": 100,
      "count": 5,
      "repeatable": true,
      "enabled": true
    }
  ],
  "position": {
    "portrait": {
      "x": 200.0,
      "y": 300.0,
      "scaleX": 1.0,
      "scaleY": 1.0,
      "rot": 0.0,
      "alignment": 5,
      "boundsType": 4,
      "boundsAlignment": 5,
      "boundsW": 500.0,
      "boundsH": 500.0,
      "cropToBounds": true
    },
    "landscape": {
      "x": 200.0,
      "y": 300.0,
      "scaleX": 1.0,
      "scaleY": 1.0,
      "rot": 0.0,
      "alignment": 5,
      "boundsType": 4,
      "boundsAlignment": 5,
      "boundsW": 500.0,
      "boundsH": 500.0,
      "cropToBounds": true
    }
  }
}
```

## 媒體列表設定

左上區塊「影片文件設定」對應 `media` 陣列。

### 可匯入的檔案

- 影片：依 OBS `ffmpeg_source` 可支援的本地影片格式為準，例如 `mp4`、`mov`、`m4v`、`mkv`、`webm`、`avi`
- 圖片：非上述影片副檔名時，會視為圖片來源，例如 `png`、`jpg`、`gif`

### 匯入限制

- 檔案大小不得超過 `200MB`
- 影片長度不得超過 `15 秒`
- 匯入後會複製到 `~/.17Live/customized_cartoons/`

### 欄位說明

- `id`：媒體唯一識別碼，使用 UUID
- `name`：顯示名稱，通常是原始檔名
- `path`：複製後的本地檔案路徑
- `type`：`video` 或 `image`
- `displaySec`：圖片播放秒數，現行預設為 `5`

### 列表操作

- 點「選擇影片」可新增素材
- 點垃圾桶按鈕可刪除素材
- 若該素材正被手動預覽，刪除時會先停止預覽
- 若該素材正位於自動播放佇列或播放中，刪除時會先停止該次播放

### 素材預覽

每一列媒體右側有預覽按鈕：

- `play`：開始預覽該媒體
- `stop`：停止預覽該媒體

預覽行為：

- 一次只允許一個媒體處於手動預覽中
- 若已有其他媒體正在預覽，再點另一個播放按鈕會提示先停止
- 影片預覽時會循環播放
- 圖片預覽時會持續顯示，直到手動停止
- 在 OBS Studio Mode 下，會優先加到 Preview Scene
- 若未啟用 Studio Mode，則會回退到 Current Scene

## 位置設定

左下區塊「動畫位置設定」對應 `position` 物件，包含：

- `portrait`
- `landscape`

兩者結構相同，分別對應直式與橫式直播。

### 設定方式

- 可在畫布上拖拽藍色區塊調整位置與大小
- 可透過右側數值欄位直接輸入
- 可按「從畫布讀取」把目前 OBS overlay 的實際 transform 讀回欄位
- 可按「套用」將欄位內容回寫到配置

### 主要欄位

- `x`、`y`：左上角位置
- `boundsW`、`boundsH`：播放區域寬高
- `scaleX`、`scaleY`：縮放值
- `rot`：旋轉角度
- `alignment`：來源對齊方式
- `boundsType`：OBS bounds 類型
- `boundsAlignment`：bounds 對齊方式
- `cropToBounds`：是否裁切到 bounds

目前 UI 在按下「套用」時，會固定採用：

- `boundsType = OBS_BOUNDS_STRETCH`
- `boundsAlignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP`
- `alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP`
- `cropToBounds = true`

### 位置預覽

位置區塊內另有「預覽位置」與「停止預覽」按鈕。

- 會以目前選中的媒體作為預覽素材
- 直式與橫式各自套用對應的 `position.portrait` 或 `position.landscape`
- 若當前正在進行媒體列表的手動預覽，位置預覽會先停止該手動預覽

## 觸發條件設定

右側區塊「觸發條件設定」對應 `rules` 陣列。

### 數量限制

- 最多 `5` 條條件
- 超過時會彈出提示，不允許新增

### 每條條件的欄位

- `id`：條件唯一識別碼
- `name`：條件名稱，目前預設為 `條件 1`、`條件 2` 等
- `mediaId`：對應播放的媒體 `id`
- `engageType`：觸發規則類型
- `points`：禮物金額門檻
- `count`：次數門檻
- `repeatable`：是否可重複觸發
- `enabled`：是否啟用

### 規則類型

目前支援兩種：

- `GIFT_AMOUNT_MILESTONE`
  - 意義：金額超過 X 的禮物，送禮超過 Y 次
  - 對應欄位：
    - `points` = X（最小金額）
    - `count` = Y（送禮次數）
- `GIFT_LUCKYBAG_FIRST_PRIZE_MILESTONE`
  - 意義：隨機袋最大獎中獎 X 次
  - 對應欄位：
    - `count` = X（中獎次數）
    - `points` 會寫為 `0`

### 條件啟用規則

只有符合以下條件的規則會實際參與直播中的觸發監控：

- `enabled = true`
- `mediaId` 非空

## 直播中的播放邏輯

當直播進行中且 engagement 條件達成時，服務會依規則把對應 `mediaId` 放入播放佇列。

### 播放方式

- 同時間只播放一個動畫或圖片
- 若多條規則同時達成，後續素材進入 queue 等待
- 新素材不會中斷正在播放中的舊素材

### 影片播放

- 使用 OBS `ffmpeg_source`
- 非手動預覽時不循環
- 播放結束後自動隱藏 overlay，並播放下一個排隊素材

### 圖片播放

- 使用 OBS `image_source`
- 顯示時間由 `displaySec` 決定
- 目前預設為 `5 秒`

## 常見操作流程

### 新增完整設定

1. 開啟「動畫觸發設定」
2. 在「影片文件設定」匯入一個或多個影片/圖片
3. 在「動畫位置設定」中分別設定直式與橫式位置
4. 按「套用」保存位置
5. 在「觸發條件設定」中新增條件
6. 選擇規則類型、填入門檻、選擇播放素材
7. 設定是否重複播放與條件狀態
8. 按右下角「應用」或「確認」

### 檢查單一素材是否可用

1. 在媒體列表中找到目標素材
2. 點播放按鈕
3. 確認 OBS Preview 或 Current Scene 中是否正常顯示
4. 再點停止按鈕結束預覽

### 調整播放位置

1. 在媒體列表中先選擇一個素材
2. 切換直式或橫式頁籤
3. 點「預覽位置」
4. 在畫布拖拽藍色區塊或修改數值欄位
5. 點「套用」
6. 點「停止預覽」

## 例外與排查

### 匯入失敗

可能原因：

- 檔案不存在
- 複製失敗
- 檔案超過 200MB
- 影片長度超過 15 秒
- 配置保存失敗

### 預覽失敗

可能原因：

- 找不到媒體
- 找不到檔案
- `ffmpeg_source` 或 `image_source` 不可用
- overlay scene item 尚未建立成功

### 規則不生效

請依序確認：

- 該規則是否為 `enabled = true`
- 該規則是否已綁定 `mediaId`
- 直播是否已開始，且 engagement 規則已成功建立
- 本地媒體檔案是否仍存在

## 相關程式碼位置

- UI：`src/17live/customized_cartoons/CustomizedCartoonDock.(hpp|cpp)`
- 服務與播放邏輯：`src/17live/customized_cartoons/CustomizedCartoonService.(hpp|cpp)`
- 配置讀寫：`src/17live/OneSevenLiveConfigManager.(hpp|cpp)`
- 需求來源：`temp/docs/p3/customize_cartoon/requirements.md`
