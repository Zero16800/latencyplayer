//
//  LPPlayerCore.m
//

#import "LPPlayerCore.h"
#import "LPLog.h"
#import "LPRtmpClient.h"
#import "LPFLVParser.h"
#import "LPH264Decoder.h"

#import <UIKit/UIKit.h>
#import <CoreImage/CoreImage.h>
#import <CoreVideo/CoreVideo.h>

// Low-latency thresholds — kept in sync with the Android SDK.
static const int64_t kStallThresholdMs = 3000;
static const NSTimeInterval kStallCheckInterval = 1.0;
static const int64_t kCatchUpEnterMs = 600;
// (kCatchUpExitMs == 200 on Android; on iOS we resync at the next keyframe.)

@interface LPPlayerCore () <LPRtmpClientDelegate, LPFLVParserDelegate, LPH264DecoderDelegate>
@property (nonatomic, strong, readwrite) LatencyPlayerConfig *config;
@property (nonatomic, assign, readwrite) LatencyPlayerState state;
@end

@implementation LPPlayerCore {
    __weak id<LPPlayerCoreDelegate> _delegate;
    NSRecursiveLock                 *_lock;

    LPRtmpClient                    *_client;
    LPFLVParser                     *_parser;
    LPH264Decoder                   *_decoder;

    NSString                        *_currentURL;
    NSString                        *_pendingURL;

    BOOL                             _running;
    BOOL                             _paused;
    BOOL                             _reconnecting;
    BOOL                             _firstFrameRendered;
    BOOL                             _catchingUp;
    BOOL                             _hasDecodedPts;
    int64_t                          _lastDecodedPtsMs;

    CFAbsoluteTime                   _sessionStartTime;
    CFAbsoluteTime                   _lastFrameWallTime;

    int                              _reconnectAttempt;

    dispatch_queue_t                 _timerQueue;
    dispatch_source_t                _stallTimer;

    CVPixelBufferRef                 _lastPixelBuffer;
}

