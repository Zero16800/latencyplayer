//
//  LPH264Decoder.m
//

#import "LPH264Decoder.h"
#import "LPLog.h"

#import <VideoToolbox/VideoToolbox.h>
#import <CoreVideo/CoreVideo.h>

static void LPH264DecodeCallback(void *decompressionOutputRefCon,
                                 void *sourceFrameRefCon,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime presentationTimeStamp,
                                 CMTime presentationDuration);

@implementation LPH264Decoder {
    VTDecompressionSessionRef   _session;
    CMVideoFormatDescriptionRef _formatDesc;
    uint8_t                     _nalLengthSize;
    BOOL                        _configured;
    CGSize                      _videoSize;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _preferHardware = YES;
        _nalLengthSize = 4;
        _videoSize = CGSizeZero;
    }
    return self;
}

- (void)dealloc
{
    [self invalidate];
}

- (BOOL)isConfigured
{
    return _configured;
}

- (CGSize)videoSize
{
    return _videoSize;
}

#pragma mark - Configuration

- (BOOL)configureWithAVCDecoderConfigurationRecord:(NSData *)avcc
{
    if (avcc.length < 7) {
        LPLogE(@"AVCDecoderConfigurationRecord too short (%lu)", (unsigned long)avcc.length);
        return NO;
    }

    const uint8_t *p = avcc.bytes;
    NSUInteger n = avcc.length;

    uint8_t nalLengthSize = (p[4] & 0x03) + 1;
    uint8_t numSPS = p[5] & 0x1F;
    NSUInteger off = 6;

    NSMutableArray<NSData *> *params = [NSMutableArray array];

    for (uint8_t i = 0; i < numSPS; i++) {
        if (off + 2 > n) return NO;
        uint16_t len = (uint16_t)((p[off] << 8) | p[off + 1]);
        off += 2;
        if (off + len > n) return NO;
        [params addObject:[NSData dataWithBytes:p + off length:len]];
        off += len;
    }

    if (off >= n) return NO;
    uint8_t numPPS = p[off];
    off += 1;

    for (uint8_t i = 0; i < numPPS; i++) {
        if (off + 2 > n) return NO;
        uint16_t len = (uint16_t)((p[off] << 8) | p[off + 1]);
        off += 2;
        if (off + len > n) return NO;
        [params addObject:[NSData dataWithBytes:p + off length:len]];
        off += len;
    }

    if (params.count == 0) {
        LPLogE(@"No SPS/PPS in AVCDecoderConfigurationRecord");
        return NO;
    }

    NSUInteger count = params.count;
    const uint8_t **ptrs = malloc(sizeof(uint8_t *) * count);
    size_t *lens = malloc(sizeof(size_t) * count);
    for (NSUInteger i = 0; i < count; i++) {
        ptrs[i] = (const uint8_t *)params[i].bytes;
        lens[i] = (size_t)params[i].length;
    }

    CMVideoFormatDescriptionRef fmt = NULL;
    OSStatus st = CMVideoFormatDescriptionCreateFromH264ParameterSets(
        kCFAllocatorDefault, (size_t)count, ptrs, lens, nalLengthSize, &fmt);
    free(ptrs);
    free(lens);

    if (st != noErr || fmt == NULL) {
        LPLogE(@"CreateFromH264ParameterSets failed: %d", (int)st);
        return NO;
    }

    // Replace any previous session.
    [self invalidate];

    _formatDesc = fmt;
    _nalLengthSize = nalLengthSize;

    NSDictionary *destAttrs = @{
        (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
        (id)kCVPixelBufferIOSurfacePropertiesKey: @{},
    };

    NSDictionary *spec = nil;
    if (self.preferHardware) {
        spec = @{ (id)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES };
    }

    VTDecompressionOutputCallbackRecord cb;
    cb.decompressionOutputCallback = LPH264DecodeCallback;
    cb.decompressionOutputRefCon = (__bridge void *)self;

    st = VTDecompressionSessionCreate(kCFAllocatorDefault,
                                      _formatDesc,
                                      (__bridge CFDictionaryRef)spec,
                                      (__bridge CFDictionaryRef)destAttrs,
                                      &cb,
                                      &_session);
    if (st != noErr || _session == NULL) {
        LPLogE(@"VTDecompressionSessionCreate failed: %d", (int)st);
        CFRelease(_formatDesc);
        _formatDesc = NULL;
        return NO;
    }

    _configured = YES;

    CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(_formatDesc);
    CGSize newSize = CGSizeMake(dims.width, dims.height);
    if (!CGSizeEqualToSize(newSize, _videoSize)) {
        _videoSize = newSize;
        if ([self.delegate respondsToSelector:@selector(h264Decoder:didChangeVideoSize:)]) {
            [self.delegate h264Decoder:self didChangeVideoSize:newSize];
        }
    }

    LPLogI(@"H.264 decoder configured: %dx%d, nalLength=%u, hw=%d",
           dims.width, dims.height, nalLengthSize, self.preferHardware);
    return YES;
}

#pragma mark - Decoding

- (void)decodeAccessUnit:(NSData *)nalu ptsMs:(int64_t)ptsMs isKeyframe:(BOOL)isKeyframe
{
    if (!_configured || _session == NULL || nalu.length == 0) {
        return;
    }

    CMBlockBufferRef blockBuffer = NULL;
    OSStatus st = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,
                                                     NULL,
                                                     nalu.length,
                                                     kCFAllocatorDefault,
                                                     NULL,
                                                     0,
                                                     nalu.length,
                                                     0,
                                                     &blockBuffer);
    if (st != kCMBlockBufferNoErr) {
        LPLogE(@"CMBlockBufferCreateWithMemoryBlock failed: %d", (int)st);
        return;
    }

    st = CMBlockBufferReplaceDataBytes(nalu.bytes, blockBuffer, 0, nalu.length);
    if (st != kCMBlockBufferNoErr) {
        LPLogE(@"CMBlockBufferReplaceDataBytes failed: %d", (int)st);
        CFRelease(blockBuffer);
        return;
    }

    CMSampleTimingInfo timing;
    timing.presentationTimeStamp = CMTimeMake(ptsMs, 1000);
    timing.duration = kCMTimeInvalid;
    timing.decodeTimeStamp = kCMTimeInvalid;

    CMSampleBufferRef sampleBuffer = NULL;
    st = CMSampleBufferCreateReady(kCFAllocatorDefault,
                                   blockBuffer,
                                   _formatDesc,
                                   1,
                                   1,
                                   &timing,
                                   0,
                                   NULL,
                                   &sampleBuffer);
    CFRelease(blockBuffer);

    if (st != noErr || sampleBuffer == NULL) {
        LPLogE(@"CMSampleBufferCreateReady failed: %d", (int)st);
        return;
    }

    VTDecodeFrameFlags flags = 0;
    VTDecodeInfoFlags outFlags = 0;
    st = VTDecompressionSessionDecodeFrame(_session, sampleBuffer, flags, NULL, &outFlags);
    if (st != noErr) {
        LPLogE(@"VTDecompressionSessionDecodeFrame failed: %d (key=%d)", (int)st, isKeyframe);
    }

    CFRelease(sampleBuffer);
}

