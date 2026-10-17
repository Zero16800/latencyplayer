//
//  LatencyPlayerError.m
//

#import "LatencyPlayerError.h"

NSString *LatencyPlayerErrorGetDescription(LatencyPlayerErrorCode code)
{
    switch (code) {
        case LatencyPlayerErrorNone:                  return @"无错误";
        case LatencyPlayerErrorNetworkConnectFailed:  return @"网络连接失败";
        case LatencyPlayerErrorNetworkTimeout:        return @"网络超时";
        case LatencyPlayerErrorRTMPConnectFailed:     return @"RTMP连接失败";
        case LatencyPlayerErrorRTMPHandshakeFailed:   return @"RTMP握手失败";
        case LatencyPlayerErrorRTMPStreamNotFound:    return @"RTMP流不存在";
        case LatencyPlayerErrorRTMPAuthFailed:        return @"RTMP认证失败";
        case LatencyPlayerErrorVideoDecodeFailed:     return @"视频解码失败";
        case LatencyPlayerErrorVideoCodecNotSupported: return @"不支持的视频编码格式";
        case LatencyPlayerErrorAudioDecodeFailed:     return @"音频解码失败";
        case LatencyPlayerErrorAudioCodecNotSupported: return @"不支持的音频编码格式";
        case LatencyPlayerErrorRenderInitFailed:      return @"渲染初始化失败";
        case LatencyPlayerErrorSurfaceInvalid:        return @"Surface无效";
        case LatencyPlayerErrorOutOfMemory:           return @"内存不足";
        case LatencyPlayerErrorGStreamerInitFailed:   return @"GStreamer初始化失败";
        case LatencyPlayerErrorPipelineCreateFailed:  return @"Pipeline创建失败";
        default:
            return [NSString stringWithFormat:@"未知错误(%ld)", (long)code];
    }
}