- (instancetype)initWithConfig:(LatencyPlayerConfig *)config
                      delegate:(id<LPPlayerCoreDelegate>)delegate
{
    self = [super init];
    if (self) {
        _config = [config copy];
        _delegate = delegate;
        _lock = [[NSRecursiveLock alloc] init];
        _state = LatencyPlayerStateIdle;

        _parser = [[LPFLVParser alloc] init];
        _parser.delegate = self;

        _decoder = [[LPH264Decoder alloc] init];
        _decoder.delegate = self;
        _decoder.preferHardware = _config.hardwareDecoder;

        _timerQueue = dispatch_queue_create("com.latencyplayer.timer", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (void)dealloc
{
    [self stopStallTimer];
    if (_lastPixelBuffer) {
        CVPixelBufferRelease(_lastPixelBuffer);
        _lastPixelBuffer = NULL;
    }
}

#pragma mark - Config

- (void)applyConfig:(LatencyPlayerConfig *)config
{
    [_lock lock];
    self.config = [config copy];
    _decoder.preferHardware = self.config.hardwareDecoder;
    [_lock unlock];
}

#pragma mark - State helpers

- (void)setStateInternal:(LatencyPlayerState)state
{
    [_lock lock];
    if (_state == state) {
        [_lock unlock];
        return;
    }
    _state = state;
    [_lock unlock];
    [_delegate core:self didChangeState:state];
}

- (LatencyPlayerState)state
{
    [_lock lock];
    LatencyPlayerState s = _state;
    [_lock unlock];
    return s;
}

#pragma mark - Session control

- (void)startWithURL:(NSString *)url
{
    if (url.length == 0) {
        [self reportError:LatencyPlayerErrorRTMPConnectFailed message:@"Empty URL"];
        return;
    }

    [_lock lock];
    if (_running) {
        [_lock unlock];
        return;
    }
    _running = YES;
    _paused = NO;
    _reconnecting = NO;
    _firstFrameRendered = NO;
    _catchingUp = NO;
    _hasDecodedPts = NO;
    _reconnectAttempt = 0;
    _currentURL = [url copy];
    _sessionStartTime = CFAbsoluteTimeGetCurrent();
    _lastFrameWallTime = _sessionStartTime;
    _state = LatencyPlayerStateBuffering;
    [_lock unlock];

    [_delegate core:self didChangeState:LatencyPlayerStateBuffering];
    [_delegate core:self didUpdateBuffering:0];

    [self startClientWithURL:url];
    [self startStallTimer];
}

- (void)startClientWithURL:(NSString *)url
{
    LPRtmpClient *client = [[LPRtmpClient alloc] initWithURL:url];
    client.delegate = self;
    client.bufferTimeMs = self.config.lowLatency ? 0 : self.config.bufferTime;

    [_parser reset];
    [_decoder invalidate];
    _decoder.preferHardware = self.config.hardwareDecoder;

    [_lock lock];
    _client = client;
    [_lock unlock];

    [client start];
}

- (void)stop
{
    LPRtmpClient *old;
    [_lock lock];
    if (!_running) {
        [_lock unlock];
        return;
    }
    _running = NO;
    _paused = NO;
    old = _client;
    _client = nil;
    [_lock unlock];

    [self stopClient:old];
    [self stopStallTimer];
    [self invalidateDecoderAndParser];

    [self setStateInternal:LatencyPlayerStateStopped];
}

- (void)stopClient:(LPRtmpClient *)client
{
    if (client == nil) {
        return;
    }
    client.delegate = nil;
    [client stop];   // waits for the reader to unwind, including in-flight work
}

- (void)pause
{
    [_lock lock];
    if (!_running) {
        [_lock unlock];
        return;
    }
    _paused = YES;
    [_lock unlock];
    [self setStateInternal:LatencyPlayerStatePaused];
}

- (void)resume
{
    [_lock lock];
    if (!_running) {
        [_lock unlock];
        return;
    }
    _paused = NO;
    _catchingUp = YES;   // resync to the next keyframe
    _hasDecodedPts = NO;
    [_lock unlock];
    [_decoder flush];
    [self setStateInternal:LatencyPlayerStatePlaying];
}

- (void)switchToURL:(NSString *)url
{
    [_lock lock];
    BOOL wasRunning = _running;
    _paused = NO;
    _reconnecting = NO;
    _firstFrameRendered = NO;
    _catchingUp = NO;
    _hasDecodedPts = NO;
    _reconnectAttempt = 0;
    _currentURL = [url copy];
    _sessionStartTime = CFAbsoluteTimeGetCurrent();
    _lastFrameWallTime = _sessionStartTime;
    [_lock unlock];

    if (!wasRunning) {
        [self startWithURL:url];
        return;
    }

    [self restartClientWithURL:url];
    [self setStateInternal:LatencyPlayerStateBuffering];
    [_delegate core:self didUpdateBuffering:0];
    [self startStallTimer];
}

- (void)restartClientWithURL:(NSString *)url
{
    LPRtmpClient *old;
    [_lock lock];
    old = _client;
    _client = nil;
    [_lock unlock];

    [self stopClient:old];
    [self invalidateDecoderAndParser];
    [self startClientWithURL:url];
}

- (void)invalidateDecoderAndParser
{
    [_parser reset];
    [_decoder invalidate];
}

#pragma mark - Reconnect

- (void)scheduleReconnect
{
    [_lock lock];
    if (!_running || _reconnecting) {
        [_lock unlock];
        return;
    }
    if (!self.config.autoReconnect) {
        [_lock unlock];
        [self reportError:LatencyPlayerErrorNetworkTimeout message:@"Stream stalled"];
        return;
    }
    _reconnecting = YES;
    _reconnectAttempt += 1;
    int attempt = _reconnectAttempt;
    int maxCount = self.config.maxReconnectCount;
    NSString *url = _currentURL;
    [_lock unlock];

    if (maxCount > 0 && attempt > maxCount) {
        LPLogW(@"Reconnect attempts exhausted (%d)", attempt);
        [self reportError:LatencyPlayerErrorNetworkTimeout message:@"Reconnect attempts exhausted"];
        [self stop];
        return;
    }

    [_delegate core:self didReconnectAttempt:attempt];

    NSTimeInterval delay = MAX(0.0, (NSTimeInterval)self.config.reconnectInterval);
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        [weakSelf performReconnect:url];
    });
}

- (void)performReconnect:(NSString *)url
{
    [_lock lock];
    BOOL stillRunning = _running;
    _firstFrameRendered = NO;
    _catchingUp = NO;
    _hasDecodedPts = NO;
    _sessionStartTime = CFAbsoluteTimeGetCurrent();
    _lastFrameWallTime = _sessionStartTime;
    _reconnecting = NO;
    [_lock unlock];

    if (!stillRunning) {
        return;
    }

    LPLogI(@"Reconnecting (attempt %d) -> %@", _reconnectAttempt, url);
    [self restartClientWithURL:url];
    [self setStateInternal:LatencyPlayerStateBuffering];
}

#pragma mark - Stall watchdog

- (void)startStallTimer
{
    [self stopStallTimer];
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _timerQueue);
    dispatch_source_set_timer(timer,
                              dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kStallCheckInterval * NSEC_PER_SEC)),
                              (uint64_t)(kStallCheckInterval * NSEC_PER_SEC),
                              (uint64_t)(0.2 * NSEC_PER_SEC));
    __weak typeof(self) weakSelf = self;
    dispatch_source_set_event_handler(timer, ^{ [weakSelf checkStall]; });
    _stallTimer = timer;
    dispatch_resume(timer);
}

