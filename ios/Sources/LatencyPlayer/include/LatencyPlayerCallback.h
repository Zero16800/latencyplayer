//
//  LatencyPlayerCallback.h
//  LatencyPlayer iOS SDK
//
//  Player event callback protocol. Mirrors the Android interface
//  com.latencyplayer.sdk.LatencyPlayerCallback (same methods, invoked on the
//  main thread).
//

#import <Foundation/Foundation.h>
#import "LatencyPlayerState.h"
#import "LatencyPlayerError.h"

NS_ASSUME_NONNULL_BEGIN

@protocol LatencyPlayerCallback <NSObject>

@required

/// 播放状态变化回调
- (void)onStateChanged:(LatencyPlayerState)state;

/// 错误回调
- (void)onError:(LatencyPlayerErrorCode)errorCode message:(NSString *)message;

/// 缓冲进度回调 (0-100)
- (void)onBuffering:(int)percent;

/// 连接成功回调
- (void)onConnected;

/// 连接断开回调
- (void)onDisconnected;

/// 首帧渲染回调
- (void)onFirstFrameRendered;

/// 下载速度回调 (bytes/sec)
- (void)onDownloadSpeed:(int64_t)bytesPerSec;

/// 正在重连回调
- (void)onReconnecting:(int)attemptCount;

/// 视频尺寸变化回调
- (void)onVideoSizeChanged:(int)width height:(int)height;

/// 播放完成回调（仅用于非直播场景）
- (void)onPlaybackCompleted;

@end

NS_ASSUME_NONNULL_END
