//
//  LPH264Decoder.h
//  VideoToolbox-backed H.264 decoder.
//

#import <Foundation/Foundation.h>
#import <CoreMedia/CoreMedia.h>

NS_ASSUME_NONNULL_BEGIN

@class LPH264Decoder;

@protocol LPH264DecoderDelegate <NSObject>

/// A decoded, display-ready sample buffer (owned by the caller for the
/// duration of the call). Intended to be enqueued on an
/// AVSampleBufferDisplayLayer.
- (void)h264Decoder:(LPH264Decoder *)decoder didDecodeSampleBuffer:(CMSampleBufferRef)sampleBuffer;

@optional
- (void)h264Decoder:(LPH264Decoder *)decoder didChangeVideoSize:(CGSize)size;
- (void)h264Decoder:(LPH264Decoder *)decoder didFailWithError:(NSError *)error;

@end

@interface LPH264Decoder : NSObject

@property (nonatomic, weak, nullable) id<LPH264DecoderDelegate> delegate;

/// Prefer the hardware decoder when available (mirrors
/// `LatencyPlayerConfig.hardwareDecoder`). Default YES.
@property (nonatomic, assign) BOOL preferHardware;

/// Parse an AVCDecoderConfigurationRecord (SPS/PPS) and (re)create the
/// decoding session. Returns NO on malformed input.
- (BOOL)configureWithAVCDecoderConfigurationRecord:(NSData *)avcc;

/// Decode a single AVCC-framed access unit. `ptsMs` is the presentation
/// timestamp in milliseconds.
- (void)decodeAccessUnit:(NSData *)nalu
                   ptsMs:(int64_t)ptsMs
              isKeyframe:(BOOL)isKeyframe;

/// Drop any pending frames.
- (void)flush;

/// Tear down the decoding session.
- (void)invalidate;

/// Dimensions of the decoded video, or CGSizeZero before the first frame.
@property (nonatomic, readonly) CGSize videoSize;

/// Whether a decoding session is currently configured.
@property (nonatomic, readonly) BOOL isConfigured;

@end

NS_ASSUME_NONNULL_END