- (void)stopStallTimer
{
    if (_stallTimer) {
        dispatch_source_cancel(_stallTimer);
        _stallTimer = nil;
    }
}

- (void)checkStall
{
    [_lock lock];
    BOOL running = _running;
    BOOL paused = _paused;
    BOOL reconnecting = _reconnecting;
    BOOL first = _firstFrameRendered;
    CFAbsoluteTime ref = first ? _lastFrameWallTime : _sessionStartTime;
    [_lock unlock];

    if (!running || paused || reconnecting) {
        return;
    }

    double ageMs = (CFAbsoluteTimeGetCurrent() - ref) * 1000.0;
    if (first && ageMs > (double)kStallThresholdMs) {
        LPLogW(@"Stall detected: no video frame for %.0f ms", ageMs);
        // Reset the reference so we do not re-trigger while reconnecting.
        [_lock lock];
        _lastFrameWallTime = CFAbsoluteTimeGetCurrent();
        [_lock unlock];
        [self scheduleReconnect];
    }
}

#pragma mark - Errors

- (void)reportError:(LatencyPlayerErrorCode)code message:(NSString *)message
{
    [_delegate core:self didFailWithError:code message:message];
}

#pragma mark - Public controls

- (void)setMute:(BOOL)mute
{
    // Audio rendering is not implemented yet; the flag is retained for parity.
    [_lock lock];
    self.config.mute = mute;
    [_lock unlock];
}

- (void)setVolume:(int)volume
{
    [_lock lock];
    self.config.volume = volume;
    [_lock unlock];
}

- (void)setLowLatency:(BOOL)lowLatency
{
    [_lock lock];
    self.config.lowLatency = lowLatency;
    [_lock unlock];
}

- (void)setBufferTimeMs:(int)bufferTimeMs
{
    [_lock lock];
    self.config.bufferTime = bufferTimeMs;
    [_lock unlock];
}

- (long)lastFrameAgeMs
{
    [_lock lock];
    BOOL running = _running;
    BOOL first = _firstFrameRendered;
    CFAbsoluteTime last = _lastFrameWallTime;
    [_lock unlock];

    if (!running || !first) {
        return -1;
    }
    return (long)((CFAbsoluteTimeGetCurrent() - last) * 1000.0);
}

- (void)shutdown
{
    [self stop];
    [_lock lock];
    _delegate = nil;
    [_lock unlock];
}

#pragma mark - Snapshot

