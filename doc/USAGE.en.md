# LatencyPlayer SDK Usage Guide

> 中文文档见 [USAGE.md](USAGE.md)。

## Quick start

### 1. Add the dependency

Add the repository in the root project `settings.gradle.kts` (see [MAVEN_INTEGRATION.en.md](MAVEN_INTEGRATION.en.md) for details):

```kotlin
maven {
    url = uri("http://192.168.3.160:8970/repository/maven-releases/")
    isAllowInsecureProtocol = true
}
```

App module `build.gradle.kts`:

```kotlin
dependencies {
    implementation("com.latencyplayer:latencyplayer-sdk:1.0.13")
}
```

(Source form in the same repo: `implementation(project(":sdk"))`; offline AAR: `implementation(files("libs/sdk-release.aar"))`)

### 2. Permissions

`AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

### 3. Layout

**Do not set an opaque background on `LatencyPlayerView`** (`#000000` etc. will cause a black screen).

```xml
<com.latencyplayer.sdk.LatencyPlayerView
    android:id="@+id/player_view"
    android:layout_width="match_parent"
    android:layout_height="300dp" />
```

### 4. Java code

```java
public class MainActivity extends AppCompatActivity {

    private LatencyPlayerManager player;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        player = new LatencyPlayerManager();
        player.init(getApplicationContext());

        LatencyPlayerConfig config = LatencyPlayerConfig.builder()
                .url("rtmp://your-server/live/stream")
                .bufferTime(200)        // or 0 for lowest live latency
                .lowLatency(false)
                .hardwareDecoder(true)
                .fastStartup(true)
                .autoReconnect(true)
                .build();

        player.setConfig(config);

        LatencyPlayerView playerView = findViewById(R.id.player_view);
        player.setSurface(playerView);

        player.setCallback(new LatencyPlayerCallback() {
            @Override
            public void onStateChanged(LatencyPlayerState state) {
                Log.d("Player", "State: " + state);
            }

            @Override
            public void onError(int errorCode, String message) {
                Log.e("Player", "Error: " + message);
            }

            @Override
            public void onFirstFrameRendered() {
                Log.d("Player", "First frame rendered");
            }

            // implement the remaining callbacks as needed
        });

        player.start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (player != null) {
            player.release();
        }
    }
}
```

`LatencyPlayerCallback` is a complete interface; you must implement all methods (or wrap it in your own adapter class).

---

## Common features

### Playback control

```java
player.start();
player.stop();
player.pause();
player.resume();
```

### Volume

```java
player.setMute(true);
player.setMute(false);
player.setVolume(50);   // 0-100
boolean muted = player.isMute();
```

### Hot URL switch

```java
player.switchUrl("rtmp://new-url/live/stream2");
```

### Snapshot

```java
boolean ok = player.saveSnapshot("/sdcard/screenshot.png");   // PNG (.png)
boolean ok2 = player.saveSnapshot("/sdcard/screenshot.jpg");  // JPEG (.jpg/.jpeg, quality=90)
```

The encoder is chosen by the path suffix (`pngenc` / `jpegenc`); returns whether the write succeeded; returns `false` when there is no picture.

### Flip / rotate / orientation

```java
player.setFlip(true, false);     // horizontal flip
player.setRotation(90);          // 90°
player.setOrientation(2);        // 2=landscape (also written into config)
```

### Change buffering at runtime

```java
player.setBufferTime(0);         // updates config + native
```

---

## Configuration options

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url("rtmp://...")
    .bufferTime(200)              // buffer (ms), 0 = lowest latency
    .fastStartup(true)
    .lowLatency(false)            // true flattens internal pipeline queues
    .hardwareDecoder(true)
    .autoReconnect(true)
    .reconnectInterval(3)
    .maxReconnectCount(0)         // 0 = unlimited
    .orientation(1)
    .mute(false)
    .volume(100)
    .build();
```

After changing the config, call `player.setConfig(config)` again so it is pushed to native (at minimum it needs to cover bufferTime / lowLatency / mute / volume).

---

## Callback events

```java
player.setCallback(new LatencyPlayerCallback() {
    void onStateChanged(LatencyPlayerState state) {}
    void onError(int code, String msg) {}
    void onBuffering(int percent) {}
    void onConnected() {}
    void onDisconnected() {}
    void onFirstFrameRendered() {}
    void onDownloadSpeed(long bytesPerSec) {}
    void onReconnecting(int attemptCount) {}
    void onVideoSizeChanged(int w, int h) {}
    void onPlaybackCompleted() {}
});
```

---

## Lowest-latency configuration

```java
LatencyPlayerConfig config = LatencyPlayerConfig.builder()
    .url(url)
    .bufferTime(0)
    .lowLatency(true)
    .fastStartup(true)
    .build();
player.setConfig(config);
player.start();
```

Notes:

- With `lowLatency=true`, native tries to flatten `queue2` / `multiqueue` / internal queues and shortens the playbin buffer duration.
- Since 1.0.13, appsink uses `sync=FALSE` and **directly drops old frames that lag behind the live edge to catch up** (never renders stale frames, no "fast-forward" feel), so after startup/recovery it **skips the historical frames burst by the CDN** and hugs the live edge; this is expected behaviour.
- In live mode (`rtmp://` / `rtsp://`), buffering does **not** stall progress-bar logic the way VOD does.
- End-to-end latency is also affected by the CDN / the encoder's GOP on the push side / transport; the SDK mainly squeezes the client-side buffer and queues.

