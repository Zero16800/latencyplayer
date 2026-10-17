//
//  LatencyPlayerError.h
//  LatencyPlayer iOS SDK
//
//  Error codes. Values are identical to the Android SDK
//  (com.latencyplayer.sdk.LatencyPlayerError).
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, LatencyPlayerErrorCode) {
    /** 无错误 */
    LatencyPlayerErrorNone                 = 0,

    /** 网络连接失败 */
    LatencyPlayerErrorNetworkConnectFailed = 1001,
    /** 网络超时 */
    LatencyPlayerErrorNetworkTimeout       = 1002,
    /** RTMP连接失败 */
    LatencyPlayerErrorRTMPConnectFailed    = 1003,
    /** RTMP握手失败 */
    LatencyPlayerErrorRTMPHandshakeFailed  = 1004,
    /** RTMP流不存在 */
    LatencyPlayerErrorRTMPStreamNotFound   = 1005,
    /** RTMP认证失败 */
    LatencyPlayerErrorRTMPAuthFailed       = 1006,

    /** 视频解码失败 */
    LatencyPlayerErrorVideoDecodeFailed     = 2001,
    /** 不支持的视频编码格式 */
    LatencyPlayerErrorVideoCodecNotSupported = 2002,
    /** 音频解码失败 */
    LatencyPlayerErrorAudioDecodeFailed     = 2003,
    /** 不支持的音频编码格式 */
    LatencyPlayerErrorAudioCodecNotSupported = 2004,

    /** 渲染初始化失败 */
    LatencyPlayerErrorRenderInitFailed     = 3001,
    /** Surface无效 */
    LatencyPlayerErrorSurfaceInvalid       = 3002,

    /** 内存不足 */
    LatencyPlayerErrorOutOfMemory          = 4001,

    /** GStreamer初始化失败 */
    LatencyPlayerErrorGStreamerInitFailed  = 5001,
    /** Pipeline创建失败 */
    LatencyPlayerErrorPipelineCreateFailed = 5002,

    /** 未知错误 */
    LatencyPlayerErrorUnknown              = -1,
};

/// Localised (zh-Hans) description matching the Android `getDescription()`.
FOUNDATION_EXPORT NSString *LatencyPlayerErrorGetDescription(LatencyPlayerErrorCode code);

NS_ASSUME_NONNULL_END
