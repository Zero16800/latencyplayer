# LatencyPlayer SDK

基于 **GStreamer**（Android 静态库）的 Android 直播播放器 SDK。
主路径：**RTMP 拉流 → 软/硬解 → appsink 直绘 `ANativeWindow`**（绕过 EGL，模拟器也稳定）。

```java
LatencyPlayerManager player = new LatencyPlayerManager();
player.init(context.getApplicationContext());
player.setConfig(LatencyPlayerConfig.builder()
    .url("rtmp://example.com/live/stream")
    .bufferTime(0)          // 最低延迟
    .lowLatency(true)       // 压扁内部队列
    .fastStartup(true)
    .build());
player.setSurface(playerView);   // LatencyPlayerView
player.setCallback(callback);
player.start();
```

---

## 功能支持

| 功能 | 状态 | 说明 |
|------|------|------|
| RTMP / RTMPS 拉流直播 | ✅ | 主路径，`rtmpsrc`（librtmp），必要时 `rtmp2src` |
| appsink 直绘渲染 | ✅ | RGBx/RGBA → `ANativeWindow`，不依赖 EGL |
| 软解 / 硬解切换 | ✅ | openh264 软解 + Android MediaCodec 路径 |
| 低延迟模式 | ✅ | `bufferTime=0` + `lowLatency=true`，见下文「延迟」 |
| 断线自动重连 | ✅ | 间隔 / 次数可配，`maxReconnectCount(0)` = 无限 |
| 暂停 / 恢复 / 停止 | ✅ | |
| 热切换 URL | ✅ | `switchUrl`，不销毁 pipeline |
| 静音 / 音量 | ✅ | 0–100 |
| 翻转 / 旋转 | ✅ | 水平/垂直翻转、0/90/180/270 |
| 下载速度 / 码率回调 | ✅ | `onDownloadSpeed` |
| 首帧 / 视频尺寸回调 | ✅ | 主线程回调 |
| 截图 | ✅ | `saveSnapshot(path)` PNG / JPEG 由路径后缀决定（`pngenc` / `jpegenc`），返回是否成功；真机验证 PNG 55.9KB、JPEG 50.1KB（`FFD8…FFD9` 合法） |
| HLS / HTTP-FLV / RTSP | ✅ | 1.0.2 回归通过（RTSP / HTTP-FLV / HLS 点播 + HLS 直播 mediamtx fMP4，2/2 出画） |
| WebRTC | ❌ | 不支持 |

### 协议

| 协议 | 支持 | 说明 |
|------|------|------|
| RTMP / RTMPS | ✅ | 主路径；1.0.2 本地 mediamtx 直播回归通过（含 AAC 音轨）；修复 decodebin 过早 expose 造成的**间歇性无视频**（本地回归 12/12 稳定） |
| RTSP / RTSPS | ✅ | 1.0.1 注册 `rtspsrc` + RTP/SDP 栈，`latency=80/200ms`；1.0.2 回归通过 |
| HTTP-FLV | ✅ | 1.0.1 注册 `libsoup-3.0` + `souphttpsrc`（timeout=10s）；1.0.2 回归通过（完整播完） |
| HLS | ✅ | 1.0.1 注册 `hls` / `mpegtsdemux` / `isomp4` / `aes`；1.0.2 点播回归通过，**直播（fMP4 多 variant）回归通过**（修复见「已知问题」4） |

### 编解码

| 编码 | 视频 | 音频 |
|------|------|------|
| H.264 | ✅ openh264 + MediaCodec | - |
| H.265 / HEVC | ⚠️ 视设备，非必保 | - |
| MP3 | - | ✅ mpg123 |
| Vorbis / Opus / Speex / FLAC | - | ✅ 已注册 |
| AC3 | - | ✅ a52dec |
| AAC | - | ✅ libav `avdec_aac`（1.0.2 起注册 `libgstlibav`） |
| Theora | ✅ | - |

插件注册清单：`sdk/src/main/cpp/gstreamer_android.c`；链接白名单：`sdk/src/main/cpp/CMakeLists.txt`。

---

## 延迟

### 实测数据（小米 11 / arm64-v8a / API 34，源流 720x1280 H.264 RTMP）

