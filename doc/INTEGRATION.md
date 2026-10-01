# LatencyPlayer SDK 集成指南

## 1. 环境要求

- Android Studio 2022.3+（或命令行 Gradle）
- Android SDK API 21+
- Android NDK **r25**（本仓库验证：`25.1.8937393`）
- CMake **3.22.1+**
- JDK **17**（本仓库验证）
- **GStreamer Android SDK 1.28.7**（静态库 / universal 按 ABI 分目录）

## 2. GStreamer Android SDK

### 2.1 本仓库路径

当前工程使用：

```text
GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
```

目录下按 ABI 划分，例如：

```text
NDK-CMake-GStreamer\
  armeabi-v7a\
  arm64-v8a\
  x86\
  x86_64\
```

### 2.2 配置位置

`gradle.properties`（推荐）：

```properties
GSTREAMER_ROOT_ANDROID=E\:\\Android-SDK\\NDK-CMake-GStreamer
```

或环境变量 `GSTREAMER_ROOT_ANDROID`。`sdk/build.gradle.kts` 会把该属性传给 CMake 的 `-DGSTREAMER_ROOT_ANDROID`。

官方下载：https://gstreamer.freedesktop.org/download/

## 3. 集成 SDK

### 3.1 依赖

**方式 A：Maven（推荐，见 [MAVEN_INTEGRATION.md](MAVEN_INTEGRATION.md)）**

```kotlin
implementation("com.latencyplayer:latencyplayer-sdk:1.0.1")
```

**方式 B：本地 AAR**

`app/build.gradle.kts`：

```kotlin
dependencies {
    implementation(files("libs/sdk-release.aar"))
}
```

### 3.2 权限

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

### 3.3 ABI

`sdk` 模块 `abiFilters`：

- `armeabi-v7a`
- `arm64-v8a`
- `x86`
- `x86_64`

## 4. 使用方法

### 4.1 布局（勿设不透明背景）

```xml
<com.latencyplayer.sdk.LatencyPlayerView
    android:id="@+id/player_view"
    android:layout_width="match_parent"
    android:layout_height="wrap_content" />
```

**禁止** `android:background="#000000"` 等会挡住 Surface 的背景，否则黑屏。

### 4.2 初始化与配置

```java
LatencyPlayerManager player = new LatencyPlayerManager();
player.init(getApplicationContext());

LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url("rtmp://your-server/live/stream")
    .bufferTime(200)
    .fastStartup(true)
    .hardwareDecoder(true)
    .autoReconnect(true)
    .reconnectInterval(3)
    .build();
player.setConfig(config);
```

### 4.3 Surface 与回调

```java
player.setSurface(playerView);
player.setCallback(callback);
```

### 4.4 控制

```java
player.start();
player.pause();
player.resume();
player.stop();
player.setMute(true);
player.setVolume(50);
player.switchUrl("rtmp://new-url");
player.saveSnapshot("/sdcard/screenshot.png");
```

### 4.5 释放

```java
@Override
protected void onDestroy() {
    super.onDestroy();
    player.release();
}
```

## 5. 高级配置

### 5.1 超低延迟

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url(url)
    .bufferTime(0)
    .lowLatency(true)
    .fastStartup(true)
    .build();
player.setConfig(config);
```

`lowLatency=true` 时 Native 在 pipeline 就绪 / PLAYING 等时机压扁内部 `queue2`、`multiqueue`、队列元素，缩短 playbin 缓冲。

### 5.2 无限重连

```java
.autoReconnect(true)
.reconnectInterval(3)
.maxReconnectCount(0)
```

### 5.3 横屏

```java
.orientation(2)
```

## 6. 项目结构

```
LatencyPlayerSDK/
├── sdk/                 # SDK 模块
│   ├── src/main/java/   # Java API
│   ├── src/main/cpp/    # Native (latency_player_*.c, gstreamer_android.c)
│   └── build.gradle.kts
├── app/                 # Demo
├── doc/                 # 文档
├── gradle.properties
└── settings.gradle.kts
```

## 7. 编译说明

### 7.1 首次编译

1. 确认 GStreamer 1.28.7 目录与 ABI 子目录齐全  
2. 配置 `GSTREAMER_ROOT_ANDROID`  
3. Sync Gradle / 执行 `gradlew :app:assembleDebug`  

命令行示例：

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :app:assembleDebug --no-daemon
```