- (BOOL)saveSnapshotToPath:(NSString *)path
{
    CVPixelBufferRef buffer;
    [_lock lock];
    buffer = _lastPixelBuffer;
    if (buffer) {
        CVPixelBufferRetain(buffer);
    }
    [_lock unlock];

    if (buffer == NULL) {
        LPLogW(@"saveSnapshot: no frame available yet");
        return NO;
    }

    BOOL ok = NO;
    @autoreleasepool {
        CIImage *ciImage = [CIImage imageWithCVPixelBuffer:buffer];
        CIContext *context = [CIContext context];
        CGImageRef cgImage = [context createCGImage:ciImage fromRect:ciImage.extent];
        if (cgImage) {
            UIImage *image = [UIImage imageWithCGImage:cgImage];
            CFRelease(cgImage);
            NSData *data = nil;
            if ([[path.pathExtension lowercaseString] isEqualToString:@"png"]) {
                data = UIImagePNGRepresentation(image);
            } else {
                data = UIImageJPEGRepresentation(image, 0.9);
            }
            ok = [data writeToFile:path atomically:YES];
        }
    }

    CVPixelBufferRelease(buffer);
    return ok;
}

#pragma mark - LPRtmpClientDelegate

- (BOOL)isCurrentClient:(LPRtmpClient *)client
{
    [_lock lock];
    BOOL current = (client == _client) && _running;
    [_lock unlock];
    return current;
}

- (void)rtmpClientDidConnect:(LPRtmpClient *)client
{
    if (![self isCurrentClient:client]) {
        return;
    }
    [_lock lock];
    _reconnectAttempt = 0;
    [_lock unlock];
    [_delegate coreDidConnect:self];
    [self setStateInternal:LatencyPlayerStatePlaying];
}

- (void)rtmpClientDidDisconnect:(LPRtmpClient *)client
{
    if (![self isCurrentClient:client]) {
        return;
    }
    [_delegate coreDidDisconnect:self];
    [self scheduleReconnect];
}

- (void)rtmpClient:(LPRtmpClient *)client didReadFLVData:(NSData *)data
{
    if (![self isCurrentClient:client]) {
        return;
    }
    [_parser appendData:data];
}

- (void)rtmpClient:(LPRtmpClient *)client didUpdateDownloadSpeed:(int64_t)bytesPerSec
{
    if (![self isCurrentClient:client]) {
        return;
    }
    [_delegate core:self didUpdateDownloadSpeed:bytesPerSec];
}

- (void)rtmpClient:(LPRtmpClient *)client
    didFailWithError:(LatencyPlayerErrorCode)code
             message:(NSString *)message
{
    if (![self isCurrentClient:client]) {
        return;
    }
    LPLogE(@"RTMP client error %ld: %@", (long)code, message);
    [self scheduleReconnect];
}

#pragma mark - LPFLVParserDelegate

- (void)flvParser:(LPFLVParser *)parser didReceiveVideoSequenceHeader:(NSData *)avcc
{
    if (parser != _parser) {
        return;
    }
    if (![_decoder configureWithAVCDecoderConfigurationRecord:avcc]) {
        [self reportError:LatencyPlayerErrorVideoCodecNotSupported message:@"Unsupported H.264 stream"];
        return;
    }
    // New format description: drop anything still queued for display.
    [self.displayLayer flush];
}