| 指标 | 结果 | 计时方式 |
|------|------|---------|
| 首帧（点击播放 → 首帧渲染） | **avg 491 ms**（3 次：457 / 491 / 528 ms） | 点击的设备时刻 → logcat `First frame post` 时间戳 |
| 稳态画面延迟（相对 PC 低延迟参考） | **53–174 ms，中位 ~125 ms**（16 次有效采样） | 逐帧内容匹配，见下 |

稳态测量方法：PC 端用 `ffmpeg -fflags +nobuffer -flags low_delay` 拉同一直播流，缓存「帧到达时刻 + 128x206 灰度帧」；手机端 `screencap`（raw 格式，设备侧 `date +%s.%3N` 时间戳，采集窗口 ~200 ms）截图裁剪播放区域，与 PC 帧做 SAD 逐帧匹配。匹配质量 `best_diff ≈ 2.7`、次优帧 ≥ 4（存在明确波谷），设备/PC 时钟用 `adb shell date` 对齐（rtt ~100 ms）。

误差与边界：

- 截图采集发生在时间窗**开头**（PNG 全量编码会把窗口拉到 1.8 s，取中点即虚高 ~900 ms，已改用 raw 规避），取中点仍系统性高估 ~100 ms；
- 该值是「手机链路 − PC 参考链路」的相对量，双方网络路径与解码耗时未剥离；
- 与配置的 `queue2 50ms + multiqueue 8 缓冲上限` 同量级，互为印证。

> 早期记录的「稳态 ≥7 s」是测量假象：测量期间设备息屏（Dozing）导致截图全黑、帧匹配失效。

低延迟配置（客户端能做的都已做）：

```java
LatencyPlayerConfig.builder()
    .bufferTime(0)          // 缓冲时间 0ms（默认 200ms，范围 0–5000）
    .lowLatency(true)       // 扁平化 pipeline 内部队列
    .fastStartup(true)
    .build();
```

`lowLatency=true` 时 Native 在 pipeline 就绪 / PLAYING 等时机压扁内部队列：

| 元素 | 参数 |
|------|------|
| `queue` / `queue2` | `max-size-buffers=3`、`max-size-time=50ms`、`max-size-bytes=0` |
| `multiqueue` | `max-size-buffers=8`、`interleave-max-bytes=0`（**不压缩** `max-size-time` / `max-size-bytes`——压扁这两项会让 decodebin 过早 expose 且无法被 overrun 宽限补丁抬回，见「已知问题」修复说明） |

即**客户端侧缓冲被压到 ~100ms 量级**，与上表稳态实测同量级；端到端剩余部分由服务端推流节奏与网络决定。

---

## 设备与兼容性

| 项 | 支持范围 |
|----|---------|
| 最低系统 | Android 5.0（API 21） |
| 目标系统 | Android 14（API 34），已实测 |
| ABI | `arm64-v8a`、`armeabi-v7a`、`x86`、`x86_64` |
| 已验证机型 | 小米 11（`M2011K2C`，arm64，Android 14）：720p RTMP 出画、播放/停止/重连正常 |
| 模拟器 | x86_64 可运行（`glimagesink` 在部分模拟器有兼容问题，SDK 默认走 appsink 规避） |
| 16 KB 页设备 | ✅ 64 位 so LOAD 段 `0x4000` 对齐，`zipalign -P 16` 通过（Android 15+） |
| 32 位老设备 | ✅ 保持 4 KB 对齐（16 KB 只要求 64 位） |
| 折叠屏 / 平板 | ✅ Surface 尺寸变化自动重绑 |
| 横竖屏 | ✅ `configChanges` 内切屏不断流 |

> 其余 arm64/armv7 设备理论兼容（纯 NDK + GStreamer 静态库），建议接入后用目标机型回归首帧与重连。

---

## 体积（2026-10 裁剪后）

| 产物 | 裁剪前 | 1.0.0 | 1.0.1 | 1.0.2 |
|------|--------|-------|-------|-------|
| AAR（4 ABI） | 82 MB | 29.4 MB | 34.1 MB | **58.2 MB** |
| `liblatencyplayer.so`（arm64，strip 后） | 46.9 MB | 17.6 MB | 20.3 MB | **32.3 MB** |
| Demo APK（全 ABI） | 138 MB | 37.1 MB | 44.7 MB | **67.2 MB** |
| 接入方 APK 增量（单 arm64） | - | 约 +17.6 MB | 约 +20.3 MB | 约 +32.3 MB |

