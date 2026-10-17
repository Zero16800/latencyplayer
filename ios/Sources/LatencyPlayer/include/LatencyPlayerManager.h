//
//  LatencyPlayerManager.h
//  LatencyPlayer iOS SDK
//
//  RTMP player core. Mirrors com.latencyplayer.sdk.LatencyPlayerManager.
//
//  Method mapping vs. Android (only where the platform forbids an exact match):
//    init(Context)           -> -initialize
//    setSurface(LatencyPlayerView) -> -setSurface:
//    setSurface(Surface)     -> -setRenderView:  (UIView host)
//    release()               -> -releasePlayer   ("release" is reserved under ARC)
//
//  All callbacks are delivered on the main thread, as on Android.
//
//  Usage:
//    LatencyPlayerManager *player = [[LatencyPlayerManager alloc] init];
//    [player initialize];
//    [player setConfig:[LatencyPlayerConfig build:^(LatencyPlayerConfig *c) {
//        c.url = @"rtmp://example.com/live/stream";
//        c.bufferTime = 200;
//    }]];
//    [player setSurface:playerView];
//    [player setCallback:self];
//    [player start];
//

#import <Foundation/Foundation.h>
#import "LatencyPlayerConfig.h"
#import "LatencyPlayerCallback.h"
#import "LatencyPlayerState.h"
#import "LatencyPlayerView.h"

NS_ASSUME_NONNULL_BEGIN

@interface LatencyPlayerManager : NSObject

/// Convenience factory (equivalent to `[[LatencyPlayerManager alloc] init]`).
+ (instancetype)player;

/// 初始化播放器（对应 Android `init(Context)`）
- (void)initialize;

/// 设置播放器配置
- (void)setConfig:(LatencyPlayerConfig *)config;
/// 获取当前配置
- (LatencyPlayerConfig *)getConfig;
@property (nonatomic, readonly) LatencyPlayerConfig *config;

/// 设置播放回调
- (void)setCallback:(nullable id<LatencyPlayerCallback>)callback;
@property (nonatomic, weak, nullable) id<LatencyPlayerCallback> callback;

/// 设置播放 View（对应 Android `setSurface(LatencyPlayerView)`）
- (void)setSurface:(LatencyPlayerView *)view;
/// 直接绑定渲染视图（对应 Android `setSurface(Surface)` 的 iOS 等价物）
- (void)setRenderView:(nullable UIView *)view;

/// 开始播放
- (void)start;
/// 停止播放
- (void)stop;
/// 暂停播放
- (void)pause;
/// 恢复播放
- (void)resume;

/// 设置静音
- (void)setMute:(BOOL)mute;
/// 是否静音
- (BOOL)isMute;

/// 设置音量 (0-100)
- (void)setVolume:(int)volume;
/// 设置缓冲时间 (ms, 0-5000)
- (void)setBufferTime:(int)bufferTimeMs;
/// 热切换 URL
- (void)switchUrl:(NSString *)newUrl;

/// 截图（PNG 或 JPEG 由 savePath 后缀决定），返回是否写盘成功
- (BOOL)saveSnapshot:(NSString *)savePath;

/// 设置播放方向 1=竖屏, 2=横屏
- (void)setOrientation:(int)orientation;
/// 设置视频翻转
- (void)setFlip:(BOOL)flipHorizontal vertical:(BOOL)flipVertical;
/// 设置视频旋转角度 0/90/180/270
- (void)setRotation:(int)degrees;

/// 是否正在播放
- (BOOL)isPlaying;
/// 获取当前状态
- (LatencyPlayerState)getState;
@property (nonatomic, readonly) LatencyPlayerState state;

/// 最近一次收到视频帧的年龄（毫秒），-1 表示本次播放尚未收到任何帧。
- (long)getLastFrameAgeMs;

/// 释放播放器资源（对应 Android `release()`）
- (void)releasePlayer;

@end

NS_ASSUME_NONNULL_END
