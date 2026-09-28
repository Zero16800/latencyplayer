# LatencyPlayer SDK

基于 **GStreamer 1.28.7**（Android 静态库）的 Android 直播播放器 SDK。
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
| 截图 | ⚠️ | API 已接，文件写入未实现 |
| HLS / HTTP-FLV / RTSP | ❌ | 未注册对应插件 |
| WebRTC | ❌ | 不支持 |

### 协议

| 协议 | 支持 | 说明 |
|------|------|------|
| RTMP / RTMPS | ✅ | 主路径 |
| RTSP / RTSPS | ❌ | 未注册 `rtspsrc` 等插件 |
| HTTP-FLV | ❌ | 未注册 soup/http 源插件 |
| HLS | ❌ | 未注册 HLS / soup 插件 |

### 编解码

| 编码 | 视频 | 音频 |
|------|------|------|
| H.264 | ✅ openh264 + MediaCodec | - |
| H.265 / HEVC | ⚠️ 视设备，非必保 | - |
| MP3 | - | ✅ mpg123 |
| Vorbis / Opus / Speex / FLAC | - | ✅ 已注册 |
| AC3 | - | ✅ a52dec |
| AAC | - | ⚠️ 无独立软解注册，视系统硬解 |
| Theora | ✅ | - |

插件注册清单：`sdk/src/main/cpp/gstreamer_android.c`；链接白名单：`sdk/src/main/cpp/CMakeLists.txt`。

---

## 延迟

**当前状态：稳态延迟 ≥7s，为已知未解决问题**（客户端侧缓冲已压到很低，根因未定位，详见 [已知问题](#已知问题)）。

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
| `multiqueue` | `max-size-buffers=8`、`max-size-time=80ms`、`interleave-max-bytes=0` |

即**客户端侧缓冲合计已压到 ~130ms 量级**，理论端到端延迟应由服务端推流节奏决定；实测仍 ≥7s，怀疑点在 GStreamer `playbin`/`rtspsrc` 之外的延迟累积（如解码器输入队列、`sync` 时序），**待定位**。

> 首帧耗时、端到端延迟的精确计时数据待补充（需真机在环测量）。

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

## 体积（2026-09 裁剪后）

| 产物 | 裁剪前 | 现在 |
|------|--------|------|
| AAR（4 ABI） | 82 MB | **29.4 MB** |
| `liblatencyplayer.so`（arm64，strip 后） | 46.9 MB | **17.6 MB** |
| Demo APK（全 ABI） | 138 MB | **37.1 MB** |
| 接入方 APK 增量（单 arm64） | - | 约 +17.6 MB |

裁剪手段：`CMakeLists.txt` 只链接核心库白名单 + 18 个插件白名单（whole-archive 仅插件）、strip、Bionic 缺失符号桩（`vk*` / `getgrgid_r` 等）。链接完整性由 `LINKER:--no-undefined` 强制校验。

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
implementation("com.latencyplayer:latencyplayer-sdk:1.0.0")
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

版本号：`gradle.properties` 的 `LATENCYPLAYER_VERSION`（默认 `1.0.0`；Nexus `ALLOW_ONCE` 禁止同版本覆盖，升级需递增）。

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

1. **稳态延迟 ≥7s** —— 根因未定位。客户端缓冲已压至 `queue2` 50ms / `multiqueue` 80ms（合计 ~130ms），延迟累积不在客户端缓冲，待排查解码/时序环节。
2. **快速 停止→播放 切换可能崩溃** —— 流线程 caps use-after-free（teardown 与 bus 回调竞态，栈在 `gst_caps_features_set_parent_refcount` / `gst_caps_push`）。修复中；**规避：停止后等状态回到 `STOPPED` 再 `start()`**。
3. **ABI 暂时全量 4 个** —— arm64-only 精简策略待拍板（可再省约 2/3 体积）。

已修复：连点「播放」double-free（`gst_object_ref_sink`）；so 符号/链接完整性（`--no-undefined` + Bionic 桩）。

---

## 重要约束

1. **`LatencyPlayerView` 不要设置不透明背景**（例如 `android:background="#000000"`），否则 Surface 洞被盖住会黑屏；View 内部已 `setBackgroundColor(0)` + `RGBA_8888`。
2. 低延迟请同时使用 `.bufferTime(0)` 与 `.lowLatency(true)`。
3. 能力以 **RTMP** 为主；HLS / HTTP-FLV / RTSP 未开启，见 [doc/INTEGRATION.md](doc/INTEGRATION.md)。
4. 改完配置必须再调一次 `setConfig()` 才生效。

---

## 许可证

Proprietary（闭源授权）。GStreamer 及其依赖的许可证见 `NDK-CMake-GStreamer` 内附带的 LICENSE 文件。
