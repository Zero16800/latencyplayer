package com.latencyplayer.sdk;

/**
 * 播放器错误码定义
 */
public final class LatencyPlayerError {

    private LatencyPlayerError() {
    }

    /** 无错误 */
    public static final int NONE = 0;

    /** 网络连接失败 */
    public static final int NETWORK_CONNECT_FAILED = 1001;

    /** 网络超时 */
    public static final int NETWORK_TIMEOUT = 1002;

    /** RTMP连接失败 */
    public static final int RTMP_CONNECT_FAILED = 1003;

    /** RTMP握手失败 */
    public static final int RTMP_HANDSHAKE_FAILED = 1004;

    /** RTMP流不存在 */
    public static final int RTMP_STREAM_NOT_FOUND = 1005;

    /** RTMP认证失败 */
    public static final int RTMP_AUTH_FAILED = 1006;

    /** 视频解码失败 */
    public static final int VIDEO_DECODE_FAILED = 2001;

    /** 不支持的视频编码格式 */
    public static final int VIDEO_CODEC_NOT_SUPPORTED = 2002;

    /** 音频解码失败 */
    public static final int AUDIO_DECODE_FAILED = 2003;

    /** 不支持的音频编码格式 */
    public static final int AUDIO_CODEC_NOT_SUPPORTED = 2004;

    /** 渲染初始化失败 */
    public static final int RENDER_INIT_FAILED = 3001;

    /** Surface无效 */
    public static final int SURFACE_INVALID = 3002;

    /** 内存不足 */
    public static final int OUT_OF_MEMORY = 4001;

    /** GStreamer初始化失败 */
    public static final int GSTREAMER_INIT_FAILED = 5001;

    /** Pipeline创建失败 */
    public static final int PIPELINE_CREATE_FAILED = 5002;

    /** 未知错误 */
    public static final int UNKNOWN = -1;

    /**
     * 获取错误描述
     */
    public static String getDescription(int errorCode) {
        switch (errorCode) {
            case NONE:
                return "无错误";
            case NETWORK_CONNECT_FAILED:
                return "网络连接失败";
            case NETWORK_TIMEOUT:
                return "网络超时";
            case RTMP_CONNECT_FAILED:
                return "RTMP连接失败";
            case RTMP_HANDSHAKE_FAILED:
                return "RTMP握手失败";
            case RTMP_STREAM_NOT_FOUND:
                return "RTMP流不存在";
            case RTMP_AUTH_FAILED:
                return "RTMP认证失败";
            case VIDEO_DECODE_FAILED:
                return "视频解码失败";
            case VIDEO_CODEC_NOT_SUPPORTED:
                return "不支持的视频编码格式";
            case AUDIO_DECODE_FAILED:
                return "音频解码失败";
            case AUDIO_CODEC_NOT_SUPPORTED:
                return "不支持的音频编码格式";
            case RENDER_INIT_FAILED:
                return "渲染初始化失败";
            case SURFACE_INVALID:
                return "Surface无效";
            case OUT_OF_MEMORY:
                return "内存不足";
            case GSTREAMER_INIT_FAILED:
                return "GStreamer初始化失败";
            case PIPELINE_CREATE_FAILED:
                return "Pipeline创建失败";
            default:
                return "未知错误(" + errorCode + ")";
        }
    }
}
