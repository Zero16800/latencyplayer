# LatencyPlayer SDK Integration Guide

> 中文文档见 [INTEGRATION.md](INTEGRATION.md)。

## 1. Requirements

- Android Studio 2022.3+ (or command-line Gradle)
- Android SDK API 21+
- Android NDK **r25** (verified in this repo: `25.1.8937393`)
- CMake **3.22.1+**
- JDK **17** (verified in this repo)
- **GStreamer Android SDK 1.28.7** (static libraries / universal, split per ABI)

## 2. GStreamer Android SDK

### 2.1 Path in this repo

This project currently uses:

```text
GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
```

The directory is split by ABI, e.g.:

```text
NDK-CMake-GStreamer\
  armeabi-v7a\
  arm64-v8a\
  x86\
  x86_64\
```

### 2.2 Where to configure it

`gradle.properties` (recommended):

```properties
GSTREAMER_ROOT_ANDROID=E\:\\Android-SDK\\NDK-CMake-GStreamer
```

Or the `GSTREAMER_ROOT_ANDROID` environment variable. `android/sdk/build.gradle.kts` passes this property to CMake as `-DGSTREAMER_ROOT_ANDROID`.

Official download: https://gstreamer.freedesktop.org/download/

## 3. Integrating the SDK

### 3.1 Dependency

**Option A: Maven (recommended, see [MAVEN_INTEGRATION.en.md](MAVEN_INTEGRATION.en.md))**

```kotlin
implementation("com.latencyplayer:latencyplayer-sdk:1.0.13")
```

**Option B: local AAR**

`android/app/build.gradle.kts`:

```kotlin
dependencies {
    implementation(files("libs/sdk-release.aar"))
}
```

### 3.2 Permissions

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

### 3.3 ABI

The `sdk` module's `abiFilters`:

- `armeabi-v7a`
- `arm64-v8a`
- `x86`
- `x86_64`

## 4. Usage

### 4.1 Layout (do not set an opaque background)

```xml
<com.latencyplayer.sdk.LatencyPlayerView
    android:id="@+id/player_view"
    android:layout_width="match_parent"
    android:layout_height="wrap_content" />
```

**Forbidden**: backgrounds such as `android:background="#000000"` that cover the Surface, otherwise the screen goes black.

### 4.2 Initialization and configuration

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

### 4.3 Surface and callbacks

```java
player.setSurface(playerView);
player.setCallback(callback);
```

### 4.4 Control

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

### 4.5 Release

```java
@Override
protected void onDestroy() {
    super.onDestroy();
    player.release();
}
```

## 5. Advanced configuration

### 5.1 Ultra-low latency

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url(url)
    .bufferTime(0)
    .lowLatency(true)
    .fastStartup(true)
    .build();
player.setConfig(config);
```

With `lowLatency=true`, native flattens the internal `queue2`, `multiqueue`, and queue elements at pipeline-ready / PLAYING moments, and shortens the playbin buffer.

### 5.2 Unlimited reconnect

```java
.autoReconnect(true)
.reconnectInterval(3)
.maxReconnectCount(0)
```

### 5.3 Landscape

```java
.orientation(2)
```

## 6. Project structure

```
LatencyPlayerSDK/
├── android/             # Android platform dir (ios/ is planned)
│   ├── sdk/             # SDK module
│   │   ├── src/main/java/   # Java API
│   │   ├── src/main/cpp/    # Native (latency_player_*.c, gstreamer_android.c)
│   │   └── build.gradle.kts
│   └── app/             # Demo
├── doc/                 # Documentation
├── gradle.properties
└── settings.gradle.kts  # :sdk / :app mapped to android/
```

## 7. Build notes

### 7.1 First build

1. Confirm the GStreamer 1.28.7 directory and its ABI subdirectories are complete
2. Configure `GSTREAMER_ROOT_ANDROID`
3. Sync Gradle / run `gradlew :app:assembleDebug`

Command-line example:

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :app:assembleDebug --no-daemon
```

### 7.2 16 KB page size (supported since 2026-09)

Background: Android 15+ supports 16 KB page devices, and Google Play requires 64-bit `.so` in apps with targetSdk 35+ to be 16 KB aligned (from 2025-11).

How this project does it (under NDK r25 / AGP 8.2):

1. **Link flags** (`CMakeLists.txt`, 64-bit ABIs only):
   ```cmake
   -Wl,-z,max-page-size=16384
   -Wl,-z,common-page-size=16384
   ```
   On NDK r27+ you can use `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON` instead; r28+ enables it by default.
