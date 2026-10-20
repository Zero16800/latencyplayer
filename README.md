# 蓝汐播放器 SDK

> English docs: [README.en.md](README.en.md)。

[蓝汐播放器](https://github.com/Zero16800/latencyplayer/blob/main/website/334.png)



堪比大牛SDK的Android 直播播放器 SDK。
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

> **iOS**：轻量原生实现（librtmp + VideoToolbox + AVSampleBufferDisplayLayer），
> API 与 Android 对齐，交付 XCFramework + Swift Package。见
> [`ios/README.md`](ios/README.md)。当前 iOS 为**首版脚手架**，视频链路已实现，**音频待实现**。

---

## 功能支持

| 功能 | 状态 | 说明 |
|------|------|------|
| RTMP / RTMPS 拉流直播 | ✅ | 主路径，`rtmpsrc`（librtmp），必要时 `rtmp2src` |
| appsink 直绘渲染 | ✅ | RGBx/RGBA → `ANativeWindow`，不依赖 EGL |
| 软解 / 硬解切换 | ✅ | openh264 软解 + Android MediaCodec 路径 |
| 低延迟模式 | ✅ | `bufferTime(0)` + `lowLatency(true)`，见下文「延迟」 |
| 断线自动重连 | ✅ | 间隔 / 次数可配，`maxReconnectCount(0)` = 无限 |
| 后台返回自恢复 | ✅ | 退后台 / 被系统冻结返回后，若视频停止流动 >3s 自动重连恢复（`getLastFrameAgeMs` + `ReconnectManager` 卡死看门狗） |
| 预连接（毫秒级切流） | ✅ | `preconnect(url)` hold 模式——后台建链 + 解码首帧 + 吃掉 CDN 突发积压，同 URL 播放直接出画；双实例切换点击→首帧 **avg 173ms** |
| 暂停 / 恢复 / 停止 | ✅ | |
| 热切换 URL | ✅ | `switchUrl`，不销毁 pipeline |
| 静音 / 音量 | ✅ | 0–100 |
| 翻转 / 旋转 | ✅ | 水平/垂直翻转、0/90/180/270 |
| 下载速度 / 码率回调 | ✅ | `onDownloadSpeed` |
| 首帧 / 视频尺寸回调 | ✅ | 主线程回调 |
| 截图 | ✅ | `saveSnapshot(path)` PNG / JPEG 由路径后缀决定（`pngenc` / `jpegenc`），返回是否成功；真机验证 PNG 55.9KB、JPEG 50.1KB（`FFD8…FFD9` 合法） |
| 视频帧年龄查询 | ✅ | `getLastFrameAgeMs()`：最近帧年龄（ms），供上层做卡死判断/自定义看门狗 |
| HLS / HTTP-FLV / RTSP | ✅ | RTSP / HTTP-FLV / HLS 点播 + HLS 直播（fMP4 多 variant）回归通过 |
| WebRTC | ❌ | 不支持 |

### 协议

| 协议 | 支持 | 说明 |
|------|------|------|
| RTMP / RTMPS | ✅ | 主路径；本地 mediamtx 直播回归通过（含 AAC 音轨）；CDN 开播突发灌入的积压由 appsink `sync=FALSE` + 追帧丢弃内置处理（见「延迟 → CDN 突发积压」）；停播/切换瞬间的读线程竞态已修复（vendored `gstrtmpsrc` 补丁，真机压测 0 崩溃） |
| RTSP / RTSPS | ✅ | 注册 `rtspsrc` + RTP/SDP 栈，`latency=80/200ms`，回归通过 |
| HTTP-FLV | ✅ | 注册 `libsoup-3.0` + `souphttpsrc`（timeout=10s），回归通过 |
| HLS | ✅ | 注册 `hls` / `mpegtsdemux` / `isomp4` / `aes`；点播 + 直播（fMP4 多 variant）回归通过 |

### 编解码

| 编码 | 视频 | 音频 |
|------|------|------|
| H.264 | ✅ openh264 + MediaCodec | - |
| H.265 / HEVC | ⚠️ 视设备，非必保 | - |
| MP3 | - | ✅ mpg123 |
| Vorbis / Opus / Speex / FLAC | - | ✅ 已注册 |
| AC3 | - | ✅ a52dec |
| AAC | - | ✅ libav `avdec_aac` |
| Theora | ✅ | - |

插件注册清单：`android/sdk/src/main/cpp/gstreamer_android.c`；链接白名单：`android/sdk/src/main/cpp/CMakeLists.txt`。

---

## 延迟

### 实测数据（小米 11 / arm64-v8a / API 34，源流 720x1280 H.264 RTMP）

| 指标 | 结果 | 计时方式 |
|------|------|---------|
| 首帧（点击播放 → 首帧渲染；普通冷启动，未预连接） | **avg 491 ms**（3 次：457 / 491 / 528 ms） | 点击的设备时刻 → logcat `First frame post` 时间戳 |
| 切换点击 → 新流首帧（双实例预连接） | **avg 173 ms**（8 连切：37 / 172–228 ms） | demo `TIMING first-frame` |
| 稳态画面延迟（相对 PC 低延迟参考） | **53–174 ms，中位 ~125 ms**（16 次有效采样） | 逐帧内容匹配，见下 |

稳态测量方法：PC 端用 `ffmpeg -fflags +nobuffer -flags low_delay` 拉同一直播流，缓存「帧到达时刻 + 128x206 灰度帧」；手机端 `screencap`（raw 格式，设备侧 `date +%s.%3N` 时间戳，采集窗口 ~200 ms）截图裁剪播放区域，与 PC 帧做 SAD 逐帧匹配。匹配质量 `best_diff ≈ 2.7`、次优帧 ≥ 4（存在明确波谷），设备/PC 时钟用 `adb shell date` 对齐（rtt ~100 ms）。

误差与边界：

- 截图采集发生在时间窗**开头**（PNG 全量编码会把窗口拉到 1.8 s，取中点即虚高 ~900 ms，已改用 raw 规避），取中点仍系统性高估 ~100 ms；
- 该值是「手机链路 − PC 参考链路」的相对量，双方网络路径与解码耗时未剥离；
- 与配置的 `queue2 50ms + multiqueue 8 缓冲上限` 同量级，互为印证。

> 息屏/Dozing 时 Surface 停止渲染、截图全黑，延迟测量会失效——测量必须保持设备常亮。

### CDN 突发积压（内置处理）

CDN（如阿里云）在 RTMP 建连瞬间会把最近约 **2.5s 的历史视频一次性突发下发**（实测初始速度约 10× 实时码率）。处理方式：

- appsink 用 `sync=FALSE`（队列按解码速度排空、贴到直播边缘，而非按 1× 逐帧排空）；
- appsink 回调按「直播边缘 − 当前帧 PTS」做**追帧丢弃**（滞后 >600ms 进入追赶并丢弃旧帧、<200ms 退出），只跳帧、不渲染旧帧，避免「快进」观感。

效果：播放器侧积压 `pipe_backlog` **40–200ms**，追平用时约 0.2–0.3s。日志证据：`catchup: START drop (lag=2361ms)` → `catchup: STOP (lag=199ms)` → `First frame post=1`。

### 预连接毫秒级切流（1.0.15）

`preconnect(url)`（hold 模式）把“RTMP 建连 + CDN 突发下发 + 突发积压消耗 + 首个关键帧软解（~300–500ms）”全部挪到点击**之前**的后台完成：

- `preconnect()` 后台管线直达 PLAYING 并持续消费流，但**不向 Surface 出画、不发 FIRST_FRAME**（样本静默丢弃，追帧丢弃逻辑同期把落后消化到直播边缘）；
- 同 URL 的 `start()` 只需放行（hold=FALSE），下一个解码帧（贴直播边的新帧）即刻出画；
- `preconnect()` 自身 ~200–400ms 返回，不阻塞调用线程；每个预连接占用一条 RTMP 连接 + 一个软解码器。

实现依据（为什么不能“停在 PAUSED”）：playbin 的 `ASYNC→PAUSED` 可仅由**音频链**完成状态迁移——此时 video 分支（input-selector/appsink）尚未建立，decodebin 要等首个关键帧**解码完成**才 expose 视频口。因此预连接必须开到 PLAYING 让视频链在后台建完，`play()` 才拿得到现成帧。

实测（小米 11，双实例 demo，两路阿里云 RTMP 720x1280 H.264 启动时各预热一路、切换时角色互换）：

| 指标 | 结果 |
|------|------|
| 切换点击 → 新流首帧 | **avg 173 ms**（8 连切：37 / 172–228 ms） |
| 启动首点（走预热） | 37–96 ms |
| 未预热的普通切换（对照） | ~1000–1135 ms |

### 低延迟配置

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
| `multiqueue` | `max-size-buffers=8`、`interleave-max-bytes=0`（**不压缩** `max-size-time` / `max-size-bytes`——压扁这两项会让 decodebin 过早 expose 且无法被 overrun 宽限补丁抬回） |

即**客户端侧缓冲被压到 ~100ms 量级**，与上表稳态实测同量级；端到端剩余部分由服务端推流节奏与网络决定。

---

## 后台与生命周期

| 场景 | 行为 |
|------|------|
| 退到后台（Home）短时间返回 | Surface 重建后自动重绑，画面立即接上 |
| 退到后台较久返回（进程被系统/MIUI 冻结） | 恢复后若视频帧已停止流动 >3s，`ReconnectManager` 卡死看门狗自动重连，实测返回后 ~750ms 出画 |
| 按**返回键**真退出 → 重开 | 当前 Demo 在 `onDestroy` 调 `release()` 销毁播放器，重开是全新实例、**不会自动播放**（需点「播放」）。如需自动恢复需业务侧持久化 URL 并重开自动 `start()`，或把播放放到前台服务 |

> 真正做到**息屏/锁屏后台继续播**（不只是返回自恢复）需要前台服务（foreground service）避免系统冻结后台进程——当前 SDK 未内置。

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

## 体积

| 产物 | 大小 |
|------|------|
| AAR（4 ABI） | **59.6 MB** |
| `liblatencyplayer.so`（arm64，strip 后） | 33.1 MB |
| Demo APK（全 ABI） | 68.7 MB |
| 接入方 APK 增量（单 arm64） | 约 +33.1 MB |

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

// android/app/build.gradle.kts
implementation("com.latencyplayer:latencyplayer-sdk:1.0.15")
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

版本号：`gradle.properties` 的 `LATENCYPLAYER_VERSION`（当前 `1.0.15`）。

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
| [ios/README.md](ios/README.md) | **iOS SDK**：架构、XCFramework/SPM 构建、Android⇄iOS API 映射、状态与 TODO |
| [android/README.md](android/README.md) | **Android 平台**：模块结构、构建/发布入口、Native 目录、文档导航 |

> 英文版：根 [README.en.md](README.en.md)、[doc/*.en.md](doc)、[android/README.en.md](android/README.en.md)。

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
├── android/           # Android 平台目录（Gradle 模块；ios/ 规划中）
│   ├── sdk/           # SDK 库模块（对外发布为 com.latencyplayer:latencyplayer-sdk）
│   └── app/           # Demo 应用
├── doc/               # 文档
├── gradle.properties  # GSTREAMER_ROOT_ANDROID / LATENCYPLAYER_VERSION
└── settings.gradle.kts # rootProject: LatencyPlayerSDK（:sdk/:app 映射到 android/）
```

---

## 16 KB 页大小（Android 15+）

已支持。`arm64-v8a` / `x86_64` 的 so LOAD 段对齐 `0x4000`（16 KB），`zipalign -P 16` 校验通过；32 位 ABI 保持 `0x1000`（16 KB 只要求 64 位）。

- NDK r25 无 `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`（r27+ 才有），改用链接标志 `-Wl,-z,max-page-size=16384`
- STL 由 `c++_shared` 改 `c++_static`：NDK r25 自带的 `libc++_shared.so` 只有 4 KB 对齐，静态化后不再打入 APK
- 注意：Google Play 对 targetSdk 35+ 强制 16 KB；当前 targetSdk 34，尚不受 Play 政策约束

---

## 已知问题

1. **快速 停止→播放 切换可能崩溃** —— 流线程 caps use-after-free（teardown 与 bus 回调竞态，栈在 `gst_caps_features_set_parent_refcount` / `gst_caps_push`）。**规避：停止后等状态回到 `STOPPED` 再 `start()`**。
2. **ABI 暂时全量 4 个** —— arm64-only 精简策略待拍板（可再省约 2/3 体积）。
3. **测量时设备必须常亮** —— 息屏（Dozing）会让 Surface 停止渲染，截图全黑、延迟测量失效。

---

## 重要约束

1. **`LatencyPlayerView` 不要设置不透明背景**（例如 `android:background="#000000"`），否则 Surface 洞被盖住会黑屏；View 内部已 `setBackgroundColor(0)` + `RGBA_8888`。
2. 低延迟请同时使用 `.bufferTime(0)` 与 `.lowLatency(true)`。
3. 能力以 **RTMP** 为主；RTSP / HTTP-FLV / HLS（点播 + 直播）回归通过，生产请优先 RTMP，见 [doc/INTEGRATION.md](doc/INTEGRATION.md)。
4. 改完配置必须再调一次 `setConfig()` 才生效。
5. 每个预连接实例占用一条 RTMP 连接 + 一个软解码器，用完用 `stop()` / `release()` 释放。

---

## 许可证

Proprietary（闭源授权）。GStreamer 及其依赖的许可证见 `NDK-CMake-GStreamer` 内附带的 LICENSE 文件；注意为解码 AAC 链接的 `libgstlibav` 为 GPL-2.0（见 [doc/INTEGRATION.md](doc/INTEGRATION.md) 第 12 节）。
