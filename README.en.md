# LatencyPlayer SDK

> 中文文档见 [README.md](README.md)。

An Android live-streaming player SDK built on **GStreamer** (statically linked).
Main path: **RTMP ingest → software/hardware decode → appsink direct-draw to `ANativeWindow`** (bypasses EGL, stable on emulators too).

```java
LatencyPlayerManager player = new LatencyPlayerManager();
player.init(context.getApplicationContext());
player.setConfig(LatencyPlayerConfig.builder()
    .url("rtmp://example.com/live/stream")
    .bufferTime(0)          // lowest latency
    .lowLatency(true)       // flatten internal queues
    .fastStartup(true)
    .build());
player.setSurface(playerView);   // LatencyPlayerView
player.setCallback(callback);
player.start();
```

> **iOS**: a lightweight native implementation (librtmp + VideoToolbox +
> AVSampleBufferDisplayLayer) with an API aligned to Android, delivered as an
> XCFramework + Swift Package. See [`ios/README.md`](ios/README.md). iOS is
> currently a **first-pass scaffold**: the video path is implemented, **audio is
> pending**.

---

## Features

| Feature | Status | Notes |
|---------|--------|-------|
| RTMP / RTMPS live ingest | ✅ | Main path, `rtmpsrc` (librtmp), falls back to `rtmp2src` when needed |
| appsink direct-draw rendering | ✅ | RGBx/RGBA → `ANativeWindow`, no EGL dependency |
| Software / hardware decode switch | ✅ | openh264 software decode + Android MediaCodec path |
| Low-latency mode | ✅ | `bufferTime(0)` + `lowLatency(true)`, see "Latency" below |
| Auto-reconnect on disconnect | ✅ | Interval / count configurable, `maxReconnectCount(0)` = unlimited |
| Background-return self-recovery | ✅ | After backgrounding / being frozen and returning, if video stops flowing for >3 s it auto-reconnects (`getLastFrameAgeMs` + `ReconnectManager` stall watchdog) |
| Preconnect (millisecond stream switching) | ✅ | `preconnect(url)` hold mode — builds the link, decodes the first frame and absorbs the CDN burst backlog in the background; same-URL playback renders instantly; dual-instance switching tap→first-frame **avg 173 ms** |
| Pause / resume / stop | ✅ | |
| Hot URL switch | ✅ | `switchUrl`, pipeline is not destroyed |
| Mute / volume | ✅ | 0–100 |
| Flip / rotate | ✅ | Horizontal/vertical flip, 0/90/180/270 |
| Download speed / bitrate callback | ✅ | `onDownloadSpeed` |
| First-frame / video-size callback | ✅ | Delivered on the main thread |
| Snapshot | ✅ | `saveSnapshot(path)` — PNG / JPEG chosen by path suffix (`pngenc` / `jpegenc`), returns success; verified on device: PNG 55.9 KB, JPEG 50.1 KB (`FFD8…FFD9` valid) |
| Frame-age query | ✅ | `getLastFrameAgeMs()`: age of the latest video frame (ms), for stall detection or a custom watchdog |
| HLS / HTTP-FLV / RTSP | ✅ | RTSP / HTTP-FLV / HLS VOD + HLS live (fMP4 multi-variant) regression-passed |
| WebRTC | ❌ | Not supported |

### Protocols

| Protocol | Supported | Notes |
|----------|-----------|-------|
| RTMP / RTMPS | ✅ | Main path; local mediamtx live regression passed (incl. AAC audio); the CDN burst backlog at connect is handled by built-in appsink `sync=FALSE` + catch-up dropping (see "Latency → CDN burst backlog"); the stop/switch instant read-thread race is fixed (vendored `gstrtmpsrc` patch), real-device stress: 0 crashes |
| RTSP / RTSPS | ✅ | Registered `rtspsrc` + RTP/SDP stack, `latency=80/200 ms`, regression passed |
| HTTP-FLV | ✅ | Registered `libsoup-3.0` + `souphttpsrc` (timeout=10 s), regression passed |
| HLS | ✅ | Registered `hls` / `mpegtsdemux` / `isomp4` / `aes`; VOD + live (fMP4 multi-variant) regression passed |

### Codecs

| Codec | Video | Audio |
|-------|-------|-------|
| H.264 | ✅ openh264 + MediaCodec | - |
| H.265 / HEVC | ⚠️ Device-dependent, not guaranteed | - |
| MP3 | - | ✅ mpg123 |
| Vorbis / Opus / Speex / FLAC | - | ✅ registered |
| AC3 | - | ✅ a52dec |
| AAC | - | ✅ libav `avdec_aac` |
| Theora | ✅ | - |