1.0.1 比 1.0.0 增加约 4.7 MB，来自新注册的插件栈：RTSP（`libgstrtsp` / `libgstsdp` / `libgstrtp` / `libgstrtpmanager`）、HTTP（`libsoup-3.0` / `libpsl` / `libnghttp2`）、HLS（`libgsthls` / `libgstmpegtsdemux` / `libgstisomp4` / `libgstaes`）、G.711 与 MP3（`libgstmulaw` / `libgstalaw` / `libgstmpg123`）。

1.0.2 比 1.0.1 增加约 24.1 MB，来自 AAC 解码（`libgstlibav`）连带的 FFmpeg 静态库（`libavcodec` / `libavutil` / `libswresample` / `libavformat` / `libavfilter` / `libswscale` / `libbz2`）与音频输出（`libgstopensles`）。

裁剪手段：`CMakeLists.txt` 只链接核心库白名单 + 34 个插件白名单（whole-archive 仅插件）、strip、Bionic 缺失符号桩（`vk*` / `getgrgid_r` 等）、`ff_localize.map` 把 FFmpeg `ff_*` 符号本地化以满足 PIC 链接。链接完整性由 `LINKER:--no-undefined` 强制校验。

---

## 接入（Maven，推荐）

```kotlin
// settings.gradle.kts -> dependencyResolutionManagement.repositories
maven {
    url = uri("http://192.168.3.160:8970/repository/maven-releases/")  // Nexus3（匿名可读）
    isAllowInsecureProtocol = true   // http 仓库必须
}
// 或开发期本地: maven { url = uri("file:///E:/Android-SDK/maven-repo") }

// app/build.gradle.kts
implementation("com.latencyplayer:latencyplayer-sdk:1.0.2")
```

发布（账密用 `-P` 传，不落库）：

```bat
rem 发到 Nexus3
gradlew :sdk:publishReleasePublicationToRemoteRepository ^
  -Platencyplayer.maven.url=http://192.168.3.160:8970/repository/maven-releases/ ^
  -Platencyplayer.maven.user=admin -Platencyplayer.maven.password=******

rem 发到项目内 maven-repo/
gradlew :sdk:publishReleasePublicationToProjectLocalRepository

rem 发到本机 ~/.m2
gradlew :sdk:publishToMavenLocal
```

版本号：`gradle.properties` 的 `LATENCYPLAYER_VERSION`（当前 `1.0.2`；Nexus `ALLOW_ONCE` 禁止同版本覆盖，升级需递增）。

> 本机 `~/.gradle` 若配了全局代理，内网 Nexus 需 `systemProp.http.nonProxyHosts=192.168.*`（项目 `gradle.properties` 已配）。
> 完整接入代码（Manifest / 布局 / MainActivity / 混淆）见 [doc/MAVEN_INTEGRATION.md](doc/MAVEN_INTEGRATION.md)。

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [doc/MAVEN_INTEGRATION.md](doc/MAVEN_INTEGRATION.md) | **Maven 接入（推荐）**、完整接入代码、API 操作、功能与设备支持 |
| [doc/USAGE.md](doc/USAGE.md) | 快速接入、常用 API、低延迟配置、异常处理 |
| [doc/API.md](doc/API.md) | 类 / 方法 / 配置 / 回调 / 状态 / 错误码 |
| [doc/INTEGRATION.md](doc/INTEGRATION.md) | 环境要求、GStreamer 路径、编译 FAQ、能力范围 |

---

## 环境摘要

- GStreamer Android SDK：`E:\Android-SDK\NDK-CMake-GStreamer`（版本 **1.28.7**）
- NDK `25.1.8937393`，CMake `3.22.1`，JDK 17，AGP 8.2 / Gradle 8.2
- minSdk 21 / targetSdk 34 / compileSdk 34
- ABI：`armeabi-v7a`、`arm64-v8a`、`x86`、`x86_64`

```properties
# gradle.properties
GSTREAMER_ROOT_ANDROID=E\:\\Android-SDK\\NDK-CMake-GStreamer
```