- (void)flvParser:(LPFLVParser *)parser
    didReceiveVideoFrame:(NSData *)nalu
              isKeyframe:(BOOL)isKeyframe
             timestampMs:(int64_t)timestampMs
{
    if (parser != _parser) {
        return;
    }

    [_lock lock];
    BOOL paused = _paused;
    BOOL catchingUp = _catchingUp;
    BOOL hasPts = _hasDecodedPts;
    int64_t lastPts = _lastDecodedPtsMs;
    [_lock unlock];

    if (paused || !_decoder.isConfigured) {
        return;
    }

    // (Re)start only from a keyframe.
    if (!hasPts || catchingUp) {
        if (!isKeyframe) {
            [_lock lock];
            _catchingUp = YES;
            [_lock unlock];
            return;
        }
        [_decoder flush];
        [_lock lock];
        _catchingUp = NO;
        _hasDecodedPts = YES;
        _lastDecodedPtsMs = timestampMs;
        [_lock unlock];
        [_decoder decodeAccessUnit:nalu ptsMs:timestampMs isKeyframe:YES];
        return;
    }

    int64_t backlog = timestampMs - lastPts;
    if (backlog > kCatchUpEnterMs) {
        // Too far behind: drop until the next keyframe.
        [_lock lock];
        _catchingUp = YES;
        [_lock unlock];
        [_decoder flush];
        [self.displayLayer flush];
        return;
    }

    [_lock lock];
    _lastDecodedPtsMs = timestampMs;
    [_lock unlock];
    [_decoder decodeAccessUnit:nalu ptsMs:timestampMs isKeyframe:isKeyframe];
}

- (void)flvParser:(LPFLVParser *)parser didReceiveAudioSequenceHeader:(NSData *)asc
{
    // TODO(audio): configure AAC decoder (AudioConverter) — not yet implemented.
    (void)parser; (void)asc;
}

- (void)flvParser:(LPFLVParser *)parser didReceiveAudioFrame:(NSData *)aac timestampMs:(int64_t)timestampMs
{
    // TODO(audio): decode & play PCM — not yet implemented.
    (void)parser; (void)aac; (void)timestampMs;
}

- (void)flvParser:(LPFLVParser *)parser didEncounterUnsupportedVideoCodec:(uint8_t)codecId
{
    if (parser != _parser) {
        return;
    }
    [self reportError:LatencyPlayerErrorVideoCodecNotSupported
              message:[NSString stringWithFormat:@"Unsupported video codec (%u)", codecId]];
}

#pragma mark - LPH264DecoderDelegate

- (void)h264Decoder:(LPH264Decoder *)decoder didDecodeSampleBuffer:(CMSampleBufferRef)sampleBuffer
{
    if (decoder != _decoder) {
        return;
    }

    [_lock lock];
    BOOL paused = _paused;
    BOOL firstFrame = !_firstFrameRendered;
    [_lock unlock];

    if (paused) {
        return;
    }

    // Keep the newest decoded frame for saveSnapshot.
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (imageBuffer) {
        [_lock lock];
        if (_lastPixelBuffer) {
            CVPixelBufferRelease(_lastPixelBuffer);
        }
        _lastPixelBuffer = (CVPixelBufferRef)CFRetain(imageBuffer);
        [_lock unlock];
    }

    AVSampleBufferDisplayLayer *layer = self.displayLayer;
    if (layer != nil) {
        if (layer.status == AVQueuedSampleBufferRenderingStatusFailed) {
            [layer flush];
        }
        if (layer.readyForMoreMediaData) {
            [layer enqueueSampleBuffer:sampleBuffer];
        }
        // If the layer is not ready we drop the frame to keep latency bounded.
    }

    [_lock lock];
    _lastFrameWallTime = CFAbsoluteTimeGetCurrent();
    [_lock unlock];

    if (firstFrame) {
        [_lock lock];
        _firstFrameRendered = YES;
        [_lock unlock];
        [_delegate core:self didUpdateBuffering:100];
        [self setStateInternal:LatencyPlayerStatePlaying];
        [_delegate coreDidRenderFirstFrame:self];
    }
}

- (void)h264Decoder:(LPH264Decoder *)decoder didChangeVideoSize:(CGSize)size
{
    if (decoder != _decoder) {
        return;
    }
    [_delegate core:self didChangeVideoSize:size];
}

- (void)h264Decoder:(LPH264Decoder *)decoder didFailWithError:(NSError *)error
{
    if (decoder != _decoder) {
        return;
    }
    LPLogE(@"H.264 decoder error: %@", error.localizedDescription);
    [self reportError:LatencyPlayerErrorVideoDecodeFailed message:error.localizedDescription];
}

@end
