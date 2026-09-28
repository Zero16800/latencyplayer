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
    implementation("com.latencyplayer:latencyplayer-sdk:1.0.0")
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
player.saveSnapshot("/sdcard/screenshot.png");
```

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
| 首屏慢 | 用 `bufferTime(0)` + `lowLatency(true)` + `fastStartup(true)` |
| 连点「播放」崩溃 | 已修复（v 2026-09）；此前 video-sink 浮动引用会 double-free |
| 快速 停止→播放 崩溃 | **已知问题**：流线程 caps use-after-free，修复中；规避方式是停止后等待状态回到 STOPPED 再 start |

---

## 产物体积（2026-09）

- AAR（4 ABI）：**29.4 MB**（裁剪前 82 MB）
- arm64 `liblatencyplayer.so`：**17.6 MB**（裁剪前 46.9 MB）
- Demo APK：**37.1 MB**（全 ABI）

## 16 KB 页大小（Android 15+）

已支持：64 位 so LOAD 段 `0x4000` 对齐，`zipalign -P 16` 通过。无需调用方做任何事。
