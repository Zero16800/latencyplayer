//
//  LPRtmpClient.h
//  Blocking RTMP transport built on the vendored librtmp.
//
//  Connects to a `rtmp://` URL, sends `play`, and streams the server's
//  FLV-shaped output to its delegate. All methods are safe to call from any
//  thread; the connection runs on a private serial queue.
//

#import <Foundation/Foundation.h>
#import "LatencyPlayerError.h"

NS_ASSUME_NONNULL_BEGIN

@class LPRtmpClient;

@protocol LPRtmpClientDelegate <NSObject>

/// TCP/RTMP session established and `play` accepted.
- (void)rtmpClientDidConnect:(LPRtmpClient *)client;

/// Stream ended (server closed / EOF). Not called when `-stop` was requested.
- (void)rtmpClientDidDisconnect:(LPRtmpClient *)client;

/// A chunk of FLV stream data (as produced by `RTMP_Read`).
- (void)rtmpClient:(LPRtmpClient *)client didReadFLVData:(NSData *)data;

/// The session failed (connect error, read error, unexpected EOF).
- (void)rtmpClient:(LPRtmpClient *)client
    didFailWithError:(LatencyPlayerErrorCode)code
             message:(NSString *)message;

@optional
/// Rolling download rate, roughly once per second.
- (void)rtmpClient:(LPRtmpClient *)client didUpdateDownloadSpeed:(int64_t)bytesPerSec;

@end

@interface LPRtmpClient : NSObject

@property (nonatomic, weak, nullable) id<LPRtmpClientDelegate> delegate;

/// Connection + receive timeout in seconds (default 10).
@property (nonatomic, assign) int timeoutSeconds;

/// librtmp buffer time in ms (default 0 = lowest latency).
@property (nonatomic, assign) int bufferTimeMs;

- (instancetype)initWithURL:(NSString *)url;

/// Blocking RTMP error, if the session ended because of one (else
/// `LatencyPlayerErrorNone`).
@property (nonatomic, readonly) LatencyPlayerErrorCode lastErrorCode;
@property (nonatomic, readonly, nullable) NSString *lastErrorMessage;

/// Connect and start reading on the internal queue. No-op if already running.
- (void)start;

/// Request stop and block until the reader has unwound (bounded wait).
- (void)stop;

@property (nonatomic, readonly) BOOL isRunning;

@end

NS_ASSUME_NONNULL_END
