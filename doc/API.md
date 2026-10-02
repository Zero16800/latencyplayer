# LatencyPlayer SDK API 文档

## 概述

LatencyPlayer SDK 是基于 **GStreamer 1.28.7** 的 Android 直播播放器 SDK（当前主路径 RTMP 拉流），对外提供 `com.latencyplayer.sdk` 下的 Java API。

**注意：** 渲染使用 `LatencyPlayerView`（SurfaceView）。布局中 **不要** 为该 View 设置不透明背景（如 `#000000`），否则会黑屏。

---

## 核心类

### LatencyPlayerManager

播放器核心管理类，提供播放、暂停、停止、音量、截图等能力。

#### 方法

| 方法 | 说明 |
|------|------|
| `init(Context)` | 初始化（Application Context），必须先调用；重复 init 忽略，`release()` 后不可再 init |
| `setConfig(LatencyPlayerConfig)` | 设置配置，并下发 `bufferTime` / `lowLatency` / `mute` / `volume` 到 Native |
| `getConfig()` | 取当前配置（`@NonNull`） |
| `setCallback(LatencyPlayerCallback)` | 设置事件回调（可空） |
| `setSurface(LatencyPlayerView)` | 绑定渲染 View（推荐） |
| `setSurface(Surface)` | 直接绑定 Android `Surface` |
| `start()` | 开始播放；**必须先** `setConfig` 且 `url` 非空 |
| `stop()` | 停止播放 |
| `pause()` | 暂停 |
| `resume()` | 恢复 |
| `setMute(boolean)` | 静音 / 取消静音 |
| `isMute()` | 是否静音 |
| `setVolume(int)` | 音量 0–100（配置内会 clamp） |
| `setBufferTime(int)` | 运行时改缓冲时间 (ms)，范围 0–5000，0 = 最低延迟 |
| `switchUrl(String)` | 热切换地址（内部 stop + play） |
| `saveSnapshot(String)` | 截图保存路径；`.png`→PNG、`.jpg`/`.jpeg`→JPEG(quality=90)，返回 `boolean` 是否成功 |
| `setOrientation(int)` | 1=竖屏, 2=横屏 |
| `setFlip(boolean, boolean)` | 水平 / 垂直翻转 |
| `setRotation(int)` | 旋转 0 / 90 / 180 / 270 |
| `isPlaying()` | 状态是否为 `PLAYING` |
| `getState()` | 当前 `LatencyPlayerState` |
| `release()` | 释放 Native 资源；可重复调用，之后不可再使用实例 |

#### 使用示例

```java
LatencyPlayerManager player = new LatencyPlayerManager();
player.init(context.getApplicationContext());

LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url("rtmp://example.com/live/stream")
    .bufferTime(0)
    .lowLatency(true)
    .fastStartup(true)
    .build();

player.setConfig(config);
player.setSurface(playerView); // LatencyPlayerView
player.setCallback(callback);
player.start();
```

---

### LatencyPlayerConfig

Builder 模式配置类。

#### 配置项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `url` | String | null | 播放地址，`start()` 前必填 |
| `bufferTime` | int | 200 | 缓冲时间 (ms)，0–5000；`setBufferTime` / setter 会 clamp；**0 = 最低延迟** |
| `fastStartup` | boolean | true | 首屏秒开标记（业务层配置项） |
| `lowLatency` | boolean | false | 超低延迟模式；经 `setConfig` → `nativeSetLowLatency` 下发（压扁内部 queue 等） |
| `hardwareDecoder` | boolean | true | 硬解优先标记 |
| `autoReconnect` | boolean | true | 自动重连（`ReconnectManager`） |
| `reconnectInterval` | int | 3 | 重连间隔 (秒) |
| `maxReconnectCount` | int | 0 | 最大重连次数，0=无限 |
| `orientation` | int | 1 | 1=竖屏, 2=横屏 |
| `mute` | boolean | false | 静音 |
| `volume` | int | 100 | 音量 0–100（setter clamp） |

> Builder 的 `bufferTime(int)` 写入时**不 clamp**，`setBufferTime` / 对应 setter 会 clamp 到 0–5000。

