//
//  LatencyPlayerManager.m
//

#import "LatencyPlayerManager.h"
#import "Internal/LPPlayerCore.h"
#import "Internal/LPLog.h"

#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#import <math.h>

BOOL LPLogEnabled = YES;

double LatencyPlayerVersionNumber = 1.0;
const unsigned char LatencyPlayerVersionString[] = "1.0.13";

@interface LatencyPlayerManager () <LPPlayerCoreDelegate, LatencyPlayerSurfaceReadyListener>
@property (nonatomic, strong, readwrite) LatencyPlayerConfig *config;
@property (nonatomic, assign, readwrite) LatencyPlayerState state;
@end

@implementation LatencyPlayerManager {
    LPPlayerCore                *_core;
    LatencyPlayerView           *_playerView;
    AVSampleBufferDisplayLayer  *_externalLayer;

    BOOL                         _initialized;
    BOOL                         _released;

    int                          _rotationDegrees;
    BOOL                         _flipHorizontal;
    BOOL                         _flipVertical;
}

+ (instancetype)player
{
    return [[self alloc] init];
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _state = LatencyPlayerStateIdle;
    }
    return self;
}

- (void)dealloc
{
    [_core shutdown];
    _core = nil;
}

#pragma mark - Init / lifecycle

- (void)initialize
{
    if (_initialized) {
        LPLogW(@"initialize: already initialized");
        return;
    }
    if (_released) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Player has been released, cannot reinitialize"];
    }

    if (_config == nil) {
        _config = [LatencyPlayerConfig config];
    }

    _core = [[LPPlayerCore alloc] initWithConfig:self.config delegate:self];
    if (_playerView != nil) {
        _core.displayLayer = _playerView.displayLayer;
    }
    if (_externalLayer != nil) {
        _core.displayLayer = _externalLayer;
    }

    _initialized = YES;
    [self setState:LatencyPlayerStateReady];
}

- (void)setConfig:(LatencyPlayerConfig *)config
{
    [self checkInitialized];
    _config = config;   // deliberately kept by reference, as on Android
    [_core applyConfig:config];
}

- (LatencyPlayerConfig *)getConfig
{
    [self checkInitialized];
    return self.config;
}

- (void)setCallback:(id<LatencyPlayerCallback>)callback
{
    _callback = callback;
}

#pragma mark - Surface

- (void)setSurface:(LatencyPlayerView *)view
{
    [self checkInitialized];
    _playerView = view;
    [view setOnSurfaceReadyListener:self];
    if (view.isSurfaceReady) {
        _core.displayLayer = view.displayLayer;
    }
}

- (void)setRenderView:(UIView *)view
{
    [self checkInitialized];

    if (view == nil) {
        _core.displayLayer = nil;
        [_externalLayer removeFromSuperlayer];
        _externalLayer = nil;
        return;
    }

    if (_externalLayer == nil) {
        _externalLayer = [[AVSampleBufferDisplayLayer alloc] init];
        _externalLayer.videoGravity = AVLayerVideoGravityResizeAspect;
        _externalLayer.backgroundColor = [UIColor blackColor].CGColor;
    }
    _externalLayer.frame = view.bounds;
    [view.layer addSublayer:_externalLayer];
    _core.displayLayer = _externalLayer;
    [self applyVideoTransform];
}

- (void)onSurfaceReady
{
    _core.displayLayer = _playerView.displayLayer;
    [self applyVideoTransform];
}

- (void)onSurfaceChanged:(int)width height:(int)height
{
    (void)width; (void)height;
}

- (void)onSurfaceDestroyed
{
    _core.displayLayer = nil;
}

#pragma mark - Transport

- (void)start
{
    [self checkInitialized];
    if (self.config.url.length == 0) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"URL not set, call setConfig() first"];
    }
    [self setState:LatencyPlayerStateBuffering];
    [_core startWithURL:self.config.url];
}

- (void)stop
{
    [self checkInitialized];
    [_core stop];
    [self setState:LatencyPlayerStateStopped];
}

- (void)pause
{
    [self checkInitialized];
    [_core pause];
    [self setState:LatencyPlayerStatePaused];
}

- (void)resume
{
    [self checkInitialized];
    [_core resume];
    [self setState:LatencyPlayerStatePlaying];
}

- (void)switchUrl:(NSString *)newUrl
{
    [self checkInitialized];
    self.config.url = newUrl;
    [_core switchToURL:newUrl];
}

#pragma mark - Audio / config knobs

- (void)setMute:(BOOL)mute
{
    [self checkInitialized];
    self.config.mute = mute;
    [_core setMute:mute];
}

- (BOOL)isMute
{
    return self.config != nil && self.config.mute;
}

