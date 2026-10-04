/**
 * LatencyPlayer Core - GStreamer播放器核心头文件
 *
 * 定义播放器上下文结构和核心API接口。
 * 基于GStreamer playbin实现RTMP/RTSP流媒体播放。
 */

#ifndef LATENCY_PLAYER_CORE_H
#define LATENCY_PLAYER_CORE_H

#include <jni.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <android/native_window_jni.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==============================================================================
 * 常量定义
 * ============================================================================== */

/** GStreamer消息类型 */
#define GST_MSG_STATE_CHANGED       0
#define GST_MSG_ERROR               1
#define GST_MSG_BUFFERING           2
#define GST_MSG_CONNECTED           3
#define GST_MSG_DISCONNECTED        4
#define GST_MSG_FIRST_FRAME         5
#define GST_MSG_DOWNLOAD_SPEED      6
#define GST_MSG_VIDEO_SIZE          7
#define GST_MSG_EOS                 8
#define GST_MSG_BITRATE_CHANGED     9
#define GST_MSG_DECODER_CHANGED     10
#define GST_MSG_POSITION_CHANGED    11
#define GST_MSG_DURATION_CHANGED    12

/** 解码器类型 */
#define DECODER_TYPE_HARDWARE   0
#define DECODER_TYPE_SOFTWARE   1
#define DECODER_TYPE_AUTO       2

/** 默认缓冲时间(ms) */
#define DEFAULT_BUFFER_TIME_MS  200

/** 默认重连间隔(s) */
#define DEFAULT_RECONNECT_INTERVAL_S  3

/** 默认RTMP超时(s) */
#define DEFAULT_RTMP_TIMEOUT_S  10

/** 默认RTMP重试次数 */
#define DEFAULT_RTMP_RETRY_COUNT  3

/* ==============================================================================
 * 播放器上下文结构体
 * ============================================================================== */

/**
 * 播放器上下文
 * 包含GStreamer pipeline和所有播放器状态信息
 */
typedef struct LatencyPlayerContext {
    /** Java虚拟机 */
    JavaVM *java_vm;

    /** Java对象引用 */
    jobject java_obj;

    /** GStreamer主循环 */
    GMainLoop *main_loop;

    /** GMainContext */
    GMainContext *context;

    /** 播放器pipeline (playbin) */
    GstElement *pipeline;

    /** 视频解码器 */
    GstElement *video_decoder;

    /** 视频渲染sink */
    GstElement *video_sink;

    /** 视频翻转元素 */
    GstElement *video_flip;

    /** 视频缩放元素 */
    GstElement *video_scale;

    /** 视频颜色空间转换 */
    GstElement *video_convert;

    /** 音频渲染sink */
    GstElement *audio_sink;

    /** Native Window (用于渲染) */
    ANativeWindow *native_window;

    /** 播放URI */
    gchar *uri;

    /** 是否正在播放 */
    gboolean is_playing;

    /** 是否是直播流 */
    gboolean is_live_source;

    /** 缓冲时间(ms) */
    gint buffer_time_ms;

    /** 是否开启低延迟模式 */
    gboolean low_latency;

    /** 是否静音 */
    gboolean is_muted;

    /** 音量 (0.0 - 1.0) */
    gdouble volume;

    /** 视频宽度 */
    gint video_width;

    /** 视频高度 */
    gint video_height;

    /** 当前码率 (bps) */
    gint64 current_bitrate;

    /** 解码器类型 0=硬解 1=软解 2=自动 */
    gint decoder_type;

    /** 是否硬解码可用 */
    gboolean hw_decoder_available;

    /** RTMP超时时间(s) */
    gint rtmp_timeout;

    /** RTMP重试次数 */
    gint rtmp_retry_count;

    /** 翻转状态 */
    gboolean flip_horizontal;
    gboolean flip_vertical;

    /** 旋转角度 */
    gint rotation_degrees;

    /** 是否已初始化 */
    gboolean initialized;

    /** 使用 appsink 直绘（绕过 glimagesink/EGL） */
    gboolean use_appsink;

    /** 是否已发送首帧 */
    gboolean first_frame_sent;

    /** setBuffersGeometry 已应用的尺寸（避免每帧重设） */
    int window_geo_w;
    int window_geo_h;

    /** 互斥锁 */
    GMutex mutex;

    /** 保护 native_window/geo 的专用锁：流线程(draw/prepare-window-handle)只拿这把，
     *  ctx->mutex 再也不被流线程触碰；持有时绝不跨 gst set_state */
    GMutex window_mutex;

    /** 下载速度统计 */
    gint64 total_bytes_downloaded;
    gint64 last_speed_check_time;
    gint64 last_bytes_at_check;
} LatencyPlayerContext;

