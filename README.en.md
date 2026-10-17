# LatencyPlayer SDK

An Android live-streaming player SDK built on **GStreamer** (statically linked).
Main path: **RTMP ingest → software/hardware decode → appsink direct-draw to `ANativeWindow`** (bypasses EGL, stable on emulators too).

> 中文文档见 [README.md](README.md)。

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
| Low-latency mode | ✅ | `bufferTime=0` + `lowLatency=true`, see "Latency" below |
| Auto-reconnect on disconnect | ✅ | Interval / count configurable, `maxReconnectCount(0)` = unlimited |
| Background-return self-recovery | ✅ | 1.0.13: after backgrounding / being frozen and returning, if video stops flowing for >3 s it auto-reconnects (`getLastFrameAgeMs` + `ReconnectManager` stall watchdog) |
| Pause / resume / stop | ✅ | |
| Hot URL switch | ✅ | `switchUrl`, pipeline is not destroyed |
| Mute / volume | ✅ | 0–100 |
| Flip / rotate | ✅ | Horizontal/vertical flip, 0/90/180/270 |
| Download speed / bitrate callback | ✅ | `onDownloadSpeed` |
| First-frame / video-size callback | ✅ | Delivered on the main thread |
| Snapshot | ✅ | `saveSnapshot(path)` — PNG / JPEG chosen by path suffix (`pngenc` / `jpegenc`), returns success; verified on device: PNG 55.9 KB, JPEG 50.1 KB (`FFD8…FFD9` valid) |
| HLS / HTTP-FLV / RTSP | ✅ | Regression-passed in 1.0.3 (RTSP / HTTP-FLV / HLS VOD + HLS live mediamtx fMP4, 2/2 rendered) |
| WebRTC | ❌ | Not supported |

### Protocols

