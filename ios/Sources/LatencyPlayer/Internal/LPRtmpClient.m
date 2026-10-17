//
//  LPRtmpClient.m
//

#import "LPRtmpClient.h"
#import "LPLog.h"

#import "rtmp.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static const int kLPReadBufferSize = 64 * 1024;

@interface LPRtmpClient () {
    NSString                *_url;
    dispatch_queue_t         _queue;
    dispatch_semaphore_t     _done;
    RTMP                    *_rtmp;
    BOOL                     _running;
    int64_t                  _bytesThisSecond;
    CFAbsoluteTime           _speedWindowStart;
    LatencyPlayerErrorCode   _lastErrorCode;
    NSString                *_lastErrorMessage;
}
@end

@implementation LPRtmpClient

- (instancetype)initWithURL:(NSString *)url
{
    self = [super init];
    if (self) {
        _url = [url copy];
        _queue = dispatch_queue_create("com.latencyplayer.rtmp", DISPATCH_QUEUE_SERIAL);
        _done = dispatch_semaphore_create(0);
        _timeoutSeconds = 10;
        _bufferTimeMs = 0;
        _lastErrorCode = LatencyPlayerErrorNone;
        _lastErrorMessage = nil;
    }
    return self;
}

- (void)dealloc
{
    // Ensure the reader is stopped before we go away.
    [self stop];
}

- (BOOL)isRunning
{
    return _running;
}

#pragma mark - Lifecycle

- (void)start
{
    if (_running) {
        return;
    }
    _running = YES;
    _lastErrorCode = LatencyPlayerErrorNone;
    _lastErrorMessage = nil;
    dispatch_async(_queue, ^{ [self runBlocking]; });
}

- (void)stop
{
    if (!_running) {
        return;
    }
    _running = NO;

    // Force a blocked recv() to return so the reader can observe `_running`.
    RTMP *r = _rtmp;
    if (r != NULL && r->m_sb.sb_socket >= 0) {
        shutdown(r->m_sb.sb_socket, SHUT_RDWR);
    }

    long waited = dispatch_semaphore_wait(_done,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3 * NSEC_PER_SEC)));
    if (waited != 0) {
        LPLogW(@"RTMP reader did not unwind within 3s");
    }
}

#pragma mark - Reader

- (void)runBlocking
{
    @autoreleasepool {
        [self performSession];
    }
    _running = NO;
    dispatch_semaphore_signal(_done);
}

- (void)performSession
{
    {
        if (_url.length == 0) {
            [self finishWithError:LatencyPlayerErrorRTMPConnectFailed message:@"Empty RTMP URL"];
            return;
        }

        RTMP *r = RTMP_Alloc();
        if (r == NULL) {
            [self finishWithError:LatencyPlayerErrorOutOfMemory message:@"RTMP_Alloc failed"];
            return;
        }
        RTMP_Init(r);
        _rtmp = r;

        if (self.timeoutSeconds > 0) {
            r->Link.timeout = self.timeoutSeconds; // connect + SO_RCVTIMEO (seconds)
        }
        if (self.bufferTimeMs > 0) {
            RTMP_SetBufferMS(r, self.bufferTimeMs);
        }

        char *url = strdup(_url.UTF8String);
        if (url == NULL) {
            [self teardownRTMP:r];
            [self finishWithError:LatencyPlayerErrorOutOfMemory message:@"strdup failed"];
            return;
        }

        if (!RTMP_SetupURL(r, url)) {
            free(url);
            [self teardownRTMP:r];
            [self finishWithError:LatencyPlayerErrorRTMPConnectFailed message:@"Invalid RTMP URL"];
            return;
        }

        r->Link.lFlags |= RTMP_LF_LIVE; // live stream

        if (!RTMP_Connect(r, NULL)) {
            LPLogE(@"RTMP_Connect failed: %@", _url);
            free(url);
            [self teardownRTMP:r];
            [self finishWithError:LatencyPlayerErrorNetworkConnectFailed message:@"RTMP connect failed"];
            return;
        }

        if (!RTMP_ConnectStream(r, 0)) {
            LPLogE(@"RTMP_ConnectStream failed: %@", _url);
            free(url);
            [self teardownRTMP:r];
            [self finishWithError:LatencyPlayerErrorRTMPStreamNotFound message:@"Stream unavailable"];
            return;
        }

        LPLogI(@"RTMP connected: %@", _url);
        id<LPRtmpClientDelegate> delegate = self.delegate;
        [delegate rtmpClientDidConnect:self];

        [self readLoopWithRTMP:r];

        free(url);
        [self teardownRTMP:r];

        if (_lastErrorCode == LatencyPlayerErrorNone) {
            // Clean EOF that we did not initiate.
            if (_running) {
                [delegate rtmpClientDidDisconnect:self];
            }
        }
    }
}

- (void)readLoopWithRTMP:(RTMP *)r
{
    char *buf = malloc(kLPReadBufferSize);
    if (buf == NULL) {
        [self finishWithError:LatencyPlayerErrorOutOfMemory message:@"read buffer alloc failed"];
        return;
    }

    _bytesThisSecond = 0;
    _speedWindowStart = CFAbsoluteTimeGetCurrent();
    id<LPRtmpClientDelegate> delegate = self.delegate;
    BOOL speedSupported = [delegate respondsToSelector:@selector(rtmpClient:didUpdateDownloadSpeed:)];

    while (_running) {
        int n = RTMP_Read(r, buf, kLPReadBufferSize);

        if (n > 0) {
            _bytesThisSecond += n;
            NSData *data = [[NSData alloc] initWithBytes:buf length:(NSUInteger)n];
            [delegate rtmpClient:self didReadFLVData:data];
        } else if (n == 0) {
            if (!_running) {
                break;
            }
            // Idle/EOF: for a live stream this means the server stopped sending.
            LPLogW(@"RTMP read returned EOF");
            [self finishWithError:LatencyPlayerErrorNetworkConnectFailed message:@"Stream ended"];
            break;
        } else {
            LPLogE(@"RTMP read error");
            [self finishWithError:LatencyPlayerErrorNetworkConnectFailed message:@"RTMP read error"];
            break;
        }

        if (speedSupported) {
            CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
            if (now - _speedWindowStart >= 1.0) {
                int64_t speed = (int64_t)(_bytesThisSecond / (now - _speedWindowStart));
                _bytesThisSecond = 0;
                _speedWindowStart = now;
                [delegate rtmpClient:self didUpdateDownloadSpeed:speed];
            }
        }
    }

    free(buf);
}

- (void)teardownRTMP:(RTMP *)r
{
    if (_rtmp == r) {
        _rtmp = NULL;
    }
    if (r != NULL) {
        RTMP_Close(r);
        RTMP_Free(r);
    }
}

- (void)finishWithError:(LatencyPlayerErrorCode)code message:(NSString *)message
{
    _lastErrorCode = code;
    _lastErrorMessage = [message copy];
    [self.delegate rtmpClient:self didFailWithError:code message:message];
}

@end