- (void)flush
{
    if (_session) {
        VTDecompressionSessionWaitForAsynchronousFrames(_session);
    }
}

- (void)invalidate
{
    if (_session) {
        VTDecompressionSessionInvalidate(_session);
        CFRelease(_session);
        _session = NULL;
    }
    if (_formatDesc) {
        CFRelease(_formatDesc);
        _formatDesc = NULL;
    }
    _configured = NO;
}

#pragma mark - Output handling

static void LPH264DecodeCallback(void *decompressionOutputRefCon,
                                 void *sourceFrameRefCon,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime presentationTimeStamp,
                                 CMTime presentationDuration)
{
    if (status != noErr || imageBuffer == NULL) {
        LPH264Decoder *decoder = (__bridge LPH264Decoder *)decompressionOutputRefCon;
        NSError *error = [NSError errorWithDomain:@"LatencyPlayer.H264Decoder"
                                             code:status
                                         userInfo:@{NSLocalizedDescriptionKey:
                                                        [NSString stringWithFormat:@"Video decode failed (%d)", (int)status]}];
        if ([decoder.delegate respondsToSelector:@selector(h264Decoder:didFailWithError:)]) {
            [decoder.delegate h264Decoder:decoder didFailWithError:error];
        }
        return;
    }

    LPH264Decoder *decoder = (__bridge LPH264Decoder *)decompressionOutputRefCon;
    [decoder handleDecodedImageBuffer:imageBuffer
                     presentationTime:presentationTimeStamp
                             duration:presentationDuration];
}

- (void)handleDecodedImageBuffer:(CVImageBufferRef)imageBuffer
                presentationTime:(CMTime)pts
                        duration:(CMTime)duration
{
    CMVideoFormatDescriptionRef imgFmt = NULL;
    if (CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, imageBuffer, &imgFmt) != noErr) {
        return;
    }

    CMSampleTimingInfo timing;
    timing.presentationTimeStamp = CMTIME_IS_VALID(pts) ? pts : kCMTimeZero;
    timing.duration = CMTIME_IS_VALID(duration) ? duration : kCMTimeInvalid;
    timing.decodeTimeStamp = kCMTimeInvalid;

    CMSampleBufferRef sampleBuffer = NULL;
    OSStatus st = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault,
                                                           imageBuffer,
                                                           imgFmt,
                                                           &timing,
                                                           &sampleBuffer);
    CFRelease(imgFmt);

    if (st != noErr || sampleBuffer == NULL) {
        return;
    }

    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    if (attachments != NULL && CFArrayGetCount(attachments) > 0) {
        CFMutableDictionaryRef dict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }

    [self.delegate h264Decoder:self didDecodeSampleBuffer:sampleBuffer];
    CFRelease(sampleBuffer);
}

@end