### 7.2 16 KB 页大小（2026-09 已支持）

背景：Android 15+ 支持 16 KB 页设备，Google Play 要求 targetSdk 35+ 的应用 64 位 so 必须 16 KB 对齐（2025-11 起）。

本项目做法（NDK r25 / AGP 8.2 现状下）：

1. **链接标志**（`CMakeLists.txt`，仅 64 位 ABI）：
   ```cmake
   -Wl,-z,max-page-size=16384
   -Wl,-z,common-page-size=16384
   ```
   NDK r27+ 可改用 `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`，r28+ 默认开启。
2. **STL 改 `c++_static`**：NDK r25 的 `libc++_shared.so` 是 4 KB 对齐，会拖累整包；SDK 源码纯 C（openh264.a 为 C++，需显式链 `c++_static c++abi`），静态化后 APK 不再含该 so。
3. **校验**：
   ```bat
   llvm-readelf -lW liblatencyplayer.so   :: LOAD 段 align 需 0x4000（64位）
   build-tools\35.0.0\zipalign -c -P 16 -v 4 app.apk   :: 需 Verification successful
   ```

达标情况：`arm64-v8a`/`x86_64` LOAD align=`0x4000`；32 位保持 `0x1000`（16 KB 只要求 64 位）；zipalign 通过；小米 11 真机出画回归通过。

> 升级路径：未来切 NDK r28+ / AGP 8.5.1+ 后可删掉手写链接标志（r28 默认 16 KB，AGP 8.5.1+ 默认按 16 KB zip 对齐非压缩 so）。

### 7.3 体积裁剪（2026-09 起，1.0.2 更新）

Native 链接策略（`sdk/src/main/cpp/CMakeLists.txt`）：

- 核心库与插件走**白名单**（`CORE_ALLOW` / `PLUGIN_ALLOW` 31 项），未列入的一律不链
- 库必须经 `target_link_libraries`（放在 `.o` 之后）；`link_options`/`@rsp` 会排在 `.o` 前面导致 archive 拉不进符号
- 库名匹配用 `NAME` + 去 `.a` 后缀（勿用 `NAME_WE`，会把 `libgstreamer-1.0.a` 截成 `libgstreamer-1`）
- `LINKER:--no-undefined` 强制链接完整，未解析符号会在构建期报出
- Bionic 缺失符号补桩（`gstreamer_stubs.c`）：`in6addr_any/loopback`、`getgrgid_r`、`fseeko64/ftello64`(32 位)、`__gnu_strerror_r`、84 个 `vk*`
- FFmpeg 静态库（`libgstlibav` 连带）部分目标文件非 PIC，靠 `ff_localize.map` 版本脚本（`local: ff_*;`）把符号本地化后才能进 `.so`（否则 arm64 报 `R_AARCH64_ADR_PREL_PG_HI21 ... recompile with -fPIC`、x86_64 报 `R_X86_64_PC32`）

结果：AAR 82→29.4 MB（1.0.0）→ 34.1 MB（1.0.1）→ **58.2 MB（1.0.2）**，arm64 so 46.9→17.6→20.3→**32.3 MB**（strip 后）。

> `in6addr_*` 桩不能 include `<netinet/in.h>`（头文件里声明为 static），需自定义结构体。

### 7.4 发布链路（Maven，当前主链路）

SDK 以 Maven 坐标 `com.latencyplayer:latencyplayer-sdk:<version>` 发布，Demo 已改为坐标依赖（`app/build.gradle.kts` → `implementation("com.latencyplayer:latencyplayer-sdk:1.0.2")`）。

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer

:: 1) 发布到本地仓库 E:\Android-SDK\maven-repo（默认，settings.gradle.kts 已加该仓库）
gradlew.bat :sdk:publishReleasePublicationToProjectLocalRepository --no-daemon

:: 2) 发布到远程仓库（gradle.properties 先配 latencyplayer.maven.url/user/password）
gradlew.bat :sdk:publishReleasePublicationToRemoteRepository --no-daemon

:: 3) 构建 Demo（版本变了建议 :app:clean 防止依赖缓存旧版）
gradlew.bat :app:clean :app:assembleDebug --no-daemon
```

版本号在 `gradle.properties` 的 `LATENCYPLAYER_VERSION`（当前 1.0.2）。接入方完整配置见 [MAVEN_INTEGRATION.md](MAVEN_INTEGRATION.md)。

> 旧的 `implementation(files("libs/sdk-release.aar"))` 离线方式仍可用：手动拷 `sdk\build\outputs\aar\sdk-release.aar` 到 `app\libs\`，AAR 变了必须 `:app:clean` 否则命中 UP-TO-DATE 打进旧包。

### 7.5 常见问题

**Could not GET 内网 Nexus ... Read timed out**

`~/.gradle/gradle.properties` 里的全局代理（如 `127.0.0.1:1080`）把内网仓库请求也转发了。项目 `gradle.properties` 已加：

```properties
systemProp.http.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1
systemProp.https.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1
```

**400 Repository does not allow updating assets**

Nexus `maven-releases` 是 `ALLOW_ONCE`，同版本禁止覆盖：升 `gradle.properties` 的 `LATENCYPLAYER_VERSION` 再发。

**找不到 gstreamer / 头文件**

检查 `GSTREAMER_ROOT_ANDROID` 是否指向含 `armeabi-v7a` 等 ABI 的父目录。

**UnsatisfiedLinkError**

确认 APK 内含对应 ABI 的 `liblatencyplayer.so` 及链接的 GStreamer 静态库已正确参与 CMake。

**播放无画面**

1. 布局是否误设了 `LatencyPlayerView` 不透明背景  
2. 是否已 `setSurface`  
3. logcat 中是否有 `Video size:`、首帧 / PLAYING 日志  

**播放无声音 / 缺编解码**

见下文能力表；插件走白名单（`PLUGIN_ALLOW` 31 项），未列入者不链。

## 8. 协议支持（当前实际）

以 `sdk/src/main/cpp/gstreamer_android.c` **已 STATIC_REGISTER** 的插件为准：

| 协议 | 支持 | 说明 |
|------|------|------|
| RTMP / RTMPS | ✅ | 主路径；优先 `rtmpsrc`（librtmp），必要时 `rtmp2src`。1.0.2 本地 mediamtx 直播回归通过（H.264 + AAC，opensles 出声） |
| RTSP / RTSPS | ✅ | 1.0.1 注册 `rtspsrc` + `rtp`/`rtpmanager` + `gstrtsp`/`gstsdp`；`latency=80ms`（lowLatency）/ `200ms`。1.0.2 回归通过（First frame + PLAYING） |
| HTTP-FLV | ✅ | 1.0.1 注册 `libsoup-3.0`（`souphttpsrc`，`timeout=10s`）。1.0.2 回归通过（完整播完，首次读挂起未复现） |
| HLS | ⚠️ | 1.0.1 注册 `hls` / `mpegtsdemux` / `isomp4` / `aes` + `uridownloader`/`adaptivedemux`。1.0.2 点播回归通过；**直播（fMP4 多 variant）见下方已知问题 5** |
| WebRTC | ❌ | 非本 SDK 能力；纯客户端 RTMP→WebRTC 不可行 |

`tcp` 插件已注册，但**不等于** 已支持完整 HTTP 播放栈。

## 9. 编解码（当前实际）

| 编码 | 视频 | 音频 |
|------|------|------|
| H.264 | ✅ openh264 + Android 媒体解码路径 | - |
| H.265/HEVC | ⚠️ 视设备 / 官方路径；非必保 | - |
| MP3 | - | ✅ mpg123 |
| Vorbis / Opus / Speex / FLAC | - | ✅ 已注册相关解码插件 |
| AC3 | - | ✅ a52dec |
| AAC | - | ✅ libav `avdec_aac`（1.0.2 注册 `libgstlibav`） |
| Theora | ✅ theora | - |

插件注册列表见 `gstreamer_android.c`；链接集合见 `sdk/src/main/cpp/CMakeLists.txt`（白名单制：`CORE_ALLOW` + `PLUGIN_ALLOW`，无排除清单）。解码器 rank：`openh264dec = PRIMARY+1` 优先于 `avdec_h264 = MARGINAL`（1.0.2 起；曾因 avdec_h264 抢占导致 HLS/TS 解码报 `decode_slice_header error`）。

## 10. 渲染路径（摘要）

1. `playbin`，`video-sink` 走 **appsink**（`RGBx/RGBA`，`sync=FALSE`，小缓冲）  
2. 回调取 sample → `ANativeWindow` 直绘  
3. Surface 尺寸变化时按需 `setBuffersGeometry`（防重复调用）  
4. `glimagesink`/EGL 在部分模拟器上不可靠，仅作回退  

## 11. 已知问题（2026-10 更新）

1. **快速 停止→播放 可能崩溃**：流线程 `multiqueue:src` caps use-after-free（teardown 与 bus 回调 `on_buffering`/`on_async_done` 竞态，栈在 `gst_caps_features_copy`）。修复中；规避：停止后等状态回 `STOPPED` 再 `start()`。
2. **测量时设备必须常亮**：息屏（Dozing）会让 Surface 停止渲染、截图全黑，延迟测量失效（早期「稳态延迟 ≥7s」即由此误判；真机实测首帧 avg 491ms、稳态画面延迟中位 ~125ms，详见 [README 延迟](../README.md#延迟)）。
3. **ABI 全量 4 个**：arm64-only 精简策略待拍板（可再省约 2/3 体积）。
4. ~~**HTTP-FLV 首次 body 读可能挂起（1.0.1 待验证）**~~ —— 1.0.2 回归通过：`souphttpsrc timeout=10` + src pad buffer 探针生效，HTTP-FLV 完整播完，未复现挂起。
5. **HLS 直播（fMP4 多 variant）片段下载异常（1.0.2 新发现）**：First frame 能出，随后 `qtdemux` 报 `atom bogus size`（收到非 MP4 数据）、`adaptivedemux` 反复 `Error while downloading fragment` 重启。定位排除项：服务端正常（ffmpeg 消费同一 mediamtx live HLS 无异常）、HLS 点播回归通过（基础 hlsdemux/分片链路无回归）、走 LAN IP 与 adb reverse 均复现（非转发层问题）。待查方向：`hlsdemux/adaptivedemux` 对 mediamtx 直播 fMP4 双 variant（video/audio 分离 variant + `?session=` 查询参数）的兼容性。

已修复：稳态延迟误判（实测 ~125ms）；连点「播放」double-free（`gst_object_ref_sink`）；so 符号/链接完整性（`--no-undefined` + Bionic 桩）；AAC 解码缺失（1.0.2 注册 `libgstlibav`）；播放无音频输出（1.0.2 注册 `libgstopensles`，链接系统 `OpenSLES`）；`avdec_h264` 抢占 openh264 导致的解码错误（1.0.2 调 rank）。

## 12. 许可证

- LatencyPlayer SDK 示例代码：可按项目约定使用  
- GStreamer 核心多为 LGPL，部分插件 GPL/其它许可 — 分发前请按所链插件核对合规  
- **注意**：1.0.2 起新链的 `libgstlibav`（gst-libav，FFmpeg 封装）为 **GPL-2.0**，连带 `libavcodec`/`libavutil` 等 FFmpeg 库（LGPL 2.1+，含汇编按其许可证）；闭源分发前需评估 GPL 传染性或以独立进程/动态加载隔离，必要时可从 `PLUGIN_ALLOW` 移除 `libgstlibav`（代价：回到无 AAC 软解）

