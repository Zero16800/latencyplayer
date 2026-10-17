# LatencyPlayer iOS SDK

> 中文文档见 [README.md](README.md)。

A lightweight, native RTMP live player for iOS with an API that mirrors the
Android SDK (`com.latencyplayer.sdk`). Delivered as an **XCFramework** and a
**Swift Package**.

Status: **scaffolding / first pass.** The video pipeline (RTMP → FLV → H.264 →
AVSampleBufferDisplayLayer), the full public API surface, low-latency tuning,
the stall watchdog and auto-reconnect are implemented. **Audio is not yet
implemented** (AAC frames are parsed but not decoded — see *Status* below).

## Architecture

```
LPRtmpClient     vendored librtmp (blocking) on a private serial queue;
                 RTMP_Read yields FLV-shaped bytes
      │
      ▼
LPFLVParser      incremental FLV tag parser → H.264 AVCC access units
      │
      ▼
LPH264Decoder    VideoToolbox (VTDecompressionSession) → CMSampleBuffer
      │
      ▼
AVSampleBufferDisplayLayer (hosted by LatencyPlayerView)
      │
      ▼
LPPlayerCore     state machine, low-latency catch-up, 3s stall watchdog,
                 auto-reconnect; notifies LPPlayerCoreDelegate
      │
      ▼
LatencyPlayerManager   public API; dispatches callbacks on the main thread
```

librtmp needs an HMAC-SHA256 backend for the plain-RTMP handshake; on iOS it is
mapped onto Apple's **CommonCrypto** (no OpenSSL). See
`third_party/librtmp/LATENCYPLAYER_PATCHES.md`.

## Layout

```
ios/
├── Package.swift                 # SPM (source targets: CLibrtmp + LatencyPlayer)
├── project.yml                   # XcodeGen spec (framework + demo app)
├── scripts/build-xcframework.sh  # produce LatencyPlayer.xcframework
├── Sources/
│   ├── CLibrtmp                 -> third_party/librtmp (target path)
│   └── LatencyPlayer/
│       ├── include/              # public headers (the SDK API)
│       └── Internal/             # engine implementation
├── third_party/librtmp/          # vendored librtmp (LGPL-2.1) + CommonCrypto patch
└── Demo/                         # minimal UIKit sample
```

## Building

> Requires macOS + Xcode. The SDK cannot be compiled on Windows.

### XCFramework

```sh
brew install xcodegen
./scripts/build-xcframework.sh
# => build/LatencyPlayer.xcframework
```

Consume it in another project's `Package.swift`:

```swift
.binaryTarget(name: "LatencyPlayer",
              path: "Vendor/LatencyPlayer.xcframework")
```

### Swift Package (development)

The source package requests ARC via `unsafeFlags(["-fobjc-arc"])`, so it is
intended for local development / as a root package, not as a remote dependency.
For distribution, use the XCFramework.

## Usage

See `Demo/ViewController.swift`:

```swift
let player = LatencyPlayerManager()
player.initialize()
let config = LatencyPlayerConfig.build { c in
    c.url = "rtmp://host/app/stream"
    c.bufferTime = 200
    c.autoReconnect = true
    c.reconnectInterval = 3
}
player.setConfig(config)
player.setSurface(playerView)   // LatencyPlayerView
player.setCallback(self)
player.start()
```

## Android ⇄ iOS API mapping

The iOS API keeps Android's names. Only where the platform forbids an exact
match does it differ:

| Android                                | iOS                                  |
|----------------------------------------|--------------------------------------|
| `init(Context)`                        | `-initialize`                        |
| `setSurface(LatencyPlayerView)`        | `-setSurface:`                       |
| `setSurface(Surface)`                  | `-setRenderView:` (a `UIView`)       |
| `release()`                            | `-releasePlayer` (`release` is reserved) |
| `LatencyPlayerState` enum              | `LatencyPlayerState` (`NS_ENUM`)     |
| `LatencyPlayerError` constants         | `LatencyPlayerErrorCode` (`NS_ENUM`) |
| `LatencyPlayerConfig.builder()...build()` | `+[LatencyPlayerConfig build:]` block |
| `LatencyPlayerCallback` interface      | `LatencyPlayerCallback` protocol     |

All other members (`start`/`stop`/`pause`/`resume`/`setMute:`/`isMute`/
`setVolume:`/`setBufferTime:`/`switchUrl:`/`saveSnapshot:`/`setOrientation:`/
`setFlip:vertical:`/`setRotation:`/`isPlaying`/`getState`/`getLastFrameAgeMs`)
match one-to-one. Callbacks are delivered on the main thread, as on Android.

## Low-latency behaviour (same policy as Android)

* **Catch-up / drop:** if the incoming timestamp falls more than **600 ms**
  behind the last decoded frame, frames are dropped until the next keyframe
  (only stale frames are skipped; nothing is rendered late).
* **Stall watchdog:** every **1 s** the engine checks the age of the last video
  frame; more than **3 s** without a frame triggers auto-reconnect.
* **`getLastFrameAgeMs`:** age of the most recent video frame, or `-1` before
  the first frame of the current session.

## Status

Implemented:
* RTMP connect/play + reconnect, FLV parsing, VideoToolbox H.264 decode,
  AVSampleBufferDisplayLayer rendering, catch-up, stall watchdog, snapshot.
* Full public API + callbacks.

Not yet implemented:
* **Audio** (AAC → PCM → AVAudioEngine/AudioQueue). Config flags are accepted
  but currently have no audible effect.
* RTSP / local file inputs (Android parity gap).
* H.264 streams with non-AVCC parameter sets beyond SPS/PPS in the FLV seq
  header, and audio-only streams.

## License

The SDK links **librtmp**, which is LGPL-2.1 (see
`third_party/librtmp/COPYING`). Keep the notice and provide relinking ability
as required by the LGPL when distributing binaries.
