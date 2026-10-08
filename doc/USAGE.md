# LatencyPlayer SDK 使用指南

## 快速开始

### 1. 添加依赖

根工程 `settings.gradle.kts` 加仓库（详见 [MAVEN_INTEGRATION.md](MAVEN_INTEGRATION.md)）：

```kotlin
maven {
    url = uri("http://192.168.3.160:8970/repository/maven-releases/")
    isAllowInsecureProtocol = true
}
```

App 模块 `build.gradle.kts`：

```kotlin
dependencies {
    implementation("com.latencyplayer:latencyplayer-sdk:1.0.7")
}
```

（同仓库源码方式：`implementation(project(":sdk"))`；离线 AAR：`implementation(files("libs/sdk-release.aar"))`）

### 2. 权限

`AndroidManifest.xml`：

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

### 3. 布局

**不要给 `LatencyPlayerView` 设置不透明背景**（`#000000` 等会导致黑屏）。

```xml
<com.latencyplayer.sdk.LatencyPlayerView
    android:id="@+id/player_view"
    android:layout_width="match_parent"
    android:layout_height="300dp" />
```

### 4. Java 代码

```java
public class MainActivity extends AppCompatActivity {

    private LatencyPlayerManager player;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        player = new LatencyPlayerManager();
        player.init(getApplicationContext());

        LatencyPlayerConfig config = LatencyPlayerConfig.builder()
                .url("rtmp://your-server/live/stream")
                .bufferTime(200)        // 或直播最低延迟用 0
                .lowLatency(false)
                .hardwareDecoder(true)
                .fastStartup(true)
                .autoReconnect(true)
                .build();

        player.setConfig(config);

        LatencyPlayerView playerView = findViewById(R.id.player_view);
        player.setSurface(playerView);

        player.setCallback(new LatencyPlayerCallback() {
            @Override
            public void onStateChanged(LatencyPlayerState state) {
                Log.d("Player", "State: " + state);
            }

            @Override
            public void onError(int errorCode, String message) {
                Log.e("Player", "Error: " + message);
            }

            @Override
            public void onFirstFrameRendered() {
                Log.d("Player", "首帧已渲染");
            }

            // 其余回调按需实现
        });

        player.start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (player != null) {
            player.release();
        }
    }
}
```

`LatencyPlayerCallback` 为完整接口，需实现全部方法（或使用适配类自行包装）。

---

## 常用功能

### 播放控制

```java
player.start();
player.stop();
player.pause();
player.resume();
```

### 音量

```java
player.setMute(true);
player.setMute(false);
player.setVolume(50);   // 0-100
boolean muted = player.isMute();
```

### 热切换 URL

```java
player.switchUrl("rtmp://new-url/live/stream2");
```

### 截图

```java
boolean ok = player.saveSnapshot("/sdcard/screenshot.png");   // PNG（.png）
boolean ok2 = player.saveSnapshot("/sdcard/screenshot.jpg");  // JPEG（.jpg/.jpeg，quality=90）
```

编码器由路径后缀决定（`pngenc` / `jpegenc`），返回是否写入成功；无画面时返回 `false`。

### 翻转 / 旋转 / 方向

```java
player.setFlip(true, false);     // 水平翻转
player.setRotation(90);          // 90°
player.setOrientation(2);        // 2=横屏（并写入 config）
```

### 运行时改缓冲

```java
player.setBufferTime(0);         // 同步更新 config + native
```

---

## 配置参数

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url("rtmp://...")
    .bufferTime(200)              // 缓冲(ms)，0=最低延迟
    .fastStartup(true)
    .lowLatency(false)            // true 时压扁 pipeline 内部队列
    .hardwareDecoder(true)
    .autoReconnect(true)
    .reconnectInterval(3)
    .maxReconnectCount(0)         // 0=无限
    .orientation(1)
    .mute(false)
    .volume(100)
    .build();
```

改完配置后重新 `player.setConfig(config)` 以便下发到 Native（至少需覆盖 bufferTime / lowLatency / mute / volume）。

---

## 回调事件

```java
player.setCallback(new LatencyPlayerCallback() {
    void onStateChanged(LatencyPlayerState state) {}
    void onError(int code, String msg) {}
    void onBuffering(int percent) {}
    void onConnected() {}
    void onDisconnected() {}
    void onFirstFrameRendered() {}
    void onDownloadSpeed(long bytesPerSec) {}
    void onReconnecting(int attemptCount) {}
    void onVideoSizeChanged(int w, int h) {}
    void onPlaybackCompleted() {}
});
```

---

## 最低延迟配置

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url(url)
    .bufferTime(0)
    .lowLatency(true)
    .fastStartup(true)
    .build();
player.setConfig(config);
player.start();
```

说明：