2. **STL switched to `c++_static`**: NDK r25's `libc++_shared.so` is 4 KB aligned and drags down the whole package; the SDK source is pure C (openh264.a is C++, so `c++_static c++abi` must be linked explicitly), and after static linking the APK no longer contains that `.so`.
3. **Verify**:
   ```bat
   llvm-readelf -lW liblatencyplayer.so   :: LOAD segment align must be 0x4000 (64-bit)
   build-tools\35.0.0\zipalign -c -P 16 -v 4 app.apk   :: must print Verification successful
   ```

Status: `arm64-v8a`/`x86_64` LOAD align=`0x4000`; 32-bit stays `0x1000` (16 KB is required only for 64-bit); zipalign passes; Xiaomi 11 real-device rendering regression passes.

> Upgrade path: after switching to NDK r28+ / AGP 8.5.1+ you can remove the hand-written link flags (r28 defaults to 16 KB; AGP 8.5.1+ aligns uncompressed `.so` to 16 KB in the zip by default).

### 7.3 Size trimming (since 2026-09, updated in 1.0.3)

Native link strategy (`android/sdk/src/main/cpp/CMakeLists.txt`):

- Core libraries and plugins go through a **whitelist** (`CORE_ALLOW` / `PLUGIN_ALLOW`, 34 entries); anything not listed is never linked
- Libraries must go through `target_link_libraries` (placed after the `.o`); `link_options`/`@rsp` would be placed before the `.o` and prevent archives from pulling in symbols
- Library name matching uses `NAME` with the `.a` suffix removed (do not use `NAME_WE`, which truncates `libgstreamer-1.0.a` to `libgstreamer-1`)
- `LINKER:--no-undefined` forces complete linking; unresolved symbols are reported at build time
- Bionic missing-symbol stubs (`gstreamer_stubs.c`): `in6addr_any/loopback`, `getgrgid_r`, `fseeko64/ftello64` (32-bit), `__gnu_strerror_r`, 84 `vk*`
- Some object files in the FFmpeg static libs (pulled in by `libgstlibav`) are non-PIC; only after localizing symbols via the `ff_localize.map` version script (`local: ff_*;`) can they enter the `.so` (otherwise arm64 reports `R_AARCH64_ADR_PREL_PG_HI21 ... recompile with -fPIC` and x86_64 reports `R_X86_64_PC32`)

Result: AAR 82→29.4 MB (1.0.0)→34.1 MB (1.0.1)→58.2 MB (1.0.2)→59.6 MB (1.0.3, **on par in 1.0.4–1.0.13**); arm64 so 46.9→17.6→20.3→32.3→33.1 MB (1.0.3, **on par in 1.0.4–1.0.13**, stripped; the period consisted only of native fixes and API additions, no new linked libraries).

> The `in6addr_*` stub cannot include `<netinet/in.h>` (it is declared `static` there), so a custom struct is required.

### 7.4 Release pipeline (Maven, current main path)

The SDK is published with the Maven coordinate `com.latencyplayer:latencyplayer-sdk:<version>`, and the Demo has been switched to a coordinate dependency (`android/app/build.gradle.kts` → `    implementation("com.latencyplayer:latencyplayer-sdk:1.0.13")`).

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer

:: 1) publish to the local repo E:\Android-SDK\maven-repo (default; settings.gradle.kts already adds it)
gradlew.bat :sdk:publishReleasePublicationToProjectLocalRepository --no-daemon

:: 2) publish to the remote repo (configure latencyplayer.maven.url/user/password in gradle.properties first)
gradlew.bat :sdk:publishReleasePublicationToRemoteRepository --no-daemon