#### Builder 示例

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url("rtmp://example.com/live/stream")
    .bufferTime(0)           // 最低延迟
    .fastStartup(true)
    .lowLatency(true)        // 超低延迟
    .hardwareDecoder(true)
    .autoReconnect(true)
    .reconnectInterval(5)
    .maxReconnectCount(0)
    .build();
```

---

### LatencyPlayerView

渲染用 `SurfaceView`。

- 内部 `setBackgroundColor(0)` + holder `setFormat(RGBA_8888)`
- **XML 不要再设不透明 `background`**
- 通过 `setOnSurfaceReadyListener` 向 Manager 汇报 Surface 生命周期
- `getSurfaceHolder()` 返回当前 holder（可能为 null）

#### 推荐 XML

```xml
<com.latencyplayer.sdk.LatencyPlayerView
    android:id="@+id/player_view"
    android:layout_width="match_parent"
    android:layout_height="300dp" />
```

---

### LatencyPlayerCallback

全部回调在**主线程**执行。

| 方法 | 说明 |
|------|------|
| `onStateChanged(LatencyPlayerState)` | 状态变化 |
| `onError(int, String)` | 错误（码见 `LatencyPlayerError`，消息来自 `getDescription`） |
| `onBuffering(int)` | 缓冲进度 0–100% |
| `onConnected()` | 连接成功（对应 STATE_CONNECTED） |
| `onDisconnected()` | 连接断开 |
| `onFirstFrameRendered()` | 首帧渲染（通常也会置为 PLAYING） |
| `onDownloadSpeed(long)` | 下载速度 (bytes/s) |
| `onReconnecting(int)` | 正在重连（第几次） |
| `onVideoSizeChanged(int, int)` | 视频宽高（Native 分参数下发，避免 32 位截断） |
| `onPlaybackCompleted()` | 播放完成（非直播 / EOS） |

---

### LatencyPlayerState

| 状态 | value | 说明 |
|------|-------|------|
| `IDLE` | 0 | 空闲，未初始化 |
| `INITIALIZING` | 1 | 初始化中 |
| `READY` | 2 | 已就绪 |
| `BUFFERING` | 3 | 缓冲 / 启动中 |
| `PLAYING` | 4 | 播放中 |
| `PAUSED` | 5 | 已暂停 |
| `STOPPING` | 6 | 停止中 |
| `STOPPED` | 7 | 已停止 |
| `ERROR` | 8 | 错误 |

`fromValue(int)`：未知值回落 `IDLE`。

---

### LatencyPlayerError

| 错误码 | 常量 | 说明 |
|--------|------|------|
| 0 | `NONE` | 无错误 |
| -1 | `UNKNOWN` | 未知错误 |
| 1001 | `NETWORK_CONNECT_FAILED` | 网络连接失败 |
| 1002 | `NETWORK_TIMEOUT` | 网络超时 |
| 1003 | `RTMP_CONNECT_FAILED` | RTMP 连接失败 |
| 1004 | `RTMP_HANDSHAKE_FAILED` | RTMP 握手失败 |
| 1005 | `RTMP_STREAM_NOT_FOUND` | 流不存在 |
| 1006 | `RTMP_AUTH_FAILED` | 认证失败 |
| 2001 | `VIDEO_DECODE_FAILED` | 视频解码失败 |
| 2002 | `VIDEO_CODEC_NOT_SUPPORTED` | 不支持的视频编码 |
| 2003 | `AUDIO_DECODE_FAILED` | 音频解码失败 |
| 2004 | `AUDIO_CODEC_NOT_SUPPORTED` | 不支持的音频编码 |
| 3001 | `RENDER_INIT_FAILED` | 渲染初始化失败 |
| 3002 | `SURFACE_INVALID` | Surface 无效 |
| 4001 | `OUT_OF_MEMORY` | 内存不足 |
| 5001 | `GSTREAMER_INIT_FAILED` | GStreamer 初始化失败 |
| 5002 | `PIPELINE_CREATE_FAILED` | Pipeline 创建失败 |

`getDescription(int)` 返回中文描述。

---

## Native 接口（内部，勿直接依赖）

Java 声明于 `LatencyPlayerManager` 的 `native*` 方法（如 `nativeSetLowLatency`、`nativeSetBufferTime`、`nativeSetSurface`）为 JNI 实现细节，应通过上层 API 使用。
