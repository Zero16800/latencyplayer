# LatencyPlayer SDK — Android Platform

> 中文文档见 [README.md](README.md)。

This directory holds the **Android platform implementation** (Gradle modules) of
the LatencyPlayer SDK. For overall features, latency data and version history
see the repository root [`README.md`](../README.md); for API and integration
details see [`doc/`](../doc).

## Module structure

The root `settings.gradle.kts` maps `:sdk` / `:app` into this directory:

```
android/
├── sdk/                     # SDK library module → published as com.latencyplayer:latencyplayer-sdk
│   ├── src/main/java/com/latencyplayer/sdk/     # Public API
│   │   ├── LatencyPlayerManager.java            # Core player management class
│   │   ├── LatencyPlayerConfig.java             # Builder config
│   │   ├── LatencyPlayerCallback.java           # Callback interface
│   │   ├── LatencyPlayerState.java              # State enum
│   │   ├── LatencyPlayerError.java              # Error codes
│   │   ├── LatencyPlayerView.java               # SurfaceView render view
│   │   └── internal/                            # GStreamerInitializer / ReconnectManager
│   ├── src/main/cpp/                            # Native (NDK)
│   │   ├── latency_player_core.c / .h           # State machine, pipeline, catch-up, watchdog
│   │   ├── latency_player_jni.c                 # JNI bridge
│   │   ├── gstreamer_android.c                  # Plugin STATIC_REGISTER
│   │   ├── gstreamer_stubs.c                    # Bionic missing-symbol stubs
│   │   ├── CMakeLists.txt                       # Link whitelist / 16KB alignment
│   │   ├── ff_localize.map                      # FFmpeg ff_* symbol localization
│   │   └── vendor/rtmp/                         # Vendored gstrtmpsrc patch
│   └── build.gradle.kts
└── app/                     # Demo app
    ├── src/main/java/com/latencyplayer/demo/    # MainActivity etc.
    ├── src/main/res/                            # Layout / theme
    └── build.gradle.kts
```

## Environment

- GStreamer Android SDK **1.28.7**: `E:\Android-SDK\NDK-CMake-GStreamer` (`GSTREAMER_ROOT_ANDROID` in `gradle.properties`)
- NDK `25.1.8937393`, CMake `3.22.1`, JDK 17, AGP 8.2 / Gradle 8.2
- minSdk 21 / targetSdk 34 / compileSdk 34; ABI: `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`
- Version: `LATENCYPLAYER_VERSION` in `gradle.properties` (currently `1.0.15`)

## Build the Demo

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :app:assembleDebug --no-daemon
```

## Publish the SDK (Maven)

```bat
gradlew.bat :sdk:publishReleasePublicationToProjectLocalRepository --no-daemon
gradlew.bat :sdk:publishReleasePublicationToRemoteRepository --no-daemon
```

Coordinate `com.latencyplayer:latencyplayer-sdk:<version>`. For full integration
and publishing instructions see [`doc/MAVEN_INTEGRATION.en.md`](../doc/MAVEN_INTEGRATION.en.md).

## Documentation

| Document | Contents |
|----------|----------|
| [Root README](../README.en.md) | Features, measured latency, size, known issues, important constraints |
| [doc/MAVEN_INTEGRATION.en.md](../doc/MAVEN_INTEGRATION.en.md) | Maven integration (recommended), full integration code, publishing flow |
| [doc/USAGE.en.md](../doc/USAGE.en.md) | Quick start, common APIs, low-latency configuration, error handling |
| [doc/API.en.md](../doc/API.en.md) | Classes / methods / config / callbacks / states / error codes |
| [doc/INTEGRATION.en.md](../doc/INTEGRATION.en.md) | Requirements, GStreamer paths, build FAQ, capability scope |
| [ios/README.en.md](../ios/README.en.md) | iOS SDK (API-aligned) |

> Chinese versions: root [README.md](../README.md), [doc/*.md](../doc), [android/README.md](README.md).

## License

Proprietary (closed-source license). Licenses for GStreamer and its
dependencies are in the LICENSE files bundled with `NDK-CMake-GStreamer`; note
that the `libgstlibav` linked since 1.0.2 is GPL-2.0 (see
[doc/INTEGRATION.en.md](../doc/INTEGRATION.en.md) section 12).