Plugin registration list: `android/sdk/src/main/cpp/gstreamer_android.c`; link whitelist: `android/sdk/src/main/cpp/CMakeLists.txt`.

---

## Latency

### Measured data (Xiaomi 11 / arm64-v8a / API 34, source 720x1280 H.264 RTMP)

| Metric | Result | Timing method |
|--------|--------|---------------|
| First frame (tap play → first frame rendered, plain cold start without preconnect) | **avg 491 ms** (3 runs: 457 / 491 / 528 ms) | device time at tap → logcat `First frame post` timestamp |
| Switch tap → new stream first frame (dual-instance preconnect) | **avg 173 ms** (8 switches: 37 / 172–228 ms) | demo `TIMING first-frame` |
| Steady-state picture latency (relative to a PC low-latency reference) | **53–174 ms, median ~125 ms** (16 valid samples) | per-frame content matching, see below |

Steady-state method: on the PC, pull the same live stream with `ffmpeg -fflags +nobuffer -flags low_delay`, caching "frame arrival time + 128x206 grayscale frame"; on the phone, `screencap` (raw format, device-side `date +%s.%3N` timestamp, capture window ~200 ms) crop the playback area and SAD-match frame-by-frame against PC frames. Match quality `best_diff ≈ 2.7`, runner-up frame ≥ 4 (a clear valley exists); device/PC clocks aligned with `adb shell date` (rtt ~100 ms).

Error and boundaries:

- The screenshot is captured at the **start** of the window (full PNG encoding stretches the window to 1.8 s; taking the midpoint overstates by ~900 ms — switched to raw to avoid this); even the midpoint still systematically overestimates by ~100 ms;
- The value is a relative quantity ("phone link − PC reference link"); the network paths and decode time of both sides are not separated;
- It is the same order of magnitude as the configured `queue2 50ms + multiqueue 8-buffer cap`, mutually corroborating.

> With the screen off / dozing, the Surface stops rendering and screenshots go all-black — latency measurement fails unless the device is kept awake.

### CDN burst backlog (built-in handling)

At the moment an RTMP connection is established, a CDN (e.g. Alibaba Cloud) bursts the last ~**2.5 s of history video in one shot** (measured initial rate ≈ 10× realtime bitrate). Handling:

- appsink runs with `sync=FALSE` (the queue drains at decode speed, hugging the live edge, rather than at 1×);
- the appsink callback performs **catch-up dropping** based on "live edge − current frame PTS" (enters catch-up and drops old frames when lagging >600 ms, exits below <200 ms), skipping frames only, never rendering stale frames, avoiding a "fast-forward" feel.

Result: player-side backlog `pipe_backlog` **40–200 ms**; catch-up takes ~0.2–0.3 s. Log evidence: `catchup: START drop (lag=2361ms)` → `catchup: STOP (lag=199ms)` → `First frame post=1`.

### Preconnect millisecond stream switching (1.0.15)

`preconnect(url)` (hold mode) moves everything — RTMP connect + CDN burst delivery + burst-backlog absorption + the first keyframe software decode (~300–500 ms) — to the background **before** the tap:

- after `preconnect()`, the pipeline runs straight to PLAYING and keeps consuming the stream in the background, but **does not render to the Surface and does not fire FIRST_FRAME** (samples are silently dropped; the catch-up-dropping logic meanwhile absorbs the lag to the live edge);
- `start()` / `switchUrl()` on the same URL just releases the hold; the next decoded frame (a fresh one hugging the live edge) renders immediately;
- `preconnect()` itself returns in ~200–400 ms and does not block the calling thread; each preconnect holds one RTMP connection + one software decoder.

Why "parked at PAUSED" does not work: playbin's `ASYNC→PAUSED` can complete from the **audio branch alone** — at that moment the video branch (input-selector/appsink) does not exist yet, and decodebin only exposes the video pad after the first keyframe has **finished decoding**. The preconnected pipeline therefore has to run to PLAYING so the video chain is built in the background and `play()` finds a ready frame.

Measured (Xiaomi 11, dual-instance demo — two Alibaba Cloud RTMP 720x1280 H.264 streams, one preconnected each at startup, roles swapped on every switch):

| Metric | Result |
|--------|--------|
| Switch tap → new stream first frame | **avg 173 ms** (8 switches: 37 / 172–228 ms) |
| Startup first tap (preconnected) | 37–96 ms |
| Ordinary switch without preconnect (baseline) | ~1000–1135 ms |

### Low-latency configuration

Everything the client can do is done:

```java
LatencyPlayerConfig.builder()
    .bufferTime(0)          // buffer time 0 ms (default 200 ms, range 0–5000)
    .lowLatency(true)       // flatten internal pipeline queues
    .fastStartup(true)
    .build();
```

When `lowLatency=true`, native code flattens internal queues at pipeline-ready / PLAYING moments:

| Element | Parameters |
|---------|------------|
| `queue` / `queue2` | `max-size-buffers=3`, `max-size-time=50ms`, `max-size-bytes=0` |
| `multiqueue` | `max-size-buffers=8`, `interleave-max-bytes=0` (**does not shrink** `max-size-time` / `max-size-bytes` — shrinking these makes decodebin expose too early and cannot be lifted back by the overrun grace patch) |

That is, the **client-side buffer is squeezed to the ~100 ms range**, the same order as the steady-state measurements above; the remaining end-to-end portion is determined by the server's push cadence and the network.

---

## Background & lifecycle

| Scenario | Behaviour |
|----------|-----------|
| Background (Home) then short return | Surface is rebuilt and rebound automatically; the picture resumes immediately |
| Background for a while then return (process frozen by system/MIUI) | After resume, if video frames have stopped flowing for >3 s, the `ReconnectManager` stall watchdog auto-reconnects; measured ~750 ms to render after returning |
| Press **Back** to truly exit → reopen | The current Demo calls `release()` in `onDestroy` to destroy the player; reopening is a brand-new instance and **does not auto-play** (you must tap "Play"). To auto-resume, persist the URL and auto-`start()` on reopen, or move playback into a foreground service |

> Truly **continuing playback with the screen off / locked** (not just return-recovery) requires a foreground service to avoid the system freezing the background process — not built into the SDK.

## Devices & compatibility

| Item | Supported range |
|------|-----------------|
| Minimum OS | Android 5.0 (API 21) |
| Target OS | Android 14 (API 34), verified |
| ABI | `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64` |
| Verified device | Xiaomi 11 (`M2011K2C`, arm64, Android 14): 720p RTMP renders, play/stop/reconnect normal |
| Emulator | x86_64 runs (`glimagesink` has compatibility issues on some emulators; the SDK uses appsink by default to avoid this) |
| 16 KB page devices | ✅ 64-bit `.so` LOAD segments `0x4000`-aligned, `zipalign -P 16` passes (Android 15+) |
| Old 32-bit devices | ✅ kept 4 KB aligned (16 KB is required only for 64-bit) |
| Foldables / tablets | ✅ Surface size changes are rebound automatically |
| Landscape/portrait | ✅ switching within `configChanges` does not interrupt the stream |

> Other arm64/armv7 devices are theoretically compatible (pure NDK + static GStreamer); after integration, regression-test first-frame and reconnect on your target devices.

---

## Size

| Artifact | Size |
|----------|------|
| AAR (4 ABIs) | **59.6 MB** |
| `liblatencyplayer.so` (arm64, stripped) | 33.1 MB |
| Demo APK (all ABIs) | 68.7 MB |
| Integrator APK delta (single arm64) | ~+33.1 MB |

Trim measures: `CMakeLists.txt` links only the core-library whitelist + a 34-plugin whitelist (whole-archive for plugins only), strip, Bionic missing-symbol stubs (`vk*` / `getgrgid_r` etc.), and `ff_localize.map` localizing FFmpeg `ff_*` symbols to satisfy PIC linking. Link integrity is enforced by `LINKER:--no-undefined`.

---

## Integration (Maven, recommended)

```kotlin
// settings.gradle.kts -> dependencyResolutionManagement.repositories
maven {
    url = uri("http://192.168.3.160:8970/repository/maven-releases/")  // Nexus3 (anonymous read)
    isAllowInsecureProtocol = true   // required for http repositories
}
// or, during development, a local repo: maven { url = uri("file:///E:/Android-SDK/maven-repo") }

// android/app/build.gradle.kts
implementation("com.latencyplayer:latencyplayer-sdk:1.0.15")
```

Publishing (credentials passed via `-P`, never stored):

```bat
rem publish to Nexus3
gradlew :sdk:publishReleasePublicationToRemoteRepository ^
  -Platencyplayer.maven.url=http://192.168.3.160:8970/repository/maven-releases/ ^
  -Platencyplayer.maven.user=admin -Platencyplayer.maven.password=******

rem publish to the in-project maven-repo/
gradlew :sdk:publishReleasePublicationToProjectLocalRepository

rem publish to the local ~/.m2
gradlew :sdk:publishToMavenLocal
```

Version: `LATENCYPLAYER_VERSION` in `gradle.properties` (currently `1.0.15`).

