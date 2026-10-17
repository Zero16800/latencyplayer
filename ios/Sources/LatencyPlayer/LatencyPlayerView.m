//
//  LatencyPlayerView.m
//

#import "LatencyPlayerView.h"

@interface LatencyPlayerView ()
@property (nonatomic, strong) AVSampleBufferDisplayLayer *displayLayer;
@property (nonatomic, weak) id<LatencyPlayerSurfaceReadyListener> surfaceReadyListener;
@property (nonatomic, assign) BOOL surfaceReadyNotified;
@end

@implementation LatencyPlayerView

+ (Class)layerClass
{
    // Use a plain container layer; the AVSampleBufferDisplayLayer is a sublayer.
    return [CALayer class];
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (void)commonInit
{
    self.backgroundColor = [UIColor clearColor];
    self.userInteractionEnabled = NO;

    _displayLayer = [[AVSampleBufferDisplayLayer alloc] init];
    _displayLayer.videoGravity = AVLayerVideoGravityResizeAspect;
    _displayLayer.backgroundColor = [UIColor blackColor].CGColor;
    _displayLayer.frame = self.bounds;
    [self.layer addSublayer:_displayLayer];
}

#pragma mark - Surface lifecycle

- (void)layoutSubviews
{
    [super layoutSubviews];
    self.displayLayer.frame = self.bounds;
    [self notifySurfaceReadyIfNeeded];
    if (self.surfaceReadyNotified && self.bounds.size.width > 0 && self.bounds.size.height > 0) {
        [self.surfaceReadyListener onSurfaceChanged:(int)self.bounds.size.width
                                             height:(int)self.bounds.size.height];
    }
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    if (self.window == nil) {
        self.surfaceReadyNotified = NO;
        [self.surfaceReadyListener onSurfaceDestroyed];
    } else {
        [self notifySurfaceReadyIfNeeded];
    }
}

- (void)notifySurfaceReadyIfNeeded
{
    if (self.surfaceReadyNotified) {
        return;
    }
    if (self.bounds.size.width <= 0 || self.bounds.size.height <= 0) {
        return;
    }
    self.surfaceReadyNotified = YES;
    [self.surfaceReadyListener onSurfaceReady];
}

#pragma mark - Public

- (void)setOnSurfaceReadyListener:(id<LatencyPlayerSurfaceReadyListener>)listener
{
    self.surfaceReadyListener = listener;
    // Surface may already be ready before the listener was registered.
    [self notifySurfaceReadyIfNeeded];
}

- (BOOL)isSurfaceReady
{
    return self.bounds.size.width > 0 && self.bounds.size.height > 0 && self.window != nil;
}

- (void)setVideoGravity:(AVLayerVideoGravity)videoGravity
{
    _videoGravity = [videoGravity copy];
    self.displayLayer.videoGravity = videoGravity;
}

- (AVLayerVideoGravity)videoGravity
{
    return _videoGravity ?: AVLayerVideoGravityResizeAspect;
}

@end
