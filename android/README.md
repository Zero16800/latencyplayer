# LatencyPlayer SDK — Android 平台

> English docs: [README.en.md](README.en.md)。

本目录是 LatencyPlayer SDK 的 **Android 平台实现**（Gradle 模块）。整体功能、延迟数据、版本历史见仓库根 [`README.md`](../README.md)；接口与接入细节见 [`doc/`](../doc)。

## 模块结构

`settings.gradle.kts`（仓库根）把 `:sdk` / `:app` 映射到本目录：

```
android/
├── sdk/                     # SDK 库模块 → 发布为 com.latencyplayer:latencyplayer-sdk
│   ├── src/main/java/com/latencyplayer/sdk/     # 公开 API
│   │   ├── LatencyPlayerManager.java            # 播放器核心管理类
│   │   ├── LatencyPlayerConfig.java             # Builder 配置
│   │   ├── LatencyPlayerCallback.java           # 回调接口
│   │   ├── LatencyPlayerState.java              # 状态枚举
│   │   ├── LatencyPlayerError.java              # 错误码
│   │   ├── LatencyPlayerView.java               # SurfaceView 渲染 View
│   │   └── internal/                            # GStreamerInitializer / ReconnectManager
│   ├── src/main/cpp/                            # Native（NDK）
│   │   ├── latency_player_core.c / .h           # 状态机、pipeline、追帧、看门狗
│   │   ├── latency_player_jni.c                 # JNI 桥接
│   │   ├── gstreamer_android.c                  # 插件 STATIC_REGISTER
│   │   ├── gstreamer_stubs.c                    # Bionic 缺失符号桩
│   │   ├── CMakeLists.txt                       # 链接白名单 / 16KB 对齐
│   │   ├── ff_localize.map                      # FFmpeg ff_* 符号本地化
│   │   └── vendor/rtmp/                         # vendored gstrtmpsrc 补丁
│   └── build.gradle.kts
└── app/                     # Demo 应用
    ├── src/main/java/com/latencyplayer/demo/    # MainActivity 等
    ├── src/main/res/                            # 布局 / 主题
    └── build.gradle.kts
```

## 环境

- GStreamer Android SDK **1.28.7**：`E:\Android-SDK\NDK-CMake-GStreamer`（`gradle.properties` 的 `GSTREAMER_ROOT_ANDROID`）
- NDK `25.1.8937393`、CMake `3.22.1`、JDK 17、AGP 8.2 / Gradle 8.2
- minSdk 21 / targetSdk 34 / compileSdk 34；ABI：`armeabi-v7a`、`arm64-v8a`、`x86`、`x86_64`
- 版本号：`gradle.properties` 的 `LATENCYPLAYER_VERSION`（当前 `1.0.13`）

## 构建 Demo

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :app:assembleDebug --no-daemon
```

## 发布 SDK（Maven）

```bat
gradlew.bat :sdk:publishReleasePublicationToProjectLocalRepository --no-daemon
gradlew.bat :sdk:publishReleasePublicationToRemoteRepository --no-daemon
```

坐标 `com.latencyplayer:latencyplayer-sdk:<version>`。完整接入与发布说明见 [`doc/MAVEN_INTEGRATION.md`](../doc/MAVEN_INTEGRATION.md)。

## 文档

| 文档 | 内容 |
|------|------|
| [根 README](../README.md) | 功能、延迟实测、体积、已知问题、重要约束 |
| [doc/MAVEN_INTEGRATION.md](../doc/MAVEN_INTEGRATION.md) | Maven 接入（推荐）、完整接入代码、发布流程 |
| [doc/USAGE.md](../doc/USAGE.md) | 快速接入、常用 API、低延迟配置、异常处理 |
| [doc/API.md](../doc/API.md) | 类 / 方法 / 配置 / 回调 / 状态 / 错误码 |
| [doc/INTEGRATION.md](../doc/INTEGRATION.md) | 环境要求、GStreamer 路径、编译 FAQ、能力范围 |
| [ios/README.md](../ios/README.md) | iOS SDK（API 对齐） |

> 英文版：根 [README.en.md](../README.en.md)、[doc/*.en.md](../doc)、[android/README.en.md](README.en.md)。

## 许可证

Proprietary（闭源授权）。GStreamer 及其依赖的许可证见 `NDK-CMake-GStreamer` 内附带的 LICENSE 文件；注意 1.0.2 起链接的 `libgstlibav` 为 GPL-2.0（见 [doc/INTEGRATION.md](../doc/INTEGRATION.md) 第 12 节）。