| Protocol | Supported | Notes |
|----------|-----------|-------|
| RTMP / RTMPS | ✅ | Main path; 1.0.2 local mediamtx live regression passed (incl. AAC audio); **1.0.3 fix** for intermittent no-video caused by decodebin exposing too early (local regression 12/12 stable); **1.0.7 fix** for SIGSEGV at the moment of stop/switch (rtmpsrc unlock×read-thread race, see Known Issues #8); **1.0.13 fix** for the **~2.5 s startup backlog** caused by the CDN burst at connect (see "Latency → Startup backlog") |
| RTSP / RTSPS | ✅ | 1.0.1 registered `rtspsrc` + RTP/SDP stack, `latency=80/200 ms`; 1.0.3 regression passed |
| HTTP-FLV | ✅ | 1.0.1 registered `libsoup-3.0` + `souphttpsrc` (timeout=10 s); 1.0.3 regression passed (plays to completion) |
| HLS | ✅ | 1.0.1 registered `hls` / `mpegtsdemux` / `isomp4` / `aes`; 1.0.3 VOD + **live (fMP4 multi-variant) regression passed** (**1.0.3 fix**, see Known Issues #4) |

### Codecs

| Codec | Video | Audio |
|-------|-------|-------|
| H.264 | ✅ openh264 + MediaCodec | - |
| H.265 / HEVC | ⚠️ Device-dependent, not guaranteed | - |
| MP3 | - | ✅ mpg123 |
| Vorbis / Opus / Speex / FLAC | - | ✅ registered |
| AC3 | - | ✅ a52dec |
| AAC | - | ✅ libav `avdec_aac` (`libgstlibav` registered since 1.0.2) |
| Theora | ✅ | - |

Plugin registration list: `android/sdk/src/main/cpp/gstreamer_android.c`; link whitelist: `android/sdk/src/main/cpp/CMakeLists.txt`.

---

## Latency

### Measured data (Xiaomi 11 / arm64-v8a / API 34, source 720x1280 H.264 RTMP)

| Metric | Result | Timing method |
|--------|--------|---------------|
| First frame (tap play → first frame rendered) | **avg 491 ms** (3 runs: 457 / 491 / 528 ms) | device time at tap → logcat `First frame post` timestamp |
| Steady-state picture latency (relative to a PC low-latency reference) | **53–174 ms, median ~125 ms** (16 valid samples) | per-frame content matching, see below |

Steady-state method: on the PC, pull the same live stream with `ffmpeg -fflags +nobuffer -flags low_delay`, caching "frame arrival time + 128x206 grayscale frame"; on the phone, `screencap` (raw format, device-side `date +%s.%3N` timestamp, capture window ~200 ms) crop the playback area and SAD-match frame-by-frame against PC frames. Match quality `best_diff ≈ 2.7`, runner-up frame ≥ 4 (a clear valley exists); device/PC clocks aligned with `adb shell date` (rtt ~100 ms).

Error and boundaries:

- The screenshot is captured at the **start** of the window (full PNG encoding stretches the window to 1.8 s; taking the midpoint overstates by ~900 ms — switched to raw to avoid this); even the midpoint still systematically overestimates by ~100 ms;
- The value is a relative quantity ("phone link − PC reference link"); the network paths and decode time of both sides are not separated;
- It is the same order of magnitude as the configured `queue2 50ms + multiqueue 8-buffer cap`, mutually corroborating.

> The earlier "steady-state ≥7 s" record was a measurement artifact: the device was dozing during measurement, so screenshots were all-black and frame matching failed.

Low-latency configuration (everything the client can do is done):

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
| `multiqueue` | `max-size-buffers=8`, `interleave-max-bytes=0` (**does not shrink** `max-size-time` / `max-size-bytes` — shrinking these makes decodebin expose too early and cannot be lifted back by the overrun grace patch; see Known Issues fixes) |

That is, the **client-side buffer is squeezed to the ~100 ms range**, the same order as the steady-state measurements above; the remaining end-to-end portion is determined by the server's push cadence and the network.

### Startup backlog (fixed in 1.0.13)

At the moment an RTMP connection is established, a CDN (e.g. Alibaba Cloud) bursts the last ~**2.5 s of history video in one shot** (measured initial rate ≈ 10× realtime bitrate). In the old implementation this was buffered whole by `multiqueue` and then played back at 1× frame-by-frame, so **the whole playback lifetime stayed a constant ~2.5 s behind the live edge** — note that `multiqueue` self-reporting a queue of 0 is untrustworthy in this vendored version; the backlog actually sits between `flvdemux → h264parse` (proven by probes: `vdm` 2520 ms, `vpipe` only 160 ms).

1.0.13 fix:

- appsink switched to `sync=FALSE` (the queue drains at decode speed, hugging the live edge, rather than at 1×);
- the appsink callback performs **catch-up dropping** based on "live edge − current frame PTS" (enters catch-up and drops old frames when lagging >600 ms, exits below <200 ms), skipping frames only, never rendering stale frames, avoiding a "fast-forward" feel.

After the fix, the player-side backlog `pipe_backlog` dropped from **~2550 ms to 40–200 ms** (same order as `vdemux` / `vparse` probes); catch-up takes ~0.2–0.3 s; first frame on this link (Alibaba Cloud CDN) is ≈0.9 s, slightly later than without dropping (acceptable). Log evidence: `catchup: START drop (lag=2361ms)` → `catchup: STOP (lag=199ms)` → `First frame post=1`.

---

## Background & lifecycle (1.0.13)

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

## Size (after the 2026-10 trim)

| Artifact | Before trim | 1.0.0 | 1.0.1 | 1.0.2 | 1.0.3 | 1.0.4 |
|----------|-------------|-------|-------|-------|-------|-------|
| AAR (4 ABIs) | 82 MB | 29.4 MB | 34.1 MB | 58.2 MB | 59.6 MB | **59.6 MB** |
| `liblatencyplayer.so` (arm64, stripped) | 46.9 MB | 17.6 MB | 20.3 MB | 32.3 MB | 33.1 MB | **33.1 MB** |
| Demo APK (all ABIs) | 138 MB | 37.1 MB | 44.7 MB | 67.2 MB | 68.7 MB | **68.7 MB** |
| Integrator APK delta (single arm64) | - | ~+17.6 MB | ~+20.3 MB | ~+32.3 MB | ~+33.1 MB | ~+33.1 MB |

1.0.1 adds ~4.7 MB over 1.0.0, from the newly registered plugin stack: RTSP (`libgstrtsp` / `libgstsdp` / `libgstrtp` / `libgstrtpmanager`), HTTP (`libsoup-3.0` / `libpsl` / `libnghttp2`), HLS (`libgsthls` / `libgstmpegtsdemux` / `libgstisomp4` / `libgstaes`), G.711 and MP3 (`libgstmulaw` / `libgstalaw` / `libgstmpg123`).

1.0.2 adds ~24.1 MB over 1.0.1, from AAC decoding (`libgstlibav`) and its bundled FFmpeg static libs (`libavcodec` / `libavutil` / `libswresample` / `libavformat` / `libavfilter` / `libswscale` / `libbz2`) plus audio output (`libgstopensles`).

1.0.3 adds ~1.4 MB over 1.0.2, from native code fixed after the 1.0.2 release (vendored `gstdecodebin2.c` patch, HLS queue raw-bytes detection, stop×buffering race locking) and debug info.

1.0.4 is on par with 1.0.3, only adding the `window_mutex` lock code for the repeated-playback ANR fix (under 1 MB, no change after rounding).

1.0.5–1.0.13 are on par with 1.0.4: the period consisted of native code fixes (rtmpsrc unlock patch, appsink `sync=FALSE` + catch-up dropping, stall watchdog) and API additions, with no new linked libraries.

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
implementation("com.latencyplayer:latencyplayer-sdk:1.0.13")
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

Version: `LATENCYPLAYER_VERSION` in `gradle.properties` (currently `1.0.13`; Nexus `ALLOW_ONCE` forbids overwriting the same version, so upgrades must increment).

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

1. **Rapid stop→play switch may crash** — stream-thread caps use-after-free (teardown vs. bus callback race; stack in `gst_caps_features_set_parent_refcount` / `gst_caps_push`). Being fixed; **workaround: after stop, wait for the state to return to `STOPPED` before `start()`**.
2. **ABI is currently all 4** — the arm64-only slimming strategy is pending a decision (could save ~2/3 of the size).
3. **The device must stay awake while measuring** — dozing stops the Surface from rendering, so screenshots go all-black and latency measurement fails; a known issue was once misjudged as "steady-state latency ≥7 s" because of this.
4. ~~**HLS live (fMP4 multi-variant) segment download anomaly (found in 1.0.2)**~~ — **fixed and regression-passed (2/2 rendered; fix shipped in 1.0.3)**: the root cause was the low-latency `queue` tune setting `leaky` on **raw container byte streams**, dropping bytes so `qtdemux` reported `atom bogus size` → `adaptivedemux` restarted repeatedly. Fix: `queue_carries_raw_bytes()` walks the sink-pad upstream to decide whether the data is still raw container bytes (before Demuxer/Parser); raw streams are **not** made leaky. The HTTP-FLV first-read hang was eliminated in the 1.0.2 regression.
5. **RTMP intermittent no-video locally (found in 1.0.2, fixed in 1.0.3)** — root-cause chain: `multiqueue` overruns too early → `decodebin` exposes an audio-only group → the flvdemux thread blocks in `gst_data_queue_push()` and cannot parse video tags → the late video pad is permanently dropped with "No current group". Fix (vendored `gstdecodebin2.c` patch): on overrun, first raise the multiqueue limits to unblock the demuxer, then wait with a 2500 ms grace for the video pad to join and expose together with video; the low-latency tune no longer shrinks `multiqueue`'s `max-size-time` / `max-size-bytes` (otherwise async-done would push the raised limits back and the demuxer would block repeatedly). Local mediamtx RTMP regression **12/12 stable** (mechanism-log evidence for both the grace path and the natural-completion path).
6. ~~**`stop()` during playback races with buffering messages → SIGSEGV (found in 1.0.2)**~~ — **fixed and stress-passed (fix shipped in 1.0.3)**: the bus thread's `on_buffering` (live branch `set_state(PLAYING)`) and the UI thread's `latency_player_stop()` `set_state(NULL)` teardown ran concurrently; playbin `activate_group` accessed a freed object (tombstone: `SIGSEGV fault addr 0xaaaaaaaa`). Fix: all bus signal handlers (error / eos / state-changed / buffering / async-done) and pause / resume / setter / position query / snapshot capture uniformly hold `ctx->mutex`, serialized with play / stop / set_surface on the same lock — if stop finishes first, handlers see `pipeline==NULL` and return; otherwise handlers run first on a healthy pipeline (no deadlock: `set_state` does not wait for bus dispatch, and sync handlers / appsink callbacks do not hold this lock). Stress: HLS live 6× + RTMP 8× stops with buffering activity, 0 crashes 0 deadlocks; HLS VOD / RTSP / snapshot regression passed.
7. ~~**Repeated playback (tapping play again during playback / rapid stop→play) triggers an ANR "not responding" (found after 1.0.3)**~~ — **fixed (2026-10-04, shipped in 1.0.4)**: on the main thread, `latency_player_play()` holding `ctx->mutex` to tear down the old pipeline with `set_state(NULL)` had to wait for the appsink stream thread to exit rendering, while the stream thread's `on_appsink_new_sample → draw_frame_to_window` took the same `ctx->mutex` to fetch `native_window` → circular wait, main thread froze, system raised an ANR (repro: around the 4th repeated play tap; `latency_player_play+252 = core.c:614` stuck at `gst_base_sink_change_state → g_mutex_lock_slowpath`). Fix: a dedicated `window_mutex` protects only `native_window`/`window_geo_*` — `draw_frame_to_window` and `prepare-window-handle` hold only this lock, `set_surface`/`release` hold it nested within `ctx->mutex` (lock order `ctx->mutex → window_mutex`, one-directional, no path holds `window_mutex` across `set_state`), so the stream thread never touches `ctx->mutex` again. Stress: repeated play during playback 25× + rapid stop→play 25× + original-input rapid taps 20× = 70 playbacks, 0 deaths 0 ANRs 0 fatals; RTMP rendering smoke + RTSP / HLS / HTTP-FLV regression passed (HLS 1920x1080 rendered, snapshot confirmed).
8. ~~**SIGSEGV at the instant of stopping/switching an RTMP stream (`fault addr 0x38`, stack in `RTMP_ReadPacket`; reproduced twice within 3 minutes on a real `com.woned.daylong` device on 2026-10-08)**~~ — **fixed (shipped in 1.0.7), 0 crashes under real-device stress**: upstream gstrtmpsrc's `unlock()` (used by GstBaseSrc to interrupt a blocked read when tearing down the pipeline) calls `RTMP_Close()` directly; librtmp's `CloseInternal()` then `free()`s and nulls `r->m_vec` (the channel table) and clears the counts — while the stream thread is still in `RTMP_ReadPacket` executing `r->m_vec[channel]` (channel=7 → `7×8=0x38` → null dereference). Race window = **stopping/switching while packets are being received**. Fix (vendored `vendor/rtmp/gstrtmpsrc.c` patch): `unlock()` only `shutdown(socket)` to wake the blocked recv (does not free state the read thread is using; the fd is retained so there is no reuse window); the full `RTMP_Close+RTMP_Free` is deferred to `stop()` (called by GstBaseSrc after the stream thread joins; zero leaks); the `start()` connect-failure path adds Close (also fixing an upstream leak); JNI init ignores SIGPIPE (writing DeleteStream to an already-shutdown socket yields EPIPE, no longer killing the process). Trade-off: no auto-reconnect after flush-seek (this SDK forces RTMP `live=1` and does not seek, so no impact). Real-device stress: rapid stop→play 120 rounds (104 first-frames, 206 unlocks all hitting the race window) + Alibaba Cloud↔local channel-switch regression, 0 crashes.
9. ~~**Startup stays a constant ~2.5 s behind the live edge (located in 1.0.12, fixed in 1.0.13)**~~ — root cause: at connect the CDN bursts ~2.5 s of history video, and the old implementation buffered it whole and drained at 1×, never emptying (`multiqueue` self-reporting 0 is untrustworthy; the backlog is actually between `flvdemux→h264parse`). Fix: appsink `sync=FALSE` + callback **catch-up dropping** (see "Latency → Startup backlog"). Measured `pipe_backlog` 2550 ms → 40–200 ms.
10. ~~**Black screen after backgrounding / being frozen and returning (fixed in 1.0.13)**~~ — scenario: backgrounding destroys the SurfaceView's surface and stops drawing; returning rebuilds and rebinds the surface. Short backgrounding recovers in a second, but after a longer background period (MIUI freezes background processes) the pipeline stalls — the RTMP source still receives bytes, but video frames stop flowing, and the screen stays black after returning. Fix: native records "age of the latest video frame" (new API `getLastFrameAgeMs()`); the SDK `ReconnectManager` checks every second during playback and forces a reconnect after >3 s with no new frame; measured ~750 ms to render after a 22 s background.

Fixed: steady-state latency misjudgment (measured ~125 ms, see [Latency](#latency)); repeated "play" taps double-free (`gst_object_ref_sink`); `.so` symbol/link integrity (`--no-undefined` + Bionic stubs); HLS live raw-byte leak; RTMP intermittent no-video; snapshot file writing (PNG/JPEG); stop×buffering race SIGSEGV during playback; repeated-playback ANR deadlock (draw and main-thread teardown waiting on each other's `ctx->mutex`); RTMP stop/switch `m_vec` NULL SIGSEGV (1.0.7 rtmpsrc unlock patch); startup CDN burst backlog (1.0.13 appsink `sync=FALSE` + catch-up dropping); black screen after background return (1.0.13 stall watchdog).

---

## Important constraints

1. **Do not set an opaque background on `LatencyPlayerView`** (e.g. `android:background="#000000"`), otherwise the Surface hole is covered and the screen goes black; the View already does `setBackgroundColor(0)` + `RGBA_8888` internally.
2. For low latency, use `.bufferTime(0)` **and** `.lowLatency(true)` together.
3. Capabilities are primarily **RTMP**; RTSP / HTTP-FLV / HLS (VOD + live) passed regression in 1.0.3, but in production prefer RTMP — see [doc/INTEGRATION.en.md](doc/INTEGRATION.en.md).
4. After changing the config you must call `setConfig()` again for it to take effect.

---

## License

Proprietary (closed-source license). Licenses for GStreamer and its dependencies are in the LICENSE files bundled with `NDK-CMake-GStreamer`.
