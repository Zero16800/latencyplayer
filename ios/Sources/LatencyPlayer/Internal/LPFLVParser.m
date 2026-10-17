//
//  LPFLVParser.m
//

#import "LPFLVParser.h"
#import "LPLog.h"

static inline uint32_t LPReadU24(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static const NSUInteger kLPCompactThreshold = 1 << 20; // 1 MiB

@interface LPFLVParser () {
    NSMutableData *_buf;
    NSUInteger     _scan;
    BOOL           _headerSeen;
    BOOL           _warnedVideo;
    BOOL           _warnedAudio;
}
@end

@implementation LPFLVParser

- (instancetype)init
{
    self = [super init];
    if (self) {
        _buf = [NSMutableData data];
    }
    return self;
}

- (void)reset
{
    [_buf setLength:0];
    _scan = 0;
    _headerSeen = NO;
    _warnedVideo = NO;
    _warnedAudio = NO;
}

- (void)appendData:(NSData *)data
{
    if (data.length == 0) {
        return;
    }
    [self appendBytes:data.bytes length:data.length];
}

- (void)appendBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (length == 0) {
        return;
    }
    [_buf appendBytes:bytes length:length];
    [self parse];
}

#pragma mark - Parsing

- (void)parse
{
    NSUInteger total = _buf.length;

    if (!_headerSeen) {
        if (total - _scan < 9) {
            return;
        }
        const uint8_t *p = (const uint8_t *)_buf.bytes + _scan;
        if (p[0] == 'F' && p[1] == 'L' && p[2] == 'V') {
            _scan += 9;                 // "FLV" + version + flags + dataOffset
        }
        _headerSeen = YES;
    }

    const uint8_t *base = (const uint8_t *)_buf.bytes;
    total = _buf.length;

    while (total - _scan >= 11) {
        const uint8_t *p = base + _scan;
        uint8_t tagType = p[0];

        // PreviousTagSize0 (or stray padding) after the FLV header.
        if (tagType == 0) {
            _scan += 4;
            continue;
        }
        // Resynchronise on unknown tag types.
        if (tagType != 8 && tagType != 9 && tagType != 18) {
            _scan += 1;
            continue;
        }

        uint32_t dataSize = LPReadU24(p + 1);
        int64_t timestamp = (int64_t)LPReadU24(p + 4) | ((int64_t)p[7] << 24);
        NSUInteger recordLen = 11 + (NSUInteger)dataSize + 4;

        if (total - _scan < recordLen) {
            break; // wait for more data
        }

        const uint8_t *body = p + 11;
        if (tagType == 9) {
            [self handleVideoTag:body size:dataSize timestamp:timestamp];
        } else if (tagType == 8) {
            [self handleAudioTag:body size:dataSize timestamp:timestamp];
        } // tagType == 18 (script data) is intentionally ignored

        _scan += recordLen;
    }

    [self compact];
}

- (void)compact
{
    if (_scan == 0) {
        return;
    }
    if (_scan >= _buf.length) {
        [_buf setLength:0];
        _scan = 0;
        return;
    }
    if (_scan >= kLPCompactThreshold) {
        [_buf replaceBytesInRange:NSMakeRange(0, _scan) withBytes:NULL length:0];
        _scan = 0;
    }
}

- (void)handleVideoTag:(const uint8_t *)body size:(uint32_t)size timestamp:(int64_t)timestamp
{
    if (size < 1) {
        return;
    }
    uint8_t frameType = body[0] >> 4;
    uint8_t codecId = body[0] & 0x0F;

    if (codecId != 7) { // 7 = AVC / H.264
        if (!_warnedVideo) {
            _warnedVideo = YES;
            if ([self.delegate respondsToSelector:@selector(flvParser:didEncounterUnsupportedVideoCodec:)]) {
                [self.delegate flvParser:self didEncounterUnsupportedVideoCodec:codecId];
            }
            LPLogW(@"Unsupported video codec id: %u", codecId);
        }
        return;
    }
    if (size < 5) {
        return;
    }

    uint8_t avcPacketType = body[1];
    int32_t cts = (int32_t)(((uint32_t)body[2] << 16) | ((uint32_t)body[3] << 8) | (uint32_t)body[4]);
    if (cts & 0x800000) {
        cts |= (int32_t)0xFF000000; // sign-extend 24-bit composition time
    }

    id<LPFLVParserDelegate> d = self.delegate;

    if (avcPacketType == 0) {
        NSData *avcc = [NSData dataWithBytes:body + 5 length:size - 5];
        [d flvParser:self didReceiveVideoSequenceHeader:avcc];
    } else if (avcPacketType == 1) {
        NSData *nalu = [NSData dataWithBytes:body + 5 length:size - 5];
        BOOL isKeyframe = (frameType == 1);
        [d flvParser:self didReceiveVideoFrame:nalu isKeyframe:isKeyframe timestampMs:timestamp + cts];
    }
    // avcPacketType == 2 (end of sequence) is ignored.
}

- (void)handleAudioTag:(const uint8_t *)body size:(uint32_t)size timestamp:(int64_t)timestamp
{
    if (size < 2) {
        return;
    }
    uint8_t soundFormat = body[0] >> 4;

    if (soundFormat != 10) { // 10 = AAC
        if (!_warnedAudio) {
            _warnedAudio = YES;
            if ([self.delegate respondsToSelector:@selector(flvParser:didEncounterUnsupportedAudioCodec:)]) {
                [self.delegate flvParser:self didEncounterUnsupportedAudioCodec:soundFormat];
            }
            LPLogW(@"Unsupported audio sound format: %u", soundFormat);
        }
        return;
    }

    uint8_t aacPacketType = body[1];
    id<LPFLVParserDelegate> d = self.delegate;

    if (aacPacketType == 0) {
        NSData *asc = [NSData dataWithBytes:body + 2 length:size - 2];
        [d flvParser:self didReceiveAudioSequenceHeader:asc];
    } else if (aacPacketType == 1) {
        NSData *aac = [NSData dataWithBytes:body + 2 length:size - 2];
        [d flvParser:self didReceiveAudioFrame:aac timestampMs:timestamp];
    }
}

@end
