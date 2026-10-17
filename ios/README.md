# LatencyPlayer iOS SDK

> English docs: [README.en.md](README.en.md)。

面向 iOS 的轻量原生 RTMP 直播播放器，API 与 Android SDK（`com.latencyplayer.sdk`）对齐。交付形式为 **XCFramework** 与 **Swift Package**。

状态：**脚手架 / 首版**。视频链路（RTMP → FLV → H.264 → AVSampleBufferDisplayLayer）、完整公开 API、低延迟调优、卡死看门狗与自动重连均已实现。**音频尚未实现**（AAC 帧已解析但未解码——见下文 *状态*）。

## 架构

```
LPRtmpClient     私有串行队列上运行 vendored librtmp（阻塞式）；
                 RTMP_Read 产出 FLV 形态的字节
      │
      ▼
LPFLVParser      增量 FLV tag 解析器 → H.264 AVCC 访问单元
      │
      ▼
LPH264Decoder    VideoToolbox（VTDecompressionSession）→ CMSampleBuffer
      │
      ▼
AVSampleBufferDisplayLayer（由 LatencyPlayerView 承载）
      │
      ▼
LPPlayerCore     状态机、低延迟追帧、3s 卡死看门狗、自动重连；
                 通知 LPPlayerCoreDelegate
      │
      ▼
LatencyPlayerManager   公开 API；在主线程派发回调
```

librtmp 的纯 RTMP 握手需要 HMAC-SHA256 后端；在 iOS 上映射到 Apple 的 **CommonCrypto**（不使用 OpenSSL）。见 `third_party/librtmp/LATENCYPLAYER_PATCHES.md`。

## 目录结构

```
ios/
├── Package.swift                 # SPM（source targets：CLibrtmp + LatencyPlayer）
├── project.yml                   # XcodeGen 规格（framework + demo app）
├── scripts/build-xcframework.sh  # 产出 LatencyPlayer.xcframework
├── Sources/
│   ├── CLibrtmp                 -> third_party/librtmp（target 路径）
│   └── LatencyPlayer/
│       ├── include/              # 公开头文件（即 SDK API）
│       └── Internal/             # 引擎实现
├── third_party/librtmp/          # vendored librtmp（LGPL-2.1）+ CommonCrypto 补丁
└── Demo/                         # 最小 UIKit 示例
```

## 构建

> 需要 macOS + Xcode。无法在 Windows 上编译本 SDK。

### XCFramework

```sh
brew install xcodegen
./scripts/build-xcframework.sh
# => build/LatencyPlayer.xcframework
```

在另一个项目的 `Package.swift` 中引用：

```swift
.binaryTarget(name: "LatencyPlayer",
              path: "Vendor/LatencyPlayer.xcframework")
```

### Swift Package（开发用）

源码包通过 `unsafeFlags(["-fobjc-arc"])` 启用 ARC，因此面向本地开发 / 作为根包使用，不适合作为远程依赖。分发请使用 XCFramework。

## 用法

见 `Demo/ViewController.swift`：

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

## Android ⇄ iOS API 映射

iOS API 沿用 Android 的命名，仅在平台不允许完全一致时才有所不同：

| Android                                  | iOS                                    |
|------------------------------------------|----------------------------------------|
| `init(Context)`                          | `-initialize`                          |
| `setSurface(LatencyPlayerView)`          | `-setSurface:`                         |
| `setSurface(Surface)`                    | `-setRenderView:`（`UIView`）          |
| `release()`                              | `-releasePlayer`（`release` 为保留字） |
| `LatencyPlayerState` 枚举                | `LatencyPlayerState`（`NS_ENUM`）      |
| `LatencyPlayerError` 常量                | `LatencyPlayerErrorCode`（`NS_ENUM`）  |
| `LatencyPlayerConfig.builder()...build()` | `+[LatencyPlayerConfig build:]` block  |
| `LatencyPlayerCallback` 接口             | `LatencyPlayerCallback` 协议           |

其余成员（`start`/`stop`/`pause`/`resume`/`setMute:`/`isMute`/`setVolume:`/`setBufferTime:`/`switchUrl:`/`saveSnapshot:`/`setOrientation:`/`setFlip:vertical:`/`setRotation:`/`isPlaying`/`getState`/`getLastFrameAgeMs`）一一对应。回调在主线程派发，与 Android 一致。

## 低延迟行为（与 Android 同策略）

* **追帧 / 丢弃：** 若到来的时间戳比最后一个已解码帧落后超过 **600 ms**，则丢弃帧直到下一个关键帧（只跳过期帧，绝不迟到渲染）。
* **卡死看门狗：** 引擎每 **1 s** 检查一次最近视频帧的年龄；超过 **3 s** 无帧即触发自动重连。
* **`getLastFrameAgeMs`：** 最近一次视频帧的年龄，本次会话首帧之前返回 `-1`。

## 状态

已实现：
* RTMP 连接/播放 + 重连、FLV 解析、VideoToolbox H.264 解码、AVSampleBufferDisplayLayer 渲染、追帧、卡死看门狗、截图。
* 完整公开 API + 回调。

尚未实现：
* **音频**（AAC → PCM → AVAudioEngine/AudioQueue）。配置开关会被接受，但目前无实际发声效果。
* RTSP / 本地文件输入（与 Android 的差距）。
* FLV seq header 中除 SPS/PPS 之外使用非 AVCC 参数集的 H.264 流，以及纯音频流。

## 许可证

本 SDK 链接 **librtmp**，其为 LGPL-2.1（见 `third_party/librtmp/COPYING`）。分发二进制时须保留声明，并按 LGPL 要求提供重新链接的能力。