构建 Demo：

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :app:assembleDebug --no-daemon
```

### 工程结构

```
E:\Android-SDK\
├── sdk/          # SDK 库模块（对外发布为 com.latencyplayer:latencyplayer-sdk）
├── app/          # Demo 应用
├── doc/          # 文档
├── gradle.properties   # GSTREAMER_ROOT_ANDROID / LATENCYPLAYER_VERSION
└── settings.gradle.kts # rootProject: LatencyPlayerSDK
```

---

## 16 KB 页大小（Android 15+）

已支持。`arm64-v8a` / `x86_64` 的 so LOAD 段对齐 `0x4000`（16 KB），`zipalign -P 16` 校验通过；32 位 ABI 保持 `0x1000`（16 KB 只要求 64 位）。

- NDK r25 无 `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`（r27+ 才有），改用链接标志 `-Wl,-z,max-page-size=16384`
- STL 由 `c++_shared` 改 `c++_static`：NDK r25 自带的 `libc++_shared.so` 只有 4 KB 对齐，静态化后不再打入 APK
- 注意：Google Play 对 targetSdk 35+ 强制 16 KB；当前 targetSdk 34，尚不受 Play 政策约束

---

## 已知问题

1. **快速 停止→播放 切换可能崩溃** —— 流线程 caps use-after-free（teardown 与 bus 回调竞态，栈在 `gst_caps_features_set_parent_refcount` / `gst_caps_push`）。修复中；**规避：停止后等状态回到 `STOPPED` 再 `start()`**。
2. **ABI 暂时全量 4 个** —— arm64-only 精简策略待拍板（可再省约 2/3 体积）。
3. **测量时设备必须常亮** —— 息屏（Dozing）会让 Surface 停止渲染，截图全黑、延迟测量失效；已知问题曾因此被误判为「稳态延迟 ≥7s」。
4. ~~**HLS 直播（fMP4 多 variant）片段下载异常（1.0.2 新发现）**~~ —— **已修复并回归通过（2/2 出画）**：根因是低延迟 `queue` tune 对**原始容器字节流**设置 `leaky`，丢字节导致 `qtdemux` 报 `atom bogus size` → `adaptivedemux` 反复重启。修复：`queue_carries_raw_bytes()` 沿 sink pad 上游判断数据是否仍为 raw 容器字节（Demuxer/Parser 之前），raw 流**不设 leaky**。HTTP-FLV 首读挂起问题已在 1.0.2 回归中消除。
5. **RTMP 本地间歇性无视频（1.0.2 新发现，已修复）** —— 根因链：`multiqueue` 过早 overrun → `decodebin` expose 仅音频的群组 → flvdemux 线程阻塞在 `gst_data_queue_push()` 无法解析视频 tag → 迟到的 video pad 因「No current group」被永久丢弃。修复（vendored `gstdecodebin2.c` 补丁）：overrun 时先抬高 multiqueue 限额解阻 demuxer，再以 2500ms 宽限等待 video pad 加入后连同视频一起 expose；低延迟 tune 不再压缩 `multiqueue` 的 `max-size-time` / `max-size-bytes`（否则 async-done 会把抬限压回，demuxer 反复阻塞）。本地 mediamtx RTMP 回归 **12/12 稳定**（含宽限路径与自然完成路径两种时序的机制日志证据）。

已修复：稳态延迟误判（实测 ~125ms，见 [延迟](#延迟)）；连点「播放」double-free（`gst_object_ref_sink`）；so 符号/链接完整性（`--no-undefined` + Bionic 桩）；HLS 直播 raw 字节泄漏；RTMP 间歇性无视频；截图文件写入（PNG/JPEG）。

---

## 重要约束

1. **`LatencyPlayerView` 不要设置不透明背景**（例如 `android:background="#000000"`），否则 Surface 洞被盖住会黑屏；View 内部已 `setBackgroundColor(0)` + `RGBA_8888`。
2. 低延迟请同时使用 `.bufferTime(0)` 与 `.lowLatency(true)`。
3. 能力以 **RTMP** 为主；RTSP / HTTP-FLV / HLS（点播 + 直播）已在 1.0.2 回归通过，生产请优先 RTMP，见 [doc/INTEGRATION.md](doc/INTEGRATION.md)。
4. 改完配置必须再调一次 `setConfig()` 才生效。

---

## 许可证

Proprietary（闭源授权）。GStreamer 及其依赖的许可证见 `NDK-CMake-GStreamer` 内附带的 LICENSE 文件。
