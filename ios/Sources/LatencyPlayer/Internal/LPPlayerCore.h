//
//  LPPlayerCore.h
//  Playback engine: RTMP transport + FLV parsing + H.264 decode + low-latency
//  tuning, stall detection and auto-reconnect.
//
//  The core is deliberately UI-agnostic: it notifies `LPPlayerCoreDelegate`
//  (implemented by LatencyPlayerManager) and never touches UIKit itself.
//

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import "LatencyPlayerConfig.h"
#import "LatencyPlayerState.h"
#import "LatencyPlayerError.h"

NS_ASSUME_NONNULL_BEGIN

@class LPPlayerCore;

@protocol LPPlayerCoreDelegate <NSObject>

- (void)core:(LPPlayerCore *)core didChangeState:(LatencyPlayerState)state;
- (void)core:(LPPlayerCore *)core didFailWithError:(LatencyPlayerErrorCode)code message:(NSString *)message;
- (void)core:(LPPlayerCore *)core didUpdateBuffering:(int)percent;
- (void)coreDidConnect:(LPPlayerCore *)core;
- (void)coreDidDisconnect:(LPPlayerCore *)core;
- (void)coreDidRenderFirstFrame:(LPPlayerCore *)core;
- (void)core:(LPPlayerCore *)core didUpdateDownloadSpeed:(int64_t)bytesPerSec;
- (void)core:(LPPlayerCore *)core didReconnectAttempt:(int)attempt;
- (void)core:(LPPlayerCore *)core didChangeVideoSize:(CGSize)size;
- (void)coreDidCompletePlayback:(LPPlayerCore *)core;

@end

@interface LPPlayerCore : NSObject

- (instancetype)initWithConfig:(LatencyPlayerConfig *)config
                      delegate:(id<LPPlayerCoreDelegate>)delegate;

@property (nonatomic, readonly) LatencyPlayerState state;
@property (nonatomic, readonly) LatencyPlayerConfig *config;

/// The layer decoded frames are fed to (set from the LatencyPlayerView).
@property (nonatomic, weak, nullable) AVSampleBufferDisplayLayer *displayLayer;

- (void)startWithURL:(NSString *)url;
- (void)stop;
- (void)pause;
- (void)resume;
- (void)switchToURL:(NSString *)url;

- (void)applyConfig:(LatencyPlayerConfig *)config;

- (void)setMute:(BOOL)mute;
- (void)setVolume:(int)volume;
- (void)setLowLatency:(BOOL)lowLatency;
- (void)setBufferTimeMs:(int)bufferTimeMs;

- (BOOL)saveSnapshotToPath:(NSString *)path;
- (long)lastFrameAgeMs;

- (void)shutdown;

@end

NS_ASSUME_NONNULL_END