---

## Error handling

```java
player.setCallback(new LatencyPlayerCallback() {
    @Override
    public void onError(int errorCode, String message) {
        switch (errorCode) {
            case LatencyPlayerError.NETWORK_CONNECT_FAILED:
                // check the network
                break;
            case LatencyPlayerError.RTMP_STREAM_NOT_FOUND:
                // check the URL
                break;
            case LatencyPlayerError.VIDEO_DECODE_FAILED:
                LatencyPlayerConfig c = player.getConfig();
                c.setHardwareDecoder(false);
                player.setConfig(c);   // setConfig is required to push/refresh
                player.stop();
                player.start();
                break;
        }
    }
});
```

> Merely calling `config.setHardwareDecoder(...)` does **not** notify native automatically; call `setConfig` again.

---

## FAQ

| Symptom | Resolution |
|---------|------------|
| Black screen but state PLAYING | Check whether the layout sets an opaque background on `LatencyPlayerView` |
| Crash on initialization | Confirm `GSTREAMER_ROOT_ANDROID`, the ABI, and that the `.so` is packaged into the APK |
| Slow first frame | Use `bufferTime(0)` + `lowLatency(true)` + `fastStartup(true)`. **First frame ≈ pipeline fixed overhead ~0.3 s + waiting for the source's next keyframe (0~GOP, mean GOP/2)** — measured locally on RTMP: avg 0.90 s when source GOP=1 s, avg 0.355 s when source GOP=0.2 s; the most effective lever is **shortening the encoder GOP** / having the server start sending quickly from a keyframe (the SDK cannot bypass the keyframe wait) |
| Crash from repeated "play" taps | Fixed (v 2026-09); previously a video-sink floating reference caused a double-free |
| Crash on rapid stop→play | **Known issue**: stream-thread caps use-after-free, being fixed; workaround is to wait for the state to return to STOPPED before start (note this differs from the "stop during playback × buffering SIGSEGV", which is already fixed) |
| Occasional SIGSEGV when stopping during playback (playbin `activate_group` / fault `0xaaaaaaaa`) | **Fixed**: bus handlers such as `on_buffering` and `stop()` are now serialized on the same `ctx->mutex`; stress: HLS live 6 + RTMP 8 stops with buffering activity, 0 crashes; see README Known Issues #6 |
| ANR "not responding" after repeated playback (tap play during playback / rapid stop→play) | **Fixed (shipped in 1.0.4)**: the stream thread's `draw_frame_to_window` used to take `ctx->mutex` in the appsink callback, waiting on the main thread's `play()` holding the lock to tear down the old pipeline → deadlock ANR; now `native_window` uses a dedicated `window_mutex` and the stream thread no longer touches `ctx->mutex`. Stress: 70 playbacks, 0 ANRs; see README Known Issues #7 |
| Crash at the instant of switching/stopping an RTMP stream (`fault addr 0x38`, stack in `RTMP_ReadPacket`) | **Fixed (shipped in 1.0.7)**: upstream rtmpsrc's `unlock()` called `RTMP_Close` while interrupting a blocked read, freeing the `m_vec` the read thread was still using; the 1.0.7 vendored patch changes unlock to only `shutdown` the socket and defers Close to `stop()`. Real-device stress: 120 rounds, 0 crashes; see README Known Issues #8 |
| HTTP-FLV / HLS no picture after startup | HTTP-FLV: **regression-passed in 1.0.3** (souphttpsrc first-read hang not reproduced); HLS VOD + **live both regression-passed** (**1.0.3 fix**, see README Known Issues #4) |
| RTMP has audio but no video | **Fixed in 1.0.3** (decodebin overrun grace patch, local regression 12/12), see README Known Issues #5 |
| Stays ~2.5 s behind the source after startup (CDN burst) | **Fixed in 1.0.13**: the CDN burst at connect caused a whole-segment backlog; after appsink `sync=FALSE` + catch-up dropping, `pipe_backlog` dropped to 40–200 ms; see README "Latency → Startup backlog" |
| Black screen after backgrounding / being frozen and returning | **Fixed in 1.0.13**: auto-reconnect when video stops flowing >3 s (`getLastFrameAgeMs` stall watchdog); measured ~750 ms to render after a 22 s background; see README Known Issues #10 |

---

## Artifact size (2026-10)

- AAR (4 ABIs): **59.6 MB** (on par since 1.0.4 through 1.0.13; 58.2 MB in 1.0.2, 34.1 MB in 1.0.1, 82 MB before trimming)
- arm64 `liblatencyplayer.so`: **33.1 MB** (on par since 1.0.4 through 1.0.13; 32.3 MB in 1.0.2, 20.3 MB in 1.0.1, 46.9 MB before trimming)
- Demo APK: **68.7 MB** (all ABIs, on par since 1.0.4 through 1.0.13)

> 1.0.5–1.0.13 only contain native code fixes and API additions, no new linked libraries, so the size is on par with 1.0.4.

> The 1.0.2 size increase came from AAC decoding (`libgstlibav` + FFmpeg static libs) and audio output (`libgstopensles`).

## 16 KB page size (Android 15+)

Supported: 64-bit `.so` LOAD segments are `0x4000`-aligned and `zipalign -P 16` passes. Integrators need to do nothing.
