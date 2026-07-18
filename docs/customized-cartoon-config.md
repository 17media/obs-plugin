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
- 直式與橫式各自使用當前頁籤對應的草稿位置設定
- 預覽不要求先按「套用」，未保存的草稿位置也會直接反映到預覽
- 若預覽已開啟，繼續拖拽藍框或修改數值，overlay 位置會即時更新
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

## 自訂動畫播放流程與效果

本節補充 `CustomizedCartoonService` 實際的執行邏輯，說明 source 何時建立、何時顯示、何時移除，以及直播播放與手動預覽的差異。

### 整體狀態模型

自訂動畫 overlay 主要有 4 種工作狀態：

- `直播待命`：已開播，且至少有一條啟用規則綁定了有效媒體
- `直播播放中`：某條 engagement 條件達成後，素材已進入播放或佇列
- `媒體預覽中`：從媒體列表點擊單一素材的播放按鈕
- `位置預覽中`：在位置設定區塊內點擊「預覽位置」

只要處於以下任一情況，overlay source 與 scene item 就會被保留：

- 正在媒體預覽
- 正在位置預覽
- 正在播放或播放佇列非空
- 直播狀態為 Streaming，且至少存在一條啟用規則

不符合上述條件時，service 會把 scene 中的自訂動畫 scene item 移除。

### Source 與 Scene Item 機制

實際播放共用兩個固定名稱的 OBS source：

- `17LiveCustomizedCartoonMedia`：影片，使用 `ffmpeg_source`
- `17LiveCustomizedCartoonImage`：圖片，使用 `image_source`

設計重點：

- source 以固定名稱重用，不會每次播放都新建一個重名 source
- scene item 也會優先查找既有項目，找不到才加到當前 Preview Scene / Current Scene
- 影片與圖片共用同一套 transform
- 真正顯示時只會顯示其中一個：
  - 影片素材：顯示 `media` source，隱藏 `image`
  - 圖片素材：顯示 `image` source，隱藏 `media`

畫面效果上，動畫會被放到 scene 最上層，並依 `position.portrait` 或 `position.landscape` 的 bounds 設定裁切與拉伸顯示。

### 直播正式播放流程

直播中的正式播放流程如下：

1. 開播後，service 會根據已啟用且已綁定媒體的規則建立 engagement
2. `pollTimer_` 每 10 秒輪詢一次 engagement progress
3. 當某條規則對應的 progress 達到 `current >= target` 時，該輪對應素材 `mediaId`
   會加入播放佇列；若輪詢剛好跨過達標點，會依已完成的輪次補發，不會在新一輪
   `1/5` 時先觸發
4. 若當前沒有其他素材播放，就立即開始 `startNextPlayback()`
5. 播放前會依直播房間的直式 / 橫式狀態選用已保存的正式位置配置
6. 影片播放完畢或圖片顯示秒數結束後，自動隱藏 overlay，並播放佇列中的下一個素材

正式播放的特性：

- 同時間只播放一個素材
- 新素材只入 queue，不會打斷正在播放的素材
- 直播正式播放一律讀取已保存配置，不讀取 dock 中未保存草稿

### 手動預覽流程

手動預覽分為兩種：

- `媒體預覽`：驗證某個素材本身是否可播、畫面大小位置是否合適
- `位置預覽`：驗證當前直式 / 橫式位置配置與素材的實際疊加效果

共同特性：

- 開始預覽前，若存在另一種預覽或正式播放，會先停止舊狀態
- 預覽會先把 OBS 預覽畫布切到對應方向：
  - 橫式：`1280x720`
  - 直式：`720x1280`
- 停止預覽時，會恢復 OBS 原本的畫布設定

差異如下：

- `媒體預覽`
  - 使用當前頁籤方向
  - 位置採用當前 dock 內的草稿位置設定
  - 適合快速檢查素材播放與當前位置效果
