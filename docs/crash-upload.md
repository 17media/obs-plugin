# Crash 日志自动上传（obs-17live）

本文档说明 obs-17live 插件在检测到异常退出后，于下次正常登录时提示用户上传 crash 日志的实现方式：包含异常退出判断方法、采集内容、去重策略，以及 API 上传流程。

## 需求来源

- 产品需求：`temp/docs/p3/crash_upload/requirements.md`
- API 参考：`temp/docs/p3/crash_upload/Phase 3-TEAMZ-OBS Log 上傳-210426-101106.pdf`

## 异常退出判断（Unclean Shutdown）

插件采用与 OBS 主程序一致的 “crash sentinel（哨兵文件）” 思路：用文件是否被正常清理来判断上次运行是否正常退出，而不是依赖某个 bool 标记位。

### 为什么不用 LastRunClean

仅在插件 “正常退出流程” 中将 `LastRunClean=true` 的方案可能误判：例如插件逻辑已结束，但在 Qt/资源释放后段发生崩溃，仍可能把上次运行标记成 “正常退出”。

### Sentinel 机制

- 目录：`~/.17Live/.sentinel/`
- 文件命名：`run_<uuid>`
- 判断逻辑：
  1. 启动时检查目录下是否存在任何 `run_` 前缀文件
     - 存在：说明上一次没有走到正常清理（可能 crash/强杀/异常退出），判定为 `previousRunClean=false`
     - 不存在：判定为 `previousRunClean=true`
  2. 启动后创建本次运行的 `run_<uuid>` 文件
  3. 正常卸载（obs_module_unload）时清理目录下所有 `run_` 文件

### 代码位置

- Sentinel 实现：`src/17live/utility/CrashSentinel.(hpp|cpp)`
- 创建/清理钩子：
  - `obs_module_load()`：`src/plugin-main.cpp`
  - `obs_module_unload()`：`src/plugin-main.cpp`
- 上次是否正常退出读取：`src/17live/OneSevenLiveCoreManager.cpp` 中 `previousRunClean_ = CrashSentinel::PreviousRunClean()`

## Crash 文件候选（macOS）

在 macOS 上，为了生成更准确的去重 key 与 crash 时间戳，插件会尝试查找系统 crash report：

- 扫描目录：
  - `~/Library/Logs/DiagnosticReports`
  - `/Library/Logs/DiagnosticReports`
- 文件筛选：
  - 扩展名：`.crash` / `.ips`
  - 文件名前缀：`obs*` 或 `OBS*`
- 处理：
  - 按修改时间倒序取最新 5 个
  - “crashTimestampSec” 优先取最新候选文件的 mtime（否则用当前时间）

对应实现：`src/17live/core/CrashUploadService.cpp` 的 `detectCrashCandidates()`

## 去重策略（避免重复提交）

为了避免重复上传相同的 crash，插件会将上传记录写入本地 config，作为 “已上传” 判断依据。

### 记录存储

- 文件：`~/.17Live/config.ini`
- section：`[OneSevenLive]`
- key：`CrashUploadHistory`
- 格式：JSON array（字符串数组）

实现：`src/17live/OneSevenLiveConfigManager.(hpp|cpp)` 的 `getCrashUploadHistory()` / `addCrashUploadHistory()`

### 唯一标识（key）生成

每个候选 crash 文件生成一个 record key：

`<userId>|<normalizedFileName>|<md5(originalFileName)>`

- `userId`：登录后的 user id
- `normalizedFileName`：对文件名做归一化（去除常见时间戳片段），减少因时间戳变化导致的误判
- `md5(originalFileName)`：辅助区分同一 base name 下的不同文件

实现：`src/17live/core/CrashUploadService.cpp` 的 `buildRecordKeys()` / `normalizeFileName()` / `md5Hex()`

### 判定规则

当本次候选 keys 全部都存在于 `CrashUploadHistory` 时，认为已上传过，不再提示。

## 上传触发与 UI 流程

### 触发时机

- 条件：
  - `previousRunClean == false`（上次异常退出）
  - 本次登录成功
  - 候选 crash 未被 `CrashUploadHistory` 去重
- 入口：`CrashUploadService::onLogin(...)`

### UI 流程

1. 弹出确认对话框（是否上传）
2. 用户点击 “确认上传” 后：
   - 弹出进度对话框（QProgressDialog，模态）
   - 依次显示阶段：
     - Preparing
     - Collecting（采集）
     - Reporting（回报 crash 事件）
     - Uploading（上传文件，显示百分比）
   - 支持 Cancel：用户点击后会明确取消本次上传
3. 上传结束后弹出结果提示（成功/失败/取消）

对应实现：`src/17live/core/CrashUploadService.cpp`

## 采集内容（Diagnostics Package）

插件使用 `seventeen::diag::IDiagnosticsCollector` 打包 zip，默认开启隐私过滤：

- `enablePrivacyFilter = true`
- `includeSensitiveData = false`

采集分类（categories）：

- `OBS_LOGS`
- `PLUGIN_LOGS`
- `CRASH_INFO`
- `CONFIG_SNAPSHOT`
- `SYSTEM_INFO`

输出：

- 生成 zip 到系统 temp 目录：`17live_diagnostics_<ts>.zip`
- 文件大小限制：超过 25MB 会直接失败并提示用户

对应实现：`src/17live/core/CrashUploadService.cpp`

## API 上传流程

上传为两步：

### 1) 回报 crash 事件

- URL：`POST /api/v1/logs`
- Content-Type：`application/json`
- Body（JSON）：
  - `type`: `"obs_crash_log"`
  - `liveStreamID`: 可选（如果本地可读到 streaming info）
  - `crashTimestampSec`: int64（秒）

实现：`OneSevenLiveApiWrappers::ReportObsCrashEvent(...)`

### 2) 上传 zip 文件

- URL：`POST /api/v1/logs/uploadFile`
- Content-Type：`multipart/form-data`
- 表单字段：
  - `file`: zip 文件

实现：

- `OneSevenLiveApiWrappers::UploadObsLogsFile(zipPath, onProgress, cancelFlag)`
- 底层 curl：`UploadMultipartFileWithProgress(...)`（用于上传进度与取消中断）

### 通用请求 headers（节选）

由 `OneSevenLiveApiWrappers::TryInsertCommand(...)` 统一附加（不应在 log 中输出 Authorization）：

- `Authorization: Bearer <token>`（需要 token 的接口）
- `Devicetype: WEB`
- `version: <PLUGIN_VERSION>`
- `OSVersion: <os version>`
- `hardware: <os>`
- `deviceId: <platform uuid>`
- `deviceName/deviceModel: OBSPlugin`

## 日志输出（调试与排查）

Crash 上传流程会输出关键阶段与服务端返回（不包含 token 等敏感 header）：

- `CrashUpload: collecting diagnostics package`
- `CrashUpload: diagnostics package created: ...`
- `CrashUpload: ReportObsCrashEvent start/success/failed ...`
- `CrashUpload: UploadObsLogsFile start/success/api error/network failed ...`

对应实现：`src/17live/core/CrashUploadService.cpp`、`src/17live/api/OneSevenLiveApiWrappers.cpp`