:: 3) build the Demo (when the version changes, prefer :app:clean to avoid cached old versions)
gradlew.bat :app:clean :app:assembleDebug --no-daemon
```

The version is `LATENCYPLAYER_VERSION` in `gradle.properties` (currently 1.0.13). For the full integrator configuration see [MAVEN_INTEGRATION.en.md](MAVEN_INTEGRATION.en.md).

> The old offline `implementation(files("libs/sdk-release.aar"))` form still works: manually copy `android\sdk\build\outputs\aar\sdk-release.aar` to `android\app\libs\`; if the AAR changed you must `:app:clean` or it will be UP-TO-DATE and the old package will be built in.

### 7.5 FAQ

**Could not GET intranet Nexus ... Read timed out**

A global proxy in `~/.gradle/gradle.properties` (e.g. `127.0.0.1:1080`) is also forwarding intranet repository requests. The project `gradle.properties` already adds:

```properties
systemProp.http.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1
systemProp.https.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1
```

**400 Repository does not allow updating assets**

Nexus `maven-releases` is `ALLOW_ONCE`, so overwriting the same version is forbidden: increment `LATENCYPLAYER_VERSION` in `gradle.properties` and publish again.

**Cannot find gstreamer / headers**

Check that `GSTREAMER_ROOT_ANDROID` points to the parent directory containing `armeabi-v7a` etc.

**UnsatisfiedLinkError**

Confirm the APK contains the `liblatencyplayer.so` for the corresponding ABI and that the linked GStreamer static libraries correctly participate in CMake.

**No picture during playback**

1. Whether the layout mistakenly sets an opaque background on `LatencyPlayerView`
2. Whether `setSurface` was called
3. Whether logcat shows `Video size:` and first-frame / PLAYING logs

**No sound / missing codecs during playback**

See the capability table below; plugins go through the whitelist (`PLUGIN_ALLOW`, 34 entries) and anything not listed is not linked.

## 8. Protocol support (current actual)

Based on the plugins that `android/sdk/src/main/cpp/gstreamer_android.c` has **STATIC_REGISTER**ed:

| Protocol | Supported | Notes |
|----------|-----------|-------|
| RTMP / RTMPS | ✅ | Main path; prefers `rtmpsrc` (librtmp), `rtmp2src` when needed. mediamtx live regression passed since 1.0.2 (H.264 + AAC, opensles audio); **1.0.3 fix** for intermittent no-video caused by decodebin exposing too early (12/12 stable); **1.0.7 fix** for SIGSEGV at the instant of stop/switch (rtmpsrc unlock race, see Known Issues #9); **1.0.13 fix** for the ~2.5 s startup backlog caused by the CDN burst at connect (appsink `sync=FALSE` + catch-up dropping, see Known Issues #10) |
| RTSP / RTSPS | ✅ | 1.0.1 registered `rtspsrc` + `rtp`/`rtpmanager` + `gstrtsp`/`gstsdp`; `latency=80ms` (lowLatency) / `200ms`. 1.0.3 regression passed (First frame + PLAYING) |
| HTTP-FLV | ✅ | 1.0.1 registered `libsoup-3.0` (`souphttpsrc`, `timeout=10s`). 1.0.3 regression passed (plays to completion; first-read hang not reproduced) |
| HLS | ✅ | 1.0.1 registered `hls` / `mpegtsdemux` / `isomp4` / `aes` + `uridownloader`/`adaptivedemux`. 1.0.3 VOD + **live (mediamtx fMP4 dual variant) regression passed** (**1.0.3 fix**, see Known Issues #5) |
| WebRTC | ❌ | Not an SDK capability; pure client-side RTMP→WebRTC is not feasible |

The `tcp` plugin is registered, but that **does not mean** a complete HTTP playback stack is supported.

## 9. Codecs (current actual)

| Codec | Video | Audio |
|-------|-------|-------|
| H.264 | ✅ openh264 + Android media decode path | - |
| H.265/HEVC | ⚠️ Device / official path dependent; not guaranteed | - |
| MP3 | - | ✅ mpg123 |
| Vorbis / Opus / Speex / FLAC | - | ✅ related decode plugins registered |
| AC3 | - | ✅ a52dec |
| AAC | - | ✅ libav `avdec_aac` (`libgstlibav` registered in 1.0.2) |
| Theora | ✅ theora | - |

The plugin registration list is in `gstreamer_android.c`; the linked set is in `android/sdk/src/main/cpp/CMakeLists.txt` (whitelist-based: `CORE_ALLOW` + `PLUGIN_ALLOW`, no exclusion list). Decoder ranks: `openh264dec = PRIMARY+1` outranks `avdec_h264 = MARGINAL` (since 1.0.2; previously avdec_h264 grabbing priority caused HLS/TS decode to report `decode_slice_header error`).

## 10. Rendering path (summary)

1. `playbin`, with `video-sink` going to **appsink** (`RGBx/RGBA`, `sync=FALSE`, small buffers). `sync=FALSE` + the callback "directly drop old frames lagging the live edge by >600 ms" (1.0.13 catch-up) keeps it hugging the live edge and prevents the CDN burst backlog from dragging it behind
2. The callback fetches the sample → direct-draws to `ANativeWindow`
3. On Surface size changes, calls `setBuffersGeometry` as needed (guards against repeated calls)
4. `glimagesink`/EGL is unreliable on some emulators and is used only as a fallback

## 11. Known issues (updated 2026-10)

1. **Rapid stop→play may crash**: stream-thread `multiqueue:src` caps use-after-free (teardown vs. bus callbacks `on_buffering`/`on_async_done` race; stack in `gst_caps_features_copy`). Being fixed; workaround: after stop, wait for the state to return to `STOPPED` before `start()`.
2. **The device must stay awake while measuring**: dozing stops the Surface from rendering and screenshots go all-black, so latency measurement fails (the early "steady-state ≥7 s" was misjudged this way; real-device measured first frame avg 491 ms, steady-state picture latency median ~125 ms — see [README Latency](../README.en.md#latency)).
3. **ABI is all 4**: the arm64-only slimming strategy is pending a decision (could save ~2/3 of the size).
4. ~~**HTTP-FLV first body read may hang (pending verification in 1.0.1)**~~ — 1.0.2 regression passed: `souphttpsrc timeout=10` + a src-pad buffer probe take effect; HTTP-FLV plays to completion, hang not reproduced.
5. ~~**HLS live (fMP4 multi-variant) segment download anomaly (found in 1.0.2)**~~ — **fixed, shipped in 1.0.3, regression passed (2/2 rendered)**: the root cause was the low-latency `queue` tune also setting `leaky` on **raw container byte streams**, dropping bytes so `qtdemux` reported `atom bogus size` → `adaptivedemux` repeatedly restarting with `Error while downloading fragment`. Fix: `queue_carries_raw_bytes()` walks the sink-pad upstream to decide whether the data is still raw container bytes (before Demuxer/Parser); raw streams are not made `leaky`.
6. ~~**RTMP intermittent no-video locally (found in 1.0.2)**~~ — **fixed in 1.0.3, local regression 12/12 stable**: low `multiqueue` limits overrun too early → `decodebin` exposes an audio-only group → the flvdemux thread blocks in `gst_data_queue_push()` and cannot parse video tags → the late video pad is dropped with `No current group`. Fix (vendored `gstdecodebin2.c`): on overrun, first raise the multiqueue limits (1000/4MB/5s) to unblock the demuxer, then wait with a 2500 ms grace for the video pad to join and expose together with video; the low-latency tune no longer shrinks multiqueue's `max-size-time`/`max-size-bytes`, avoiding the async-done walk-tune pushing the raised limits back.
7. ~~**`stop()` during playback races with buffering messages → SIGSEGV (found in 1.0.2)**~~ — **fixed in 1.0.3, stress 0 crashes 0 deadlocks**: the bus thread's `on_buffering` (live branch `set_state(PLAYING)`) and `latency_player_stop()`'s `set_state(NULL)` ran concurrently; playbin `activate_group` accessed a freed object (`SIGSEGV fault addr 0xaaaaaaaa`). Fix: all bus signal handlers (error/eos/state-changed/buffering/async-done) and pause/resume/setter/query/snapshot-capture uniformly hold `ctx->mutex`, serialized with play/stop/set_surface on the same lock. Stress: HLS live 6 + RTMP 8 stops with buffering activity, all-protocol regression passed.
8. ~~**Repeated playback triggers an ANR "not responding" (found after 1.0.3)**~~ — **fixed (2026-10-04, shipped in 1.0.4), stress 70 playbacks 0 deaths 0 ANRs**: on the main thread, `latency_player_play()` holding `ctx->mutex` to tear down the old pipeline (`set_state(NULL)` waiting for the appsink stream thread to exit rendering) and the stream thread's `on_appsink_new_sample → draw_frame_to_window` waiting on the same `ctx->mutex` formed a circular wait → main thread froze, system raised an ANR (reproduced around the 4th repeated play tap; `latency_player_play+252 = core.c:614` stuck at `gst_base_sink_change_state → g_mutex_lock_slowpath`). Fix: a new `window_mutex` protects `native_window`/`window_geo_*`; `draw_frame_to_window` and `prepare-window-handle` hold only this lock; `set_surface`/`release` hold it nested within `ctx->mutex` (one-directional lock order `ctx->mutex → window_mutex`, never holding `window_mutex` across `set_state`); the stream thread no longer touches `ctx->mutex`. Regression: RTMP rendering smoke, RTSP/HLS/HTTP-FLV all passed (HLS 1920x1080 rendered, snapshot confirmed).
9. ~~**SIGSEGV at the instant of stopping/switching an RTMP stream (`fault addr 0x38`, stack in `RTMP_ReadPacket`; reproduced on a real device on 2026-10-08)**~~ — **fixed (shipped in 1.0.7), 0 crashes under real-device stress**: upstream gstrtmpsrc's `unlock()` calls `RTMP_Close()` directly; librtmp's `CloseInternal()` `free()`s and nulls `r->m_vec`, while the stream thread is still dereferencing `m_vec[channel]` in `RTMP_ReadPacket` (channel=7 → 0x38); the window = stopping/switching while packets are being received. Fix (vendored `vendor/rtmp/gstrtmpsrc.c`): `unlock()` only `shutdown(socket)`; `RTMP_Close+RTMP_Free` is deferred to `stop()` (after the stream thread joins, zero leaks); the `start()` failure path adds Close; JNI ignores SIGPIPE (writing DeleteStream to an already-shutdown socket yields EPIPE, no longer killing the process). Trade-off: no auto-reconnect after flush-seek (this SDK forces RTMP `live=1` and does not seek). Stress: rapid stop→play 120 rounds + channel-switch regression, 0 crashes.
10. ~~**Startup stays a constant ~2.5 s behind the live edge (located in 1.0.12, fixed in 1.0.13)**~~ — root cause: at connect the CDN (Alibaba Cloud) bursts ~2.5 s of history video (initial rate ≈ 10× realtime bitrate); the old implementation buffered it whole and drained at 1×, never emptying (`multiqueue` self-reporting 0 is untrustworthy; the backlog is actually between `flvdemux→h264parse`, proven by `vdm` 2520 ms and `vpipe` only 160 ms). Fix: appsink `sync=FALSE` + the callback catch-up dropping based on "live edge − frame PTS" (>600 ms enters, <200 ms exits; skip frames only, never render stale frames). Measured `pipe_backlog` 2550 ms → 40–200 ms, catch-up ~0.2–0.3 s, first frame ≈0.9 s.
11. ~~**Black screen after backgrounding / being frozen and returning (fixed in 1.0.13)**~~ — scenario: backgrounding destroys the SurfaceView surface and stops drawing; returning rebuilds and rebinds it; short backgrounding recovers in a second, but after a longer background period (MIUI freezes background processes) the RTMP source still receives bytes while video frames stop flowing, and the screen stays black after returning. Fix: native records "age of the latest video frame" (new API `getLastFrameAgeMs()`, `last_video_sample_wall_us` updated per sample); `ReconnectManager` checks every second during playback and forces a reconnect after >3 s with no new frame (log `Video stalled: no frame for Xms, forcing reconnect`). Measured ~750 ms to render after a 22 s background. Note: pressing Back to truly exit calls `release()` to destroy the player; reopening requires the business layer to `start()` again.

Fixed: steady-state latency misjudgment (measured ~125 ms); repeated "play" taps double-free (`gst_object_ref_sink`); `.so` symbol/link integrity (`--no-undefined` + Bionic stubs); missing AAC decoding (1.0.2 registered `libgstlibav`); no audio output during playback (1.0.2 registered `libgstopensles`, linking the system `OpenSLES`); decode errors from `avdec_h264` grabbing priority over openh264 (rank adjusted in 1.0.2); HLS live raw-byte leak; RTMP intermittent no-video; snapshot file writing (PNG/JPEG by suffix); stop×buffering race SIGSEGV during playback; repeated-playback ANR deadlock (draw and main-thread teardown waiting on each other's `ctx->mutex`); RTMP stop/switch `m_vec` NULL SIGSEGV (1.0.7 rtmpsrc unlock patch); startup CDN burst backlog (1.0.13 appsink `sync=FALSE` + catch-up dropping); black screen after background return (1.0.13 stall watchdog, `getLastFrameAgeMs`).

## 12. License

- LatencyPlayer SDK sample code: usable per the project's convention
- GStreamer core is mostly LGPL, some plugins are GPL/other licenses — verify compliance against the linked plugins before distribution
- **Note**: the newly linked `libgstlibav` (gst-libav, the FFmpeg wrapper) since 1.0.2 is **GPL-2.0**, pulling in FFmpeg libraries such as `libavcodec`/`libavutil` (LGPL 2.1+, assembly under its own license); before closed-source distribution, assess GPL contagion or isolate via a separate process/dynamic loading, and if necessary remove `libgstlibav` from `PLUGIN_ALLOW` (cost: back to no AAC software decode)
