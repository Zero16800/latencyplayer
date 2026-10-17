//
//  LatencyPlayerConfig.h
//  LatencyPlayer iOS SDK
//
//  Player configuration. Mirrors com.latencyplayer.sdk.LatencyPlayerConfig
//  (same fields and defaults; the Java builder is offered as a block builder).
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/** Default buffer time (ms). */
FOUNDATION_EXPORT const int LatencyPlayerDefaultBufferTime;      // 200
/** Default reconnect interval (s). */
FOUNDATION_EXPORT const int LatencyPlayerDefaultReconnectInterval; // 3

@interface LatencyPlayerConfig : NSObject <NSCopying>

/// 播放地址. Playback URL.
@property (nonatomic, copy, nullable) NSString *url;

/// 缓冲时间，范围 0-5000ms，0 为最低延迟. Clamped to [0, 5000].
@property (nonatomic, assign) int bufferTime;              // default 200

/// 是否开启首屏秒开. default YES
@property (nonatomic, assign) BOOL fastStartup;

/// 是否开启超低延迟模式. default NO
@property (nonatomic, assign) BOOL lowLatency;

/// 是否硬解码优先. default YES
@property (nonatomic, assign) BOOL hardwareDecoder;

/// 是否自动重连. default YES
@property (nonatomic, assign) BOOL autoReconnect;

/// 重连间隔(秒). default 3
@property (nonatomic, assign) int reconnectInterval;

/// 最大重连次数，0 表示无限重连. default 0
@property (nonatomic, assign) int maxReconnectCount;

/// 屏幕方向 1=竖屏, 2=横屏. default 1
@property (nonatomic, assign) int orientation;

/// 是否静音. default NO
@property (nonatomic, assign) BOOL mute;

/// 音量 0-100. Clamped to [0, 100]. default 100
@property (nonatomic, assign) int volume;

/// `new LatencyPlayerConfig()`
+ (instancetype)config;

/// `new LatencyPlayerConfig(url)`
+ (instancetype)configWithURL:(nullable NSString *)url;

/// Block-builder equivalent of `LatencyPlayerConfig.builder()...build()`.
///
///     LatencyPlayerConfig *c = [LatencyPlayerConfig build:^(LatencyPlayerConfig *c) {
///         c.url = @"rtmp://...";
///         c.bufferTime = 200;
///     }];
+ (instancetype)build:(void (^)(LatencyPlayerConfig *config))block;

@end

NS_ASSUME_NONNULL_END