- (void)setVolume:(int)volume
{
    [self checkInitialized];
    [self.config setVolume:volume];
    [_core setVolume:self.config.volume];
}

- (void)setBufferTime:(int)bufferTimeMs
{
    [self checkInitialized];
    [self.config setBufferTime:bufferTimeMs];
    [_core setBufferTimeMs:self.config.bufferTime];
}

#pragma mark - Snapshot

- (BOOL)saveSnapshot:(NSString *)savePath
{
    [self checkInitialized];
    return [_core saveSnapshotToPath:savePath];
}

#pragma mark - Render transforms

- (void)setOrientation:(int)orientation
{
    [self checkInitialized];
    self.config.orientation = orientation;
}

- (void)setFlip:(BOOL)flipHorizontal vertical:(BOOL)flipVertical
{
    [self checkInitialized];
    _flipHorizontal = flipHorizontal;
    _flipVertical = flipVertical;
    [self applyVideoTransform];
}

- (void)setRotation:(int)degrees
{
    [self checkInitialized];
    _rotationDegrees = degrees;
    [self applyVideoTransform];
}

- (void)applyVideoTransform
{
    AVSampleBufferDisplayLayer *layer = (_playerView.displayLayer != nil)
        ? _playerView.displayLayer
        : _externalLayer;
    if (layer == nil) {
        return;
    }
    CATransform3D transform = CATransform3DIdentity;
    transform = CATransform3DRotate(transform, (CGFloat)_rotationDegrees * M_PI / 180.0, 0, 0, 1);
    transform = CATransform3DScale(transform,
                                   _flipHorizontal ? -1.0 : 1.0,
                                   _flipVertical ? -1.0 : 1.0,
                                   1.0);
    layer.transform = transform;
}

#pragma mark - State / queries

- (BOOL)isPlaying
{
    return self.state == LatencyPlayerStatePlaying;
}

- (LatencyPlayerState)getState
{
    return self.state;
}

- (long)getLastFrameAgeMs
{
    if (!_initialized || _core == nil) {
        return -1;
    }
    return [_core lastFrameAgeMs];
}

- (void)releasePlayer
{
    if (_released) {
        return;
    }
    LPLogD(@"Releasing player");
    [_core shutdown];
    _core = nil;
    if (_externalLayer != nil) {
        [_externalLayer removeFromSuperlayer];
        _externalLayer = nil;
    }
    _released = YES;
    _initialized = NO;
}

#pragma mark - Internals

- (void)checkInitialized
{
    if (!_initialized) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Player not initialized, call initialize first"];
    }
    if (_released) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Player has been released"];
    }
}

- (void)setState:(LatencyPlayerState)state
{
    if (self.state == state) {
        return;
    }
    _state = state;

    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onStateChanged:state];
        });
    }
}

#pragma mark - LPPlayerCoreDelegate

- (void)core:(LPPlayerCore *)core didChangeState:(LatencyPlayerState)state
{
    [self setState:state];
}

- (void)core:(LPPlayerCore *)core didFailWithError:(LatencyPlayerErrorCode)code message:(NSString *)message
{
    (void)core;
    [self setState:LatencyPlayerStateError];
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onError:code message:message];
        });
    }
}

- (void)core:(LPPlayerCore *)core didUpdateBuffering:(int)percent
{
    (void)core;
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onBuffering:percent];
        });
    }
}

- (void)coreDidConnect:(LPPlayerCore *)core
{
    (void)core;
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onConnected];
        });
    }
}

- (void)coreDidDisconnect:(LPPlayerCore *)core
{
    (void)core;
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onDisconnected];
        });
    }
}

- (void)coreDidRenderFirstFrame:(LPPlayerCore *)core
{
    (void)core;
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onFirstFrameRendered];
        });
    }
}

- (void)core:(LPPlayerCore *)core didUpdateDownloadSpeed:(int64_t)bytesPerSec
{
    (void)core;
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onDownloadSpeed:bytesPerSec];
        });
    }
}

- (void)core:(LPPlayerCore *)core didReconnectAttempt:(int)attempt
{
    (void)core;
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onReconnecting:attempt];
        });
    }
}

- (void)core:(LPPlayerCore *)core didChangeVideoSize:(CGSize)size
{
    (void)core;
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        int w = (int)size.width;
        int h = (int)size.height;
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onVideoSizeChanged:w height:h];
        });
    }
}

- (void)coreDidCompletePlayback:(LPPlayerCore *)core
{
    (void)core;
    [self setState:LatencyPlayerStateStopped];
    id<LatencyPlayerCallback> callback = self.callback;
    if (callback != nil) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [callback onPlaybackCompleted];
        });
    }
}

@end