- `lowLatency=true` 时 Native 会尝试压扁 `queue2` / `multiqueue` / 内部队列，并缩短 playbin buffer duration。
- 直播（`rtmp://` / `rtsp://`）下 buffering **不会** 像点播那样卡住进度条逻辑。
- 端到端延迟还受 CDN / 推流端编码 GOP / 传输影响；SDK 侧主要压客户端缓冲与队列。

---

## 异常处理

```java
player.setCallback(new LatencyPlayerCallback() {
    @Override
    public void onError(int errorCode, String message) {
        switch (errorCode) {
            case LatencyPlayerError.NETWORK_CONNECT_FAILED:
                // 检查网络
                break;
            case LatencyPlayerError.RTMP_STREAM_NOT_FOUND:
                // 检查 URL
                break;
            case LatencyPlayerError.VIDEO_DECODE_FAILED:
                LatencyPlayerConfig c = player.getConfig();
                c.setHardwareDecoder(false);
                player.setConfig(c);   // 必须 setConfig 才会下发/刷新
                player.stop();
                player.start();
                break;
        }
    }
});
```

> 仅 `config.setHardwareDecoder(...)` **不会** 自动通知 Native，请再调 `setConfig`。

---

## 常见问题

| 现象 | 处理 |
|------|------|
| 黑屏但状态 PLAYING | 检查布局是否给 `LatencyPlayerView` 设了不透明 background |
| 初始化崩溃 | 确认 `GSTREAMER_ROOT_ANDROID`、ABI 与 so 已打进 APK |
| 首屏慢 | 用 `bufferTime(0)` + `lowLatency(true)` + `fastStartup(true)`。**起播首帧 ≈ 管线固定开销 ~0.3s + 等源流下一个关键帧（0~GOP，均值 GOP/2）**——实测本地 RTMP：源 GOP=1s 时首帧 avg 0.90s，源 GOP=0.2s 时 avg 0.355s；最有效手段是**缩短编码 GOP** / 服务器从关键帧快速起发（SDK 侧无法绕过关键帧等待） |
| 连点「播放」崩溃 | 已修复（v 2026-09）；此前 video-sink 浮动引用会 double-free |
| 快速 停止→播放 崩溃 | **已知问题**：流线程 caps use-after-free，修复中；规避方式是停止后等待状态回到 STOPPED 再 start（注意与「播放中停止×buffering SIGSEGV」不同，后者已修复） |
| 播放中点停止偶发 SIGSEGV（playbin `activate_group` / fault `0xaaaaaaaa`） | **已修复**：`on_buffering` 等 bus handler 与 `stop()` 已用同一把 `ctx->mutex` 序列化；压测 HLS 直播 6 + RTMP 8 次带 buffering 的停止 0 崩溃，见 README 已知问题 6 |
| 重复播放后弹「没有响应」（ANR，播放中再点播放 / 快速停止→播放） | **已修复（随 1.0.4 发布）**：流线程 `draw_frame_to_window` 曾在 appsink 回调里拿 `ctx->mutex`，与主线程 `play()` 持锁拆解旧管线互等 → 死锁 ANR；现 `native_window` 改专用 `window_mutex`，流线程不再碰 `ctx->mutex`。压测 70 次播放 0 ANR，见 README 已知问题 7 |
| 切换/停止 RTMP 流瞬间崩溃（`fault addr 0x38`，栈在 `RTMP_ReadPacket`） | **已修复（随 1.0.7 发布）**：上游 rtmpsrc 的 `unlock()` 在打断阻塞读时 `RTMP_Close` 释放了读线程仍在用的 `m_vec`；1.0.7 vendored 补丁改为 unlock 只 `shutdown` socket、Close 延后到 `stop()`。真机压测 120 轮 0 崩溃，见 README 已知问题 8 |
| HTTP-FLV / HLS 起播后无画面 | HTTP-FLV：**1.0.3 回归通过**（souphttpsrc 首读挂起未复现）；HLS 点播 + **直播均回归通过**（**1.0.3 修复**，见 README 已知问题 4） |
| RTMP 只有声音无画面 | **1.0.3 已修复**（decodebin overrun 宽限补丁，本地回归 12/12），见 README 已知问题 5 |

---

## 产物体积（2026-10）

- AAR（4 ABI）：**59.6 MB**（1.0.4，与 1.0.3 持平；1.0.2 为 58.2 MB，1.0.1 为 34.1 MB，裁剪前 82 MB）
- arm64 `liblatencyplayer.so`：**33.1 MB**（1.0.4，与 1.0.3 持平；1.0.2 为 32.3 MB，1.0.1 为 20.3 MB，裁剪前 46.9 MB）
- Demo APK：**68.7 MB**（全 ABI，1.0.4）

> 1.0.2 体积增长来自 AAC 解码（`libgstlibav` + FFmpeg 静态库）与音频输出（`libgstopensles`）。

## 16 KB 页大小（Android 15+）

已支持：64 位 so LOAD 段 `0x4000` 对齐，`zipalign -P 16` 通过。无需调用方做任何事。
