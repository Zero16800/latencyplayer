# LatencyPlayer SDK

基于 **GStreamer 1.28.7**（Android 静态库）的 Android 直播播放器 SDK，当前以 **RTMP 拉流 + appsink 直绘** 为主路径。

## 文档索引

| 文档 | 内容 |
|------|------|
| [doc/MAVEN_INTEGRATION.md](doc/MAVEN_INTEGRATION.md) | **Maven 接入（推荐）**、完整接入代码、API 操作、功能与设备支持 |
| [doc/USAGE.md](doc/USAGE.md) | 快速接入、常用 API、低延迟配置、异常处理 |
| [doc/API.md](doc/API.md) | 类 / 方法 / 配置 / 回调 / 状态 / 错误码 |
| [doc/INTEGRATION.md](doc/INTEGRATION.md) | 环境要求、GStreamer 路径、编译 FAQ、能力范围 |

## Maven 依赖（推荐接入方式）

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

发布到 Nexus3（账密用 `-P` 传，不落库）：
```bat
gradlew :sdk:publishReleasePublicationToRemoteRepository ^
  -Platencyplayer.maven.url=http://192.168.3.160:8970/repository/maven-releases/ ^
  -Platencyplayer.maven.user=admin -Platencyplayer.maven.password=******
```
本地发布：`gradlew :sdk:publishReleasePublicationToProjectLocalRepository`
版本号：`gradle.properties` 的 `LATENCYPLAYER_VERSION`（默认 1.0.0；Nexus `ALLOW_ONCE` 禁止同版本覆盖，升级需递增）。

> 本机 `~/.gradle` 若配了全局代理，内网 Nexus 需 `systemProp.http.nonProxyHosts=192.168.*`（项目 `gradle.properties` 已配）。
> Nexus 建库/建账号/验证详见 [doc/MAVEN_INTEGRATION.md](doc/MAVEN_INTEGRATION.md)。

## 工程结构

```
E:\Android-SDK\
├── sdk/          # SDK 库模块（对外发布为 com.latencyplayer:latencyplayer-sdk）
├── app/          # Demo 应用
├── doc/          # 文档
├── gradle.properties   # GSTREAMER_ROOT_ANDROID
└── settings.gradle.kts # rootProject: LatencyPlayerSDK
```

## 环境摘要

- GStreamer Android SDK：`E:\Android-SDK\NDK-CMake-GStreamer`（版本 **1.28.7**）
- NDK：`25.1.8937393`，CMake `3.22.1`，JDK 17，AGP 8.2 / Gradle 8.2
- minSdk 21 / targetSdk 34 / compileSdk 34
- ABI：`armeabi-v7a`、`arm64-v8a`、`x86`、`x86_64`

配置（`gradle.properties`）：

```properties
GSTREAMER_ROOT_ANDROID=E\:\\Android-SDK\\NDK-CMake-GStreamer
```

## 最小示例

```java
LatencyPlayerManager player = new LatencyPlayerManager();
player.init(context.getApplicationContext());
player.setConfig(LatencyPlayerConfig.builder()
    .url("rtmp://example.com/live/stream")
    .bufferTime(0)
    .lowLatency(true)
    .fastStartup(true)
    .build());
player.setSurface(playerView); // LatencyPlayerView
player.setCallback(callback);
player.start();
```

## 体积（2026-09 裁剪后）

| 产物 | 裁剪前 | 现在 |
|------|--------|------|
| AAR（4 ABI） | 82 MB | **29.4 MB** |
| `liblatencyplayer.so`（arm64，strip 后） | 46.9 MB | **17.6 MB** |
| Demo APK（全 ABI） | 138 MB | **37.1 MB** |

裁剪手段：`CMakeLists.txt` 只链接核心库白名单 + 18 个插件白名单（whole-archive 仅插件）、strip、Bionic 缺失符号桩（`vk*` / `getgrgid_r` 等）。链接验证依赖 `LINKER:--no-undefined`。

## 真机验证

- 小米 11（`arm64-v8a`，API 34）：RTMP 720p 出画正常（appsink 直绘路径）
- 已修复：连点「播放」导致的 double-free 崩溃（video-sink 浮动引用 `gst_object_ref_sink`）

## 16 KB 页大小（Android 15+）

已支持。`arm64-v8a` / `x86_64` 的 so LOAD 段对齐 `0x4000`（16 KB），`zipalign -P 16` 校验通过；32 位 ABI 保持 `0x1000`（16 KB 只要求 64 位）。

- NDK r25 无 `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`（r27+ 才有），改用链接标志 `-Wl,-z,max-page-size=16384`
- STL 由 `c++_shared` 改 `c++_static`：NDK r25 自带的 `libc++_shared.so` 只有 4 KB 对齐，静态化后不再打入 APK
- 注意：Google Play 对 targetSdk 35+ 强制 16 KB；当前 targetSdk 34，尚不受 Play 政策约束

## 已知问题

1. **快速 停止→播放 切换可能崩溃**（流线程 caps use-after-free，teardown 与 bus 回调竞态）——修复中
2. 稳态延迟偏高（≥7s，根因未定位）
3. ABI 暂时全量 4 个（arm64-only 策略未拍板）

## 重要约束

1. **`LatencyPlayerView` 不要设置不透明背景**（例如 `android:background="#000000"`），否则 Surface 洞被盖住会黑屏；View 内部已 `setBackgroundColor(0)` + `RGBA_8888`。
2. 低延迟请同时使用 `.bufferTime(0)` 与 `.lowLatency(true)`。
3. 能力以 **RTMP** 为主；HLS / HTTP-FLV / RTSP 等未在当前静态插件注册列表中开启，见 [doc/INTEGRATION.md](doc/INTEGRATION.md)。

## 构建 Demo

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :app:assembleDebug --no-daemon
```
