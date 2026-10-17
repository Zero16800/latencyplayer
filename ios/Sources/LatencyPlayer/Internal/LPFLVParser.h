//
//  LPFLVParser.h
//  Incremental FLV (Flash Video) tag parser.
//
//  librtmp's `RTMP_Read` yields a byte stream shaped like an FLV file
//  (9-byte "FLV" header followed by <tag header><body><prevTagSize> records).
//  This parser reassembles those records across arbitrary chunk boundaries and
//  reports H.264 / AAC payloads to its delegate.
//
//  Video payloads are handed over in "AVCC" form (4-byte big-endian NALU length
//  prefixes), which is exactly what VideoToolbox expects when the format
//  description is created with NAL unit header length 4.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class LPFLVParser;

@protocol LPFLVParserDelegate <NSObject>

/// `avcc` is a raw AVCDecoderConfigurationRecord (AVCPacketType 0).
- (void)flvParser:(LPFLVParser *)parser didReceiveVideoSequenceHeader:(NSData *)avcc;

/// `nalu` is AVCC-framed access unit data (AVCPacketType 1).
- (void)flvParser:(LPFLVParser *)parser
    didReceiveVideoFrame:(NSData *)nalu
              isKeyframe:(BOOL)isKeyframe
             timestampMs:(int64_t)timestampMs;

/// `asc` is a raw AudioSpecificConfig (AACPacketType 0).
- (void)flvParser:(LPFLVParser *)parser didReceiveAudioSequenceHeader:(NSData *)asc;

/// `aac` is raw AAC access-unit data (AACPacketType 1).
- (void)flvParser:(LPFLVParser *)parser
    didReceiveAudioFrame:(NSData *)aac
             timestampMs:(int64_t)timestampMs;

@optional
/// Reported once if a video codec other than H.264 is encountered.
- (void)flvParser:(LPFLVParser *)parser didEncounterUnsupportedVideoCodec:(uint8_t)codecId;
/// Reported once if an audio codec other than AAC is encountered.
- (void)flvParser:(LPFLVParser *)parser didEncounterUnsupportedAudioCodec:(uint8_t)soundFormat;

@end

@interface LPFLVParser : NSObject

@property (nonatomic, weak, nullable) id<LPFLVParserDelegate> delegate;

/// Feed a chunk of FLV stream data as returned by `RTMP_Read`.
- (void)appendBytes:(const uint8_t *)bytes length:(NSUInteger)length;

/// Feed a chunk from an `NSData` object.
- (void)appendData:(NSData *)data;

/// Clear all buffered state (does not affect the delegate).
- (void)reset;

@end

NS_ASSUME_NONNULL_END