/* ==============================================================================
 * 核心API函数声明
 * ============================================================================== */

/**
 * 初始化播放器上下文
 */
int latency_player_init(LatencyPlayerContext *ctx, JavaVM *java_vm, jobject java_obj);

/**
 * 设置播放URI并开始播放
 */
int latency_player_play(LatencyPlayerContext *ctx, const char *uri);

/**
 * 停止播放
 */
int latency_player_stop(LatencyPlayerContext *ctx);

/**
 * 暂停播放
 */
int latency_player_pause(LatencyPlayerContext *ctx);

/**
 * 恢复播放
 */
int latency_player_resume(LatencyPlayerContext *ctx);

/**
 * 设置Native Window (渲染目标)
 */
int latency_player_set_surface(LatencyPlayerContext *ctx, ANativeWindow *window);

/**
 * 设置静音
 */
int latency_player_set_mute(LatencyPlayerContext *ctx, gboolean mute);

/**
 * 设置音量 (0.0 - 1.0)
 */
int latency_player_set_volume(LatencyPlayerContext *ctx, gdouble volume);

/**
 * 设置缓冲时间(ms)
 */
int latency_player_set_buffer_time(LatencyPlayerContext *ctx, gint buffer_ms);

/**
 * 设置低延迟模式
 */
int latency_player_set_low_latency(LatencyPlayerContext *ctx, gboolean enable);

/**
 * 设置解码器类型
 *
 * @param ctx          播放器上下文
 * @param decoder_type DECODER_TYPE_HARDWARE/DECODER_TYPE_SOFTWARE/DECODER_TYPE_AUTO
 */
int latency_player_set_decoder_type(LatencyPlayerContext *ctx, gint decoder_type);

/**
 * 设置视频翻转
 */
int latency_player_set_flip(LatencyPlayerContext *ctx, gboolean flip_h, gboolean flip_v);

/**
 * 设置视频旋转角度 (0/90/180/270)
 */
int latency_player_set_rotation(LatencyPlayerContext *ctx, gint degrees);

/**
 * 保存当前帧截图
 */
int latency_player_save_snapshot(LatencyPlayerContext *ctx, const char *path);

/**
 * 获取当前播放位置(ms)
 */
gint64 latency_player_get_position(LatencyPlayerContext *ctx);

/**
 * 获取总时长(ms)
 */
gint64 latency_player_get_duration(LatencyPlayerContext *ctx);

/**
 * 设置RTMP超时时间(s)
 */
int latency_player_set_rtmp_timeout(LatencyPlayerContext *ctx, gint timeout);

/**
 * 设置RTMP重试次数
 */
int latency_player_set_rtmp_retry(LatencyPlayerContext *ctx, gint retry_count);

/**
 * 切换URL (热切换)
 */
int latency_player_switch_uri(LatencyPlayerContext *ctx, const char *uri);

/**
 * 获取当前下载速度(bytes/s)
 */
gint64 latency_player_get_download_speed(LatencyPlayerContext *ctx);

/**
 * 释放播放器资源
 */
void latency_player_release(LatencyPlayerContext *ctx);

/**
 * 向Java层发送消息
 */
void latency_player_send_message(LatencyPlayerContext *ctx, int type, long param1, long param2);

#ifdef __cplusplus
}
#endif

#endif /* LATENCY_PLAYER_CORE_H */