> If your machine's `~/.gradle` has a global proxy, the intranet Nexus needs `systemProp.http.nonProxyHosts=192.168.*` (already set in the project `gradle.properties`).
> Full integration code (Manifest / layout / MainActivity / ProGuard) is in [doc/MAVEN_INTEGRATION.en.md](doc/MAVEN_INTEGRATION.en.md).

---

## Documentation index

| Document | Contents |
|----------|----------|
| [doc/MAVEN_INTEGRATION.en.md](doc/MAVEN_INTEGRATION.en.md) | **Maven integration (recommended)**, full integration code, API operations, features & device support |
| [doc/USAGE.en.md](doc/USAGE.en.md) | Quick start, common APIs, low-latency configuration, error handling |
| [doc/API.en.md](doc/API.en.md) | Classes / methods / config / callbacks / states / error codes |
| [doc/INTEGRATION.en.md](doc/INTEGRATION.en.md) | Requirements, GStreamer paths, build FAQ, capability scope |
| [ios/README.md](ios/README.md) | **iOS SDK**: architecture, XCFramework/SPM build, Android⇄iOS API mapping, status & TODO |
| [android/README.md](android/README.md) | **Android platform**: module structure, build/publish entry points, native layout, doc navigation |

> Chinese version: [README.md](README.md), [doc/*.md](doc), [android/README.md](android/README.md).

---

## Environment summary

- GStreamer Android SDK: `E:\Android-SDK\NDK-CMake-GStreamer` (version **1.28.7**)
- NDK `25.1.8937393`, CMake `3.22.1`, JDK 17, AGP 8.2 / Gradle 8.2
- minSdk 21 / targetSdk 34 / compileSdk 34
- ABI: `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`

```properties
# gradle.properties
GSTREAMER_ROOT_ANDROID=E\:\\Android-SDK\\NDK-CMake-GStreamer
```

Build the Demo:

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :app:assembleDebug --no-daemon
```

### Project structure

```
E:\Android-SDK\
├── android/           # Android platform dir (Gradle modules; ios/ is planned)
│   ├── sdk/           # SDK library module (published as com.latencyplayer:latencyplayer-sdk)
│   └── app/           # Demo app
├── doc/               # Documentation
├── gradle.properties  # GSTREAMER_ROOT_ANDROID / LATENCYPLAYER_VERSION
└── settings.gradle.kts # rootProject: LatencyPlayerSDK (:sdk/:app mapped to android/)
```

---

## 16 KB page size (Android 15+)

Supported. `arm64-v8a` / `x86_64` `.so` LOAD segments are `0x4000`-aligned (16 KB); `zipalign -P 16` passes; 32-bit ABIs stay `0x1000` (16 KB is required only for 64-bit).

- NDK r25 has no `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES` (only r27+), so use the link flag `-Wl,-z,max-page-size=16384`
- STL switched from `c++_shared` to `c++_static`: NDK r25's bundled `libc++_shared.so` is only 4 KB aligned; after static linking it is no longer packaged into the APK
- Note: Google Play enforces 16 KB for targetSdk 35+; currently targetSdk 34, so not yet subject to the Play policy

---

## Known issues

1. **Rapid stop→play switch may crash** — stream-thread caps use-after-free (teardown vs. bus callback race; stack in `gst_caps_features_set_parent_refcount` / `gst_caps_push`). **Workaround: after stop, wait for the state to return to `STOPPED` before `start()`**.
2. **ABI is currently all 4** — the arm64-only slimming strategy is pending a decision (could save ~2/3 of the size).
3. **The device must stay awake while measuring** — dozing stops the Surface from rendering, so screenshots go all-black and latency measurement fails.

---

## Important constraints

1. **Do not set an opaque background on `LatencyPlayerView`** (e.g. `android:background="#000000"`), otherwise the Surface hole is covered and the screen goes black; the View already does `setBackgroundColor(0)` + `RGBA_8888` internally.
2. For low latency, use `.bufferTime(0)` **and** `.lowLatency(true)` together.
3. Capabilities are primarily **RTMP**; RTSP / HTTP-FLV / HLS (VOD + live) passed regression, but in production prefer RTMP — see [doc/INTEGRATION.en.md](doc/INTEGRATION.en.md).
4. After changing the config you must call `setConfig()` again for it to take effect.
5. Each preconnect instance holds one RTMP connection + one software decoder; free them with `stop()` / `release()` when done.

---

## License

Proprietary (closed-source license). Licenses for GStreamer and its dependencies are in the LICENSE files bundled with `NDK-CMake-GStreamer`; note that `libgstlibav` (linked for AAC decoding) is GPL-2.0 (see [doc/INTEGRATION.en.md](doc/INTEGRATION.en.md), section 12).
