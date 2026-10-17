# LatencyPlayer SDK API Reference

## Overview

LatencyPlayer SDK is an Android live-streaming player SDK built on **GStreamer 1.28.7** (the current main path is RTMP ingest), exposing Java APIs under `com.latencyplayer.sdk`.

> 中文文档见 [API.md](API.md)。

**Note:** Rendering uses `LatencyPlayerView` (a `SurfaceView`). Do **not** set an opaque background (e.g. `#000000`) on this View in the layout, otherwise the screen goes black.

---

## Core classes

### LatencyPlayerManager

The core player management class, providing playback, pause, stop, volume, snapshot and more.

#### Methods

| Method | Description |
|--------|-------------|
| `init(Context)` | Initialize (Application Context); must be called first; repeated init is ignored; cannot init again after `release()` |
| `setConfig(LatencyPlayerConfig)` | Set configuration and push `bufferTime` / `lowLatency` / `mute` / `volume` down to native |
| `getConfig()` | Get the current configuration (`@NonNull`) |
| `setCallback(LatencyPlayerCallback)` | Set the event callback (nullable) |
| `setSurface(LatencyPlayerView)` | Bind the rendering View (recommended) |
| `setSurface(Surface)` | Bind an Android `Surface` directly |
| `start()` | Start playback; **requires** `setConfig` first and a non-empty `url` |
| `stop()` | Stop playback |
| `pause()` | Pause |
| `resume()` | Resume |
| `setMute(boolean)` | Mute / unmute |
| `isMute()` | Whether muted |
| `setVolume(int)` | Volume 0–100 (clamped internally) |
| `setBufferTime(int)` | Change buffer time at runtime (ms), range 0–5000, 0 = lowest latency |
| `switchUrl(String)` | Hot-switch the URL (internally stop + play) |
| `saveSnapshot(String)` | Snapshot save path; `.png`→PNG, `.jpg`/`.jpeg`→JPEG (quality=90); returns `boolean` success |
| `setOrientation(int)` | 1 = portrait, 2 = landscape |
| `setFlip(boolean, boolean)` | Horizontal / vertical flip |
| `setRotation(int)` | Rotation 0 / 90 / 180 / 270 |
| `isPlaying()` | Whether the state is `PLAYING` |
| `getState()` | The current `LatencyPlayerState` |
| `getLastFrameAgeMs()` | Age of the most recently received video frame (ms); `-1` = no frame received yet this session. Lets callers detect whether video has stopped flowing after backgrounding / being frozen (the SDK's `ReconnectManager` uses it for a >3 s stall watchdog). Added in 1.0.13 |
| `release()` | Release native resources; may be called repeatedly; the instance must not be used afterwards |

#### Usage example

```java
LatencyPlayerManager player = new LatencyPlayerManager();
player.init(context.getApplicationContext());

LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url("rtmp://example.com/live/stream")
    .bufferTime(0)
    .lowLatency(true)
    .fastStartup(true)
    .build();

player.setConfig(config);
player.setSurface(playerView); // LatencyPlayerView
player.setCallback(callback);
player.start();
```

---

### LatencyPlayerConfig

A builder-pattern configuration class.

#### Configuration options

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `url` | String | null | Playback URL; required before `start()` |
| `bufferTime` | int | 200 | Buffer time (ms), 0–5000; `setBufferTime` / the setter clamps; **0 = lowest latency** |
| `fastStartup` | boolean | true | Fast-startup flag (a business-layer option) |
| `lowLatency` | boolean | false | Ultra-low-latency mode; pushed via `setConfig` → `nativeSetLowLatency` (flattens internal queues etc.) |
| `hardwareDecoder` | boolean | true | Prefer-hardware-decode flag |
| `autoReconnect` | boolean | true | Auto-reconnect (`ReconnectManager`) |
| `reconnectInterval` | int | 3 | Reconnect interval (seconds) |
| `maxReconnectCount` | int | 0 | Max reconnect attempts, 0 = unlimited |
| `orientation` | int | 1 | 1 = portrait, 2 = landscape |
| `mute` | boolean | false | Mute |
| `volume` | int | 100 | Volume 0–100 (setter clamps) |

> The builder's `bufferTime(int)` does **not** clamp on write; `setBufferTime` / the corresponding setter clamps to 0–5000.

#### Builder example

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url("rtmp://example.com/live/stream")
    .bufferTime(0)           // lowest latency
    .fastStartup(true)
    .lowLatency(true)        // ultra-low latency
    .hardwareDecoder(true)
    .autoReconnect(true)
    .reconnectInterval(5)
    .maxReconnectCount(0)
    .build();
```

---

### LatencyPlayerView

A `SurfaceView` for rendering.

- Internally `setBackgroundColor(0)` + holder `setFormat(RGBA_8888)`
- **Do not set an opaque `background` in XML**
- Reports Surface lifecycle to the Manager via `setOnSurfaceReadyListener`
- `getSurfaceHolder()` returns the current holder (may be null)

#### Recommended XML

```xml
<com.latencyplayer.sdk.LatencyPlayerView
    android:id="@+id/player_view"
    android:layout_width="match_parent"
    android:layout_height="300dp" />
```

---

### LatencyPlayerCallback

All callbacks run on the **main thread**.

| Method | Description |
|--------|-------------|
| `onStateChanged(LatencyPlayerState)` | State change |
| `onError(int, String)` | Error (code in `LatencyPlayerError`, message from `getDescription`) |
| `onBuffering(int)` | Buffering progress 0–100% |
| `onConnected()` | Connection established (corresponds to STATE_CONNECTED) |
| `onDisconnected()` | Connection lost |
| `onFirstFrameRendered()` | First frame rendered (usually also transitions to PLAYING) |
| `onDownloadSpeed(long)` | Download speed (bytes/s) |
| `onReconnecting(int)` | Reconnecting (attempt number) |
| `onVideoSizeChanged(int, int)` | Video width/height (native delivers these as separate params to avoid 32-bit truncation) |
| `onPlaybackCompleted()` | Playback completed (non-live / EOS) |

---

### LatencyPlayerState

| State | value | Description |
|-------|-------|-------------|
| `IDLE` | 0 | Idle, not initialized |
| `INITIALIZING` | 1 | Initializing |
| `READY` | 2 | Ready |
| `BUFFERING` | 3 | Buffering / starting |
| `PLAYING` | 4 | Playing |
| `PAUSED` | 5 | Paused |
| `STOPPING` | 6 | Stopping |
| `STOPPED` | 7 | Stopped |
| `ERROR` | 8 | Error |

`fromValue(int)`: unknown values fall back to `IDLE`.

---

### LatencyPlayerError

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `NONE` | No error |
| -1 | `UNKNOWN` | Unknown error |
| 1001 | `NETWORK_CONNECT_FAILED` | Network connection failed |
| 1002 | `NETWORK_TIMEOUT` | Network timeout |
| 1003 | `RTMP_CONNECT_FAILED` | RTMP connection failed |
| 1004 | `RTMP_HANDSHAKE_FAILED` | RTMP handshake failed |
| 1005 | `RTMP_STREAM_NOT_FOUND` | Stream not found |
| 1006 | `RTMP_AUTH_FAILED` | Authentication failed |
| 2001 | `VIDEO_DECODE_FAILED` | Video decode failed |
| 2002 | `VIDEO_CODEC_NOT_SUPPORTED` | Unsupported video codec |
| 2003 | `AUDIO_DECODE_FAILED` | Audio decode failed |
| 2004 | `AUDIO_CODEC_NOT_SUPPORTED` | Unsupported audio codec |
| 3001 | `RENDER_INIT_FAILED` | Render initialization failed |
| 3002 | `SURFACE_INVALID` | Invalid Surface |
| 4001 | `OUT_OF_MEMORY` | Out of memory |
| 5001 | `GSTREAMER_INIT_FAILED` | GStreamer initialization failed |
| 5002 | `PIPELINE_CREATE_FAILED` | Pipeline creation failed |

`getDescription(int)` returns a description string.

---

## Native interface (internal, do not depend on directly)

The `native*` methods declared on `LatencyPlayerManager` (such as `nativeSetLowLatency`, `nativeSetBufferTime`, `nativeSetSurface`) are JNI implementation details and should be used through the higher-level API.