- `位置預覽`
  - 使用當前頁籤方向
  - 同樣採用當前 dock 內的草稿位置設定
  - 若預覽已開啟，繼續拖動藍框或改 `X / Y / 寬度 / 高度`，OBS 中的 overlay 會同步更新

這代表：

- `預覽` 看的是當前正在編輯的草稿
- `套用` 才會把草稿寫進 `config.ini`
- `直播正式播放` 只看已保存配置

### 位置配置與畫布縮放關係

位置設定的基準畫布固定為：

- `portrait`：`720 x 1280`
- `landscape`：`1280 x 720`

實際套用到 OBS scene item 時：

- 若當前畫布就是上述基準尺寸，直接按配置值使用
- 若實際畫布尺寸不同，會按寬高比例縮放 `x / y / boundsW / boundsH`
- 從 OBS 畫布讀回位置時，也會反向換算回上述基準座標系

因此文檔中的位置值，代表的是標準直式或橫式畫布下的配置值，而不是任意畫布尺寸下的絕對像素。

### 使用者可見效果

從使用者角度，可觀察到的效果如下：

- 開播前且沒有手動預覽時，scene 中不會長期保留可見的動畫 overlay
- 開播後若至少存在一條啟用規則，overlay source 會進入待命狀態，但只有播放瞬間才可見
- 影片素材會循環播放於手動預覽，但在直播正式播放中不循環
- 圖片素材在手動預覽中會持續顯示，直到手動停止；正式播放則按 `displaySec` 自動結束
- 停播後，若沒有手動預覽或待播內容，overlay 會從 scene 中移除

## 測試建議

建議至少按以下維度驗證，避免只測單一路徑。

### 基本功能

- 匯入 1 個影片與 1 個圖片，確認都能正常預覽與停止
- 在直式與橫式頁籤分別設定不同位置，確認切換頁籤後藍框與數值能正確切換
- 不按「套用」，直接點媒體預覽與位置預覽，確認使用的是當前草稿位置
- 預覽開啟後繼續修改 `X / Y / 寬度 / 高度`，確認 OBS 中 overlay 即時更新

### 配置與保存

- 修改位置後點「套用」，重新打開 dock，確認配置已保存
- 修改位置但不點「套用」直接關閉，再重新打開，確認未保存草稿不會污染正式配置
- 驗證直播正式播放仍然只使用已保存配置，而不是上次未套用草稿

### 直播播放

- 建立至少 2 條啟用規則，綁定不同媒體，確認達標時素材會按 queue 順序播放
- 驗證同一時間只會有一個素材顯示，後續素材不會打斷前一個
- 驗證影片正式播放完畢後會自動隱藏，圖片會在 `displaySec` 後自動結束
- 停播後確認 overlay source / scene item 會被移除，不殘留在 scene 中

### 預覽互斥與切換

- 媒體預覽進行中再點位置預覽，確認媒體預覽會先停止
- 位置預覽進行中再點媒體列表其他素材預覽，確認位置預覽會先停止
- 反覆開始 / 停止預覽，確認不會產生重複 source 或重複 scene item
- 在 Studio Mode 與非 Studio Mode 下都驗證一次，確認 overlay 被加到正確的 scene

### 畫布與方向

- 橫式預覽時確認 OBS 畫布切到 `1280x720`，停止後恢復原值
- 直式預覽時確認 OBS 畫布切到 `720x1280`，停止後恢復原值
- 使用非基準輸出尺寸驗證一次，確認 transform 會按比例縮放，顯示位置不偏移

### 例外場景

- 測試不存在的媒體檔案，確認會彈出預覽失敗錯誤
- 測試刪除正在預覽中的素材，確認會先停止預覽再刪除
- 測試刪除正在播放中的素材，確認會先停止該次播放並清理狀態
- 測試所有規則都停用後開播，確認不會保留待命 overlay

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
5. 視需要點「套用」保存正式配置
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
