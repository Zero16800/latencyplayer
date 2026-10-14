/**
 * LatencyPlayer Core - GStreamer播放器核心实现
 *
 * 基于GStreamer playbin实现RTMP/RTSP流媒体播放。
 * 支持硬件解码、软硬解码切换、低延迟模式、截图、翻转、旋转等功能。
 */

#include "latency_player_core.h"

#include <string.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/app/app.h>
#include <gst/video/video.h>
#include <glib/gstdio.h>

#define LOG_TAG "LatencyPlayerCore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static GMainLoop *g_player_main_loop = NULL;
static GThread *g_player_main_thread = NULL;

static gpointer player_main_thread_fn(gpointer data) {
    (void)data;
    if (g_player_main_loop == NULL) {
        return NULL;
    }
    LOGI("Player GMainLoop thread started");
    g_main_loop_run(g_player_main_loop);
    LOGI("Player GMainLoop thread exited");
    return NULL;
}

static void ensure_main_loop(void) {
    static GMutex loop_lock;
    static gboolean started = FALSE;

    g_mutex_lock(&loop_lock);
    if (!started) {
        /* 使用全局默认 context，保证 gst_bus_add_signal_watch 能收到消息 */
        g_player_main_loop = g_main_loop_new(NULL, FALSE);
        g_player_main_thread = g_thread_new("latencyplayer-loop",
                                            player_main_thread_fn, NULL);
        started = TRUE;
        LOGI("Player GMainLoop created (default context)");
    }
    g_mutex_unlock(&loop_lock);
}

/* ==============================================================================
 * GStreamer回调函数声明
 * ============================================================================== */

static void on_error(GstBus *bus, GstMessage *msg, gpointer user_data);
static void on_eos(GstBus *bus, GstMessage *msg, gpointer user_data);
static void on_state_changed(GstBus *bus, GstMessage *msg, gpointer user_data);
static void on_buffering(GstBus *bus, GstMessage *msg, gpointer user_data);
static void on_duration_changed(GstBus *bus, GstMessage *msg, gpointer user_data);
static void on_async_done(GstBus *bus, GstMessage *msg, gpointer user_data);
static void on_video_size_changed(GstElement *element, gint width, gint height, gpointer user_data);
static void on_first_video_frame(GstElement *element, GstPad *pad, gpointer user_data);
static void on_source_setup(GstElement *playbin, GstElement *source, gpointer user_data);
static void on_video_decoder_changed(GstElement *element, GParamSpec *pspec, gpointer user_data);
static GstBusSyncReply on_bus_sync(GstBus *bus, GstMessage *message, gpointer user_data);
static GstFlowReturn on_appsink_new_sample(GstElement *sink, gpointer user_data);
static void tune_element_latency(GstElement *element, gpointer user_data);
static void tune_pipeline_queues(GstBin *bin);
static void on_deep_element_added(GstBin *bin, GstBin *sub_bin, GstElement *element,
                                  gpointer user_data);
/* Walk upstream from a queue's sink pad. Returns TRUE while the data is still
 * raw container bytes (source first) and FALSE once a demuxer/parser/decoder
 * has framed it. Raw byte streams must never be leaked: dropping bytes
 * corrupts the container stream for the demuxer (HLS fMP4 / FLV / TS). */
static gboolean queue_carries_raw_bytes(GstElement *queue) {
    GstPad *cur = gst_element_get_static_pad(queue, "sink");
    gboolean framed = FALSE;
    gint depth;

    for (depth = 0; cur != NULL && depth < 32 && !framed; depth++) {
        GstPad *peer = gst_pad_get_peer(cur);
        GstElement *owner;
        GstPad *next = NULL;

        gst_object_unref(cur);
        cur = NULL;
        if (peer == NULL) break;

        owner = gst_pad_get_parent_element(peer);
        if (owner == NULL) {
            gst_object_unref(peer);
            break;
        }

        if (GST_IS_GHOST_PAD(peer)) {
            GstPad *target = gst_ghost_pad_get_target(GST_GHOST_PAD(peer));
            if (target != NULL) next = gst_object_ref(target);
        } else if (GST_IS_BIN(owner)) {
            gst_object_unref(owner);
            gst_object_unref(peer);
            break;
        } else {
            GstElementFactory *factory = gst_element_get_factory(owner);
            const gchar *klass =
                factory != NULL ? gst_element_factory_get_klass(factory) : NULL;

            if (klass != NULL && (strstr(klass, "Demuxer") != NULL ||
                                  strstr(klass, "Parser") != NULL ||
                                  strstr(klass, "Decoder") != NULL ||
                                  strstr(klass, "Depayloader") != NULL)) {
                framed = TRUE;
            } else if (GST_OBJECT_FLAG_IS_SET(owner, GST_ELEMENT_FLAG_SOURCE) ||
                       (klass != NULL && strstr(klass, "Source") != NULL)) {
                gst_object_unref(owner);
                gst_object_unref(peer);
                return TRUE;
            } else {
                next = gst_element_get_static_pad(owner, "sink");
                if (next == NULL) {
                    GstIterator *it = gst_element_iterate_sink_pads(owner);
                    GValue v = G_VALUE_INIT;
                    gboolean done = FALSE;
                    while (!done) {
                        switch (gst_iterator_next(it, &v)) {
                        case GST_ITERATOR_OK:
                            next = GST_PAD(g_value_dup_object(&v));
                            g_value_reset(&v);
                            done = TRUE;
                            break;
                        case GST_ITERATOR_RESYNC:
                            gst_iterator_resync(it);
                            break;
                        default:
                            done = TRUE;
                            break;
                        }
                    }
                    g_value_unset(&v);
                    gst_iterator_free(it);
                }
            }
        }
        gst_object_unref(owner);
        gst_object_unref(peer);
        cur = next;
    }
    if (cur != NULL) gst_object_unref(cur);

    /* Undecided (unlinked yet / walk ended): conservative = raw, never drop. */
    return !framed;
}

/* Flatten queue/queue2/multiqueue so live RTMP does not sit on multi-second buffers.
 */
static void tune_element_latency(GstElement *element, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    if (ctx == NULL || !ctx->low_latency || element == NULL) return;

    GstElementFactory *factory = gst_element_get_factory(element);
    if (factory == NULL) return;
    const gchar *fname = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    if (fname == NULL) return;

    if (g_strcmp0(fname, "queue") == 0) {
        g_object_set(element,
                     "max-size-buffers", 3,
                     "max-size-bytes", 0u,
                     "max-size-time", (guint64)(50 * GST_MSECOND),
                     "leaky", queue_carries_raw_bytes(element) ? 0 : 2,
                     NULL);
        LOGI("low-latency tune queue %s", GST_OBJECT_NAME(element));
    } else if (g_strcmp0(fname, "queue2") == 0) {
        g_object_set(element,
                     "max-size-buffers", 3,
                     "max-size-bytes", 0u,
                     "max-size-time", (guint64)(50 * GST_MSECOND),
                     "use-buffering", FALSE,
                     NULL);
        LOGI("low-latency tune queue2 %s", GST_OBJECT_NAME(element));
    } else if (g_strcmp0(fname, "multiqueue") == 0) {
        /* Only shrink the buffer count. max-size-time/max-size-bytes stay at
         * decodebin's defaults: the overrun-grace patch in gstdecodebin2.c
         * raises them (1000/4MB/5s) to unblock flvdemux's push() and any
         * later shrink here would re-collateralize the demuxer thread. The
         * buffers setter refuses to drop below the current fill level, so
         * the raise survives this tune. buffers=8 still bounds preroll. */
        g_object_set(element,
                     "max-size-buffers", 8,
                     NULL);
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), "interleave-max-bytes")) {
            g_object_set(element, "interleave-max-bytes", 0u, NULL);
        }
        LOGI("low-latency tune multiqueue %s", GST_OBJECT_NAME(element));
    }
}

static void tune_pipeline_queues(GstBin *bin) {
    LatencyPlayerContext *ctx = NULL;
    /* user_data is bound via deep-element-added; this walk uses element names only.
       Callers pass ctx through deep signal; for explicit walk we re-get from parent. */
    (void)ctx;
    if (bin == NULL) return;

    GstIterator *it = gst_bin_iterate_elements(bin);
    GValue item = G_VALUE_INIT;
    gboolean done = FALSE;
    while (!done) {
        switch (gst_iterator_next(it, &item)) {
        case GST_ITERATOR_OK: {
            GstElement *el = GST_ELEMENT(g_value_get_object(&item));
            GstElementFactory *factory = gst_element_get_factory(el);
            if (factory != NULL) {
                const gchar *fname = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
                if (fname != NULL &&
                    (g_strcmp0(fname, "queue") == 0 ||
                     g_strcmp0(fname, "queue2") == 0 ||
                     g_strcmp0(fname, "multiqueue") == 0)) {
                    /* Tune without ctx — force shallow always when this is called
                       only from low-latency paths (deep-element / async-done). */
                    if (g_strcmp0(fname, "queue") == 0) {
                        g_object_set(el, "max-size-buffers", 3, "max-size-bytes", 0u,
                                     "max-size-time", (guint64)(50 * GST_MSECOND),
                                     "leaky",
                                     queue_carries_raw_bytes(el) ? 0 : 2, NULL);
                    } else if (g_strcmp0(fname, "queue2") == 0) {
                        g_object_set(el, "max-size-buffers", 3, "max-size-bytes", 0u,
                                     "max-size-time", (guint64)(50 * GST_MSECOND),
                                     "use-buffering", FALSE, NULL);
                    } else if (g_strcmp0(fname, "multiqueue") == 0) {
                        /* buffers only — see tune_element_latency(): shrinking
                         * time/bytes here would undo the decodebin overrun
                         * raise and re-block flvdemux. */
                        g_object_set(el, "max-size-buffers", 8, NULL);
                    }
                    LOGI("walk-tune %s %s", fname, GST_OBJECT_NAME(el));
                }
            }
            if (GST_IS_BIN(el)) {
                tune_pipeline_queues(GST_BIN(el));
            }
            g_value_reset(&item);
            break;
        }
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(it);
            break;
        case GST_ITERATOR_ERROR:
        case GST_ITERATOR_DONE:
        default:
            done = TRUE;
            break;
        }
    }
    g_value_unset(&item);
    gst_iterator_free(it);
}

static void on_deep_element_added(GstBin *bin, GstBin *sub_bin, GstElement *element,
                                  gpointer user_data) {
    (void)bin;
    (void)sub_bin;
    tune_element_latency(element, user_data);
    if (GST_IS_BIN(element)) {
        LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
        if (ctx != NULL && ctx->low_latency) {
            GstIterator *it = gst_bin_iterate_recurse(GST_BIN(element));
            GValue item = G_VALUE_INIT;
            gboolean done = FALSE;
            while (!done) {
                switch (gst_iterator_next(it, &item)) {
                case GST_ITERATOR_OK: {
                    GstElement *el = GST_ELEMENT(g_value_get_object(&item));
                    tune_element_latency(el, ctx);
                    g_value_reset(&item);
                    break;
                }
                case GST_ITERATOR_RESYNC:
                    gst_iterator_resync(it);
                    break;
                default:
                    done = TRUE;
                    break;
                }
            }
            g_value_unset(&item);
            gst_iterator_free(it);
        }
    }
}

/* Draw one RGBx/RGBA frame into ANativeWindow with nearest-neighbor scale.
 * Returns TRUE if a buffer was locked and posted. */
static gboolean draw_frame_to_window(LatencyPlayerContext *ctx,
                                     const uint8_t *src, int src_w, int src_h, int src_stride) {
    if (ctx == NULL || src == NULL || src_w <= 0 || src_h <= 0) return FALSE;

    ANativeWindow *win = NULL;
    g_mutex_lock(&ctx->window_mutex);
    win = ctx->native_window;
    if (win != NULL) ANativeWindow_acquire(win);
    g_mutex_unlock(&ctx->window_mutex);
    if (win == NULL) {
        LOGW("draw: no native_window");
        return FALSE;
    }

    /* Prefer RGBX (matches LE memory layout of GStreamer RGBx).
       Call only when size/format changes — every-frame geometry resets BufferQueue. */
    if (ctx->window_geo_w != src_w || ctx->window_geo_h != src_h) {
        int geo = ANativeWindow_setBuffersGeometry(win, src_w, src_h, WINDOW_FORMAT_RGBX_8888);
        if (geo != 0) {
            LOGW("setBuffersGeometry(%d,%d,RGBX) ret=%d, fallback RGBA 0,0", src_w, src_h, geo);
            ANativeWindow_setBuffersGeometry(win, 0, 0, WINDOW_FORMAT_RGBA_8888);
            ctx->window_geo_w = 0;
            ctx->window_geo_h = 0;
        } else {
            ctx->window_geo_w = src_w;
            ctx->window_geo_h = src_h;
            LOGI("setBuffersGeometry once %dx%d RGBX", src_w, src_h);
        }
    }

    ANativeWindow_Buffer out;
    int lock_ret = ANativeWindow_lock(win, &out, NULL);
    if (lock_ret != 0 || out.bits == NULL) {
        LOGW("ANativeWindow_lock failed ret=%d bits=%p fmt=%d %dx%d stride=%d",
             lock_ret, out.bits, out.format, out.width, out.height, out.stride);
        ANativeWindow_release(win);
        return FALSE;
    }

    int dst_w = out.width;
    int dst_h = out.height;
    int dst_stride = out.stride;
    uint32_t *dst = (uint32_t *)out.bits;

    for (int y = 0; y < dst_h; ++y) {
        uint32_t *row = dst + (size_t)y * dst_stride;
        for (int x = 0; x < dst_w; ++x) row[x] = 0xFF000000u;
    }

    int draw_w = dst_w, draw_h = dst_h, off_x = 0, off_y = 0;
    if (src_w * dst_h > dst_w * src_h) {
        draw_h = (int)((int64_t)dst_w * src_h / src_w);
        off_y = (dst_h - draw_h) / 2;
    } else {
        draw_w = (int)((int64_t)dst_h * src_w / src_h);
        off_x = (dst_w - draw_w) / 2;
    }
    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    uint32_t sample_sum = 0;
    for (int y = 0; y < draw_h; ++y) {
        int sy = (int)((int64_t)y * src_h / draw_h);
        if (sy >= src_h) sy = src_h - 1;
        const uint32_t *src_row = (const uint32_t *)(src + (size_t)sy * src_stride);
        uint32_t *dst_row = dst + (size_t)(y + off_y) * dst_stride + off_x;
        for (int x = 0; x < draw_w; ++x) {
            int sx = (int)((int64_t)x * src_w / draw_w);
            if (sx >= src_w) sx = src_w - 1;
            uint32_t p = src_row[sx];
            dst_row[x] = p | 0xFF000000u;
            if ((x & 63) == 0 && (y & 63) == 0) sample_sum += p;
        }
    }
    if (sample_sum == 0) {
        LOGW("draw: sampled src pixels all zero, draw=%dx%d", draw_w, draw_h);
    }

    int post_ret = ANativeWindow_unlockAndPost(win);
    if (post_ret != 0) LOGW("ANativeWindow_unlockAndPost ret=%d", post_ret);
    ANativeWindow_release(win);
    return post_ret == 0;
}

static GstFlowReturn on_appsink_new_sample(GstElement *sink, gpointer user_data) {
    static gboolean entered_logged = FALSE;
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    if (!entered_logged) {
        entered_logged = TRUE;
        LOGI("appsink new-sample: ENTERED (callback invoked)");
    }
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (sample == NULL) {
        LOGI("appsink new-sample: pull NULL");
        return GST_FLOW_ERROR;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    if (buffer == NULL || caps == NULL) {
        LOGI("appsink new-sample: buffer=%p caps=%p", buffer, caps);
    } else {
        GstVideoInfo vinfo;
        if (!gst_video_info_from_caps(&vinfo, caps)) {
            gchar *cs = gst_caps_to_string(caps);
            LOGI("appsink new-sample: video_info_from_caps FAILED %s", cs);
            g_free(cs);
        } else {
            GstMapInfo map;
            if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                int w = GST_VIDEO_INFO_WIDTH(&vinfo);
                int h = GST_VIDEO_INFO_HEIGHT(&vinfo);
                int stride = (int)GST_VIDEO_INFO_PLANE_STRIDE(&vinfo, 0);
                if (stride < w * 4) stride = w * 4;

                gboolean posted = draw_frame_to_window(ctx, map.data, w, h, stride);
                gst_buffer_unmap(buffer, &map);

                if (ctx->video_width != w || ctx->video_height != h) {
                    ctx->video_width = w;
                    ctx->video_height = h;
                    latency_player_send_message(ctx, GST_MSG_VIDEO_SIZE, (long)w, (long)h);
                }
                if (!ctx->first_frame_sent) {
                    ctx->first_frame_sent = TRUE;
                    LOGI("First frame post=%d (%dx%d)", posted, w, h);
                    latency_player_send_message(ctx, GST_MSG_FIRST_FRAME, 0, 0);
                }
            } else {
                LOGI("appsink new-sample: gst_buffer_map READ failed");
            }
        }
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

/* ==============================================================================
 * 内部辅助函数
 * ============================================================================== */

/**
 * 构建带RTMP参数的URI
 * 支持 live=1, timeout, reconnect 等参数
 */
static gchar* build_uri_with_params(const char *uri, gint timeout, gint retry) {
    (void)timeout;
    (void)retry;
    return g_strdup(uri);
}

/**
 * 尝试创建硬件解码器
 * 返回成功创建的解码器元素，失败返回NULL
 */
static GstElement* try_create_hw_decoder(LatencyPlayerContext *ctx) {
    GstElement *decoder = NULL;

    // 尝试MediaCodec H.264硬解
    decoder = gst_element_factory_make("vtdec", "hw_decoder_h264");
    if (decoder != NULL) {
        LOGI("Hardware H.264 decoder (vtdec) created");
        ctx->hw_decoder_available = TRUE;
        return decoder;
    }

    // 尝试其他硬解实现
    decoder = gst_element_factory_make("amcviddec-omxgoogleh264decoder", "hw_decoder_h264");
    if (decoder != NULL) {
        LOGI("Hardware H.264 decoder (MediaCodec) created");
        ctx->hw_decoder_available = TRUE;
        return decoder;
    }

    LOGI("No hardware decoder available, falling back to software");
    ctx->hw_decoder_available = FALSE;
    return NULL;
}

/**
 * 根据解码器类型创建解码器
 */
static GstElement* create_decoder(LatencyPlayerContext *ctx) {
    GstElement *decoder = NULL;

    switch (ctx->decoder_type) {
        case DECODER_TYPE_HARDWARE:
            decoder = try_create_hw_decoder(ctx);
            if (decoder == NULL) {
                LOGW("Hardware decoder requested but not available");
            }
            break;

        case DECODER_TYPE_SOFTWARE:
            decoder = gst_element_factory_make("avdec_h264", "sw_decoder");
            LOGI("Software decoder (avdec_h264) created");
            break;

        case DECODER_TYPE_AUTO:
        default:
            // 先尝试硬解
            decoder = try_create_hw_decoder(ctx);
            if (decoder == NULL) {
                // 回退到软解
                decoder = gst_element_factory_make("avdec_h264", "sw_decoder");
                LOGI("Falling back to software decoder");
            }
            break;
    }

    return decoder;
}

/**
 * 检查是否支持H.265解码
 */
static gboolean has_h265_support(void) {
    GstElementFactory *factory = gst_element_factory_find("avdec_h265");
    if (factory != NULL) {
        gst_object_unref(factory);
        return TRUE;
    }
    return FALSE;
}

/**
 * 创建视频处理pipeline片段
 * source -> decode -> convert -> scale -> flip -> sink
 */
static gboolean setup_video_pipeline(LatencyPlayerContext *ctx) {
    if (ctx->pipeline == NULL) return FALSE;

    // 获取playbin的video-sink
    // playbin会自动处理解码，我们通过flags控制是否启用视频
    guint flags;
    g_object_get(ctx->pipeline, "flags", &flags, NULL);

    // 启用视频和音频
    flags |= 0x01; // video
    flags |= 0x02; // audio

    // 禁用文本字幕
    flags &= ~0x04;

    g_object_set(ctx->pipeline, "flags", flags, NULL);

    // 获取video sink并设置
    g_object_get(ctx->pipeline, "video-sink", &ctx->video_sink, NULL);

    if (ctx->video_sink != NULL) {
        // 设置window handle
        if (ctx->native_window != NULL) {
            gst_video_overlay_set_window_handle(
                GST_VIDEO_OVERLAY(ctx->video_sink),
                (guintptr)ctx->native_window
            );
        }

        // 连接尺寸变化信号
        g_signal_connect(ctx->video_sink, "size-changed",
                        G_CALLBACK(on_video_size_changed), ctx);

        LOGI("Video pipeline setup complete");
        return TRUE;
    }

    LOGW("Failed to get video sink");
    return FALSE;
}

/* ==============================================================================
 * 核心API实现
 * ============================================================================== */

int latency_player_init(LatencyPlayerContext *ctx, JavaVM *java_vm, jobject java_obj) {
    if (ctx == NULL) return -1;

    LOGI("Initializing LatencyPlayer context...");

    memset(ctx, 0, sizeof(LatencyPlayerContext));
    ctx->java_vm = java_vm;
    ctx->java_obj = java_obj;
    ctx->buffer_time_ms = DEFAULT_BUFFER_TIME_MS;
    ctx->volume = 1.0;
    ctx->decoder_type = DECODER_TYPE_AUTO;
    ctx->rtmp_timeout = DEFAULT_RTMP_TIMEOUT_S;
    ctx->rtmp_retry_count = DEFAULT_RTMP_RETRY_COUNT;
    ctx->initialized = TRUE;

    g_mutex_init(&ctx->mutex);
    g_mutex_init(&ctx->window_mutex);

    LOGI("LatencyPlayer context initialized");
    return 0;
}

static gboolean is_live_uri(const char *uri) {
    if (uri == NULL) return FALSE;
    if (g_str_has_prefix(uri, "rtmp://") || g_str_has_prefix(uri, "rtmps://") ||
        g_str_has_prefix(uri, "rtsp://") || g_str_has_prefix(uri, "rtsps://")) {
        return TRUE;
    }
    const char *q = strpbrk(uri, "?#");
    gsize len = q != NULL ? (gsize)(q - uri) : strlen(uri);
    return (len >= 4 && g_ascii_strcasecmp(uri + len - 4, ".flv") == 0) ||
           (len >= 5 && g_ascii_strcasecmp(uri + len - 5, ".m3u8") == 0);
}

int latency_player_play(LatencyPlayerContext *ctx, const char *uri) {
    if (ctx == NULL || uri == NULL) return -1;

    LOGI("Playing URI: %s", uri);

    ensure_main_loop();

    g_mutex_lock(&ctx->mutex);

    // 停止当前播放
    if (ctx->pipeline != NULL) {
        gst_element_set_state(ctx->pipeline, GST_STATE_NULL);
        gst_object_unref(ctx->pipeline);
        ctx->pipeline = NULL;
    }

    // 释放旧URI
    if (ctx->uri != NULL) {
        g_free(ctx->uri);
    }

    // 构建带参数的URI
    gchar *full_uri = build_uri_with_params(uri, ctx->rtmp_timeout, ctx->rtmp_retry_count);
    ctx->uri = g_strdup(uri);

    /* RTMP/RTSP/HTTP-FLV/HLS are live: mark before PLAYING so buffering path
       does not pause and intermediate queue2 percentages are not reported as
       "stuck buffering". */
    ctx->is_live_source = is_live_uri(uri);

    GError *error = NULL;

    // 创建playbin pipeline
    ctx->pipeline = gst_parse_launch("playbin", &error);
    if (error != NULL) {
        LOGE("Failed to create pipeline: %s", error->message);
        g_error_free(error);
        g_free(full_uri);
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }

    // 设置URI（干净URI，不含空格参数）
    g_object_set(ctx->pipeline, "uri", full_uri, NULL);
    g_free(full_uri);

    // 启用last-sample以支持截图功能（appsink 属性，playbin 上可能不存在）
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(ctx->pipeline),
                                      "enable-last-sample")) {
        g_object_set(ctx->pipeline, "enable-last-sample", TRUE, NULL);
    }

    // 配置缓冲（0 也要写入：最低延迟）
    // buffer-time 不是 playbin 属性，只在存在时设置
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(ctx->pipeline),
                                      "buffer-time")) {
        g_object_set(ctx->pipeline, "buffer-time",
                    (gint64)(ctx->buffer_time_ms * GST_MSECOND), NULL);
    }

    // 配置音量
    g_object_set(ctx->pipeline, "volume", ctx->volume, NULL);
    g_object_set(ctx->pipeline, "mute", ctx->is_muted, NULL);

    // 设置视频解码器（通过flags控制）
    guint flags;
    g_object_get(ctx->pipeline, "flags", &flags, NULL);
    flags |= 0x01;  // video
    flags |= 0x02;  // audio
    flags &= ~0x04; // disable text
    g_object_set(ctx->pipeline, "flags", flags, NULL);

    // 低延迟：playbin queue2 尽量浅（勿用 GST_CLOCK_TIME_NONE — 那是无限缓冲）
    if (ctx->low_latency) {
        g_object_set(ctx->pipeline, "buffer-size", (gint64)0, NULL);
        g_object_set(ctx->pipeline, "buffer-duration", (gint64)(50 * GST_MSECOND), NULL);
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(ctx->pipeline),
                                          "buffer-time")) {
            g_object_set(ctx->pipeline, "buffer-time", (gint64)0, NULL);
        }
        LOGI("Low-latency playbin: buffer-size=0 buffer-duration=50ms");
    }

    // 显式创建 video sink，避免 playbin 默认 sink 为 NULL / 无法挂窗口
    if (ctx->video_sink != NULL) {
        gst_object_unref(ctx->video_sink);
        ctx->video_sink = NULL;
    }
    /* appsink + ANativeWindow直绘: glimagesink在夜神模拟器EGL_BAD_DISPLAY黑屏。
       RGBx由playbin videoconvert协商，new-sample回调锁Surface画帧。 */
    ctx->use_appsink = FALSE;
    ctx->first_frame_sent = FALSE;
    LOGI("Creating appsink for direct ANativeWindow draw...");
    ctx->video_sink = gst_element_factory_make("appsink", NULL);
    LOGI("appsink factory: %p", ctx->video_sink);
    if (ctx->video_sink == NULL) {
        LOGW("appsink unavailable, trying glimagesink");
        ctx->video_sink = gst_element_factory_make("glimagesink", NULL);
    }
    /* factory_make returns a floating ref; g_object_set(video-sink) sinks it
       into playbin. Take an owned ref so our later unref is not a double-free. */
    if (ctx->video_sink != NULL) gst_object_ref_sink(ctx->video_sink);
    if (ctx->video_sink != NULL &&
        g_strcmp0(GST_OBJECT_NAME(gst_element_get_factory(ctx->video_sink)), "appsink") == 0) {
        ctx->use_appsink = TRUE;
    }
    if (ctx->video_sink != NULL && ctx->use_appsink) {
        /* RGBx/RGBA only: memory layout matches WINDOW_FORMAT_RGBA_8888 on LE.
           BGRA would need swizzle — avoid for now. */
        GstCaps *caps = gst_caps_from_string("video/x-raw,format=(string){RGBx,RGBA}");
        g_object_set(ctx->video_sink,
                     "caps", caps,
                     "emit-signals", TRUE,
                     "sync", FALSE,
                     "max-buffers", 2,
                     "drop", TRUE,
                     NULL);
        gst_caps_unref(caps);
        g_signal_connect(ctx->video_sink, "new-sample",
                         G_CALLBACK(on_appsink_new_sample), ctx);
        LOGI("appsink configured (RGBx/RGBA, emit-signals, sync=FALSE, max-buffers=2)");
    }
    if (ctx->video_sink != NULL) {
        LOGI("Setting playbin video-sink...");
        g_object_set(ctx->pipeline, "video-sink", ctx->video_sink, NULL);
        gchar *sink_name = gst_element_get_name(ctx->video_sink);
        LOGI("Using video-sink: %s (appsink=%d)", sink_name ? sink_name : "(null)",
             ctx->use_appsink);
        if (sink_name != NULL) g_free(sink_name);

        if (!ctx->use_appsink &&
            g_signal_lookup("size-changed", G_OBJECT_TYPE(ctx->video_sink)) > 0) {
            g_signal_connect(ctx->video_sink, "size-changed",
                            G_CALLBACK(on_video_size_changed), ctx);
        }
    } else {
        LOGW("Failed to create video-sink, using playbin default");
        g_object_get(ctx->pipeline, "video-sink", &ctx->video_sink, NULL);
    }

    // 获取audio sink
    g_object_get(ctx->pipeline, "audio-sink", &ctx->audio_sink, NULL);

    // 设置source参数
    g_signal_connect(ctx->pipeline, "source-setup",
                    G_CALLBACK(on_source_setup), ctx);
    if (ctx->low_latency && g_signal_lookup("deep-element-added",
                                             G_OBJECT_TYPE(ctx->pipeline)) > 0) {
        g_signal_connect(ctx->pipeline, "deep-element-added",
                         G_CALLBACK(on_deep_element_added), ctx);
        LOGI("deep-element-added connected (flatten queues)");
    }

    // 设置bus消息监听（必须在主循环上下文中，ensure_main_loop已保证）
    GstBus *bus = gst_element_get_bus(ctx->pipeline);
    /* Sync handler: answer prepare-window-handle immediately on streaming thread
       (required when sink creates its window asynchronously). */
    gst_bus_set_sync_handler(bus, on_bus_sync, ctx, NULL);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(on_error), ctx);
    g_signal_connect(bus, "message::eos", G_CALLBACK(on_eos), ctx);
    g_signal_connect(bus, "message::state-changed", G_CALLBACK(on_state_changed), ctx);
    g_signal_connect(bus, "message::buffering", G_CALLBACK(on_buffering), ctx);
    g_signal_connect(bus, "message::duration-changed", G_CALLBACK(on_duration_changed), ctx);
    g_signal_connect(bus, "message::async-done", G_CALLBACK(on_async_done), ctx);
    gst_object_unref(bus);

    // 重置统计
    ctx->total_bytes_downloaded = 0;
    ctx->last_speed_check_time = 0;
    ctx->last_bytes_at_check = 0;

    /* Bring pipeline to READY first so VideoOverlay/window is valid, then
       attach ANativeWindow + expose (tutorial-3 pattern), then PLAYING. */
    LOGI("Setting pipeline READY (window=%p)...", ctx->native_window);
    GstStateChangeReturn ready_ret = gst_element_set_state(ctx->pipeline, GST_STATE_READY);
    LOGI("READY ret=%d", (int)ready_ret);
    if (ready_ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Failed to set pipeline to READY state");
        gst_object_unref(ctx->pipeline);
        ctx->pipeline = NULL;
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }

    /* playbin VideoOverlay only for glimagesink path; appsink draws itself. */
    if (!ctx->use_appsink) {
        if (ctx->native_window != NULL && GST_IS_VIDEO_OVERLAY(ctx->pipeline)) {
            gst_video_overlay_set_window_handle(
                GST_VIDEO_OVERLAY(ctx->pipeline),
                (guintptr)ctx->native_window
            );
            LOGI("playbin overlay window handle set");
        }

        if (ctx->video_sink != NULL && !GST_IS_VIDEO_OVERLAY(ctx->video_sink)) {
            GstElement *ov = GST_ELEMENT(gst_bin_get_by_interface(
                GST_BIN(ctx->pipeline), GST_TYPE_VIDEO_OVERLAY));
            if (ov != NULL) {
                gst_object_unref(ctx->video_sink);
                ctx->video_sink = ov;
                LOGI("Resolved VideoOverlay sink via interface");
            }
        }

        if (ctx->video_sink != NULL && ctx->native_window != NULL &&
            GST_IS_VIDEO_OVERLAY(ctx->video_sink)) {
            gst_video_overlay_set_window_handle(
                GST_VIDEO_OVERLAY(ctx->video_sink),
                (guintptr)ctx->native_window
            );
            gst_video_overlay_expose(GST_VIDEO_OVERLAY(ctx->video_sink));
            gst_video_overlay_expose(GST_VIDEO_OVERLAY(ctx->video_sink));
            LOGI("Video overlay window handle set (with expose x2)");
        }
    } else {
        LOGI("appsink path: window bound via ctx->native_window");
    }

    // 设置为播放状态
    LOGI("Setting pipeline PLAYING...");
    GstStateChangeReturn ret = gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
    LOGI("PLAYING ret=%d", (int)ret);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Failed to set pipeline to PLAYING state");
        gst_object_unref(ctx->pipeline);
        ctx->pipeline = NULL;
        g_mutex_unlock(&ctx->mutex);
        return -1;
    } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
        ctx->is_live_source = TRUE;
        LOGI("Live source detected (NO_PREROLL)");
    }

    ctx->is_playing = TRUE;

    if (ctx->low_latency) {
        tune_pipeline_queues(GST_BIN(ctx->pipeline));
    }

    // 发送连接成功消息
    latency_player_send_message(ctx, GST_MSG_CONNECTED, 0, 0);

    g_mutex_unlock(&ctx->mutex);
    LOGI("Playback started successfully (live=%d)", ctx->is_live_source);
    return 0;
}

int latency_player_stop(LatencyPlayerContext *ctx) {
    if (ctx == NULL) return -1;

    LOGI("Stopping playback");

    g_mutex_lock(&ctx->mutex);

    if (ctx->pipeline != NULL) {
        gst_element_set_state(ctx->pipeline, GST_STATE_NULL);
        gst_object_unref(ctx->pipeline);
        ctx->pipeline = NULL;
    }

    if (ctx->video_sink != NULL) {
        gst_object_unref(ctx->video_sink);
        ctx->video_sink = NULL;
    }

    if (ctx->audio_sink != NULL) {
        gst_object_unref(ctx->audio_sink);
        ctx->audio_sink = NULL;
    }

    ctx->is_playing = FALSE;
    ctx->is_live_source = FALSE;

    g_mutex_unlock(&ctx->mutex);
    return 0;
}

int latency_player_pause(LatencyPlayerContext *ctx) {
    if (ctx == NULL) return -1;

    LOGD("Pausing playback");

    g_mutex_lock(&ctx->mutex);
    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }
    GstStateChangeReturn ret = gst_element_set_state(ctx->pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Failed to pause");
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }

    ctx->is_playing = FALSE;
    g_mutex_unlock(&ctx->mutex);
    return 0;
}

int latency_player_resume(LatencyPlayerContext *ctx) {
    if (ctx == NULL) return -1;

    LOGD("Resuming playback");

    g_mutex_lock(&ctx->mutex);
    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }
    GstStateChangeReturn ret = gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Failed to resume");
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }

    ctx->is_playing = TRUE;
    g_mutex_unlock(&ctx->mutex);
    return 0;
}

int latency_player_set_surface(LatencyPlayerContext *ctx, ANativeWindow *window) {
    if (ctx == NULL) return -1;

    LOGI("Setting native window: %p (old=%p)", window, ctx->native_window);

    g_mutex_lock(&ctx->mutex);

    /* Same ANativeWindow: tutorial-3 only re-exposes; set_window_handle while
       PLAYING is illegal for glimagesink and causes a permanent black screen.
       appsink reads ctx->native_window each frame — nothing to rebind. */
    if (window != NULL && ctx->native_window != NULL &&
        window == ctx->native_window) {
        LOGI("set_surface: same window %p", window);
        if (!ctx->use_appsink && ctx->pipeline != NULL &&
            GST_IS_VIDEO_OVERLAY(ctx->pipeline) && ctx->is_playing) {
            gst_video_overlay_expose(GST_VIDEO_OVERLAY(ctx->pipeline));
            gst_video_overlay_expose(GST_VIDEO_OVERLAY(ctx->pipeline));
            LOGI("set_surface: same window, expose x2 only");
        }
        ANativeWindow_release(window);
        g_mutex_unlock(&ctx->mutex);
        return 0;
    }

    // 释放旧的window引用 (window_mutex: 流线程 draw 每帧读 native_window，
    // 写者必须与其同步；本段绝不包含 gst set_state)
    g_mutex_lock(&ctx->window_mutex);
    if (ctx->native_window != NULL) {
        ANativeWindow_release(ctx->native_window);
        ctx->native_window = NULL;
    }
    ctx->window_geo_w = 0;
    ctx->window_geo_h = 0;

    // 设置新的window (JNI层已调用ANativeWindow_fromSurface — 直接接管该引用)
    if (window != NULL) {
        ctx->native_window = window;
        LOGI("native_window acquired: %p", ctx->native_window);
    }
    g_mutex_unlock(&ctx->window_mutex);

    /* glimagesink accepts a new window only in NULL/READY (GstVideoOverlay docs).
       appsink does not need READY bounce — just swap native_window. */
    gboolean was_playing = FALSE;
    if (ctx->pipeline != NULL && ctx->is_playing && !ctx->use_appsink) {
        was_playing = TRUE;
        LOGI("set_surface: pipeline PLAYING → READY for window rebind");
        gst_element_set_state(ctx->pipeline, GST_STATE_READY);
    }

    if (!ctx->use_appsink && ctx->pipeline != NULL && ctx->native_window != NULL &&
        GST_IS_VIDEO_OVERLAY(ctx->pipeline)) {
        gst_video_overlay_set_window_handle(
            GST_VIDEO_OVERLAY(ctx->pipeline),
            (guintptr)ctx->native_window
        );
        LOGI("set_surface: playbin overlay window handle set");
    }
    if (!ctx->use_appsink && ctx->video_sink != NULL && ctx->native_window != NULL &&
        GST_IS_VIDEO_OVERLAY(ctx->video_sink)) {
        gst_video_overlay_set_window_handle(
            GST_VIDEO_OVERLAY(ctx->video_sink),
            (guintptr)ctx->native_window
        );
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(ctx->video_sink));
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(ctx->video_sink));
        LOGI("set_surface: window handle + expose x2");
    } else if (!ctx->use_appsink && ctx->video_sink != NULL && ctx->native_window != NULL) {
        LOGW("set_surface: video_sink %s does not implement VideoOverlay",
             G_OBJECT_TYPE_NAME(ctx->video_sink));
    } else if (ctx->use_appsink) {
        LOGI("set_surface: appsink will draw to new window on next sample");
    }

    if (was_playing && ctx->pipeline != NULL) {
        gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
        LOGI("set_surface: resumed PLAYING after rebind");
    }

    g_mutex_unlock(&ctx->mutex);
    return 0;
}

int latency_player_set_mute(LatencyPlayerContext *ctx, gboolean mute) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);
    ctx->is_muted = mute;

    if (ctx->pipeline != NULL) {
        g_object_set(ctx->pipeline, "mute", mute, NULL);
    }
    g_mutex_unlock(&ctx->mutex);

    return 0;
}

int latency_player_set_volume(LatencyPlayerContext *ctx, gdouble volume) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);
    ctx->volume = CLAMP(volume, 0.0, 1.0);

    if (ctx->pipeline != NULL) {
        g_object_set(ctx->pipeline, "volume", ctx->volume, NULL);
    }
    g_mutex_unlock(&ctx->mutex);

    return 0;
}

int latency_player_set_buffer_time(LatencyPlayerContext *ctx, gint buffer_ms) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);
    ctx->buffer_time_ms = CLAMP(buffer_ms, 0, 5000);

    if (ctx->pipeline != NULL) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(ctx->pipeline),
                                          "buffer-time")) {
            g_object_set(ctx->pipeline, "buffer-time",
                        (gint64)(ctx->buffer_time_ms * GST_MSECOND), NULL);
        }
        if (ctx->low_latency) {
            g_object_set(ctx->pipeline, "buffer-duration", (gint64)0, NULL);
            g_object_set(ctx->pipeline, "buffer-size", (gint64)0, NULL);
        }
    }
    g_mutex_unlock(&ctx->mutex);

    return 0;
}

int latency_player_set_low_latency(LatencyPlayerContext *ctx, gboolean enable) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);
    ctx->low_latency = enable;
    LOGI("Low-latency mode: %d", (int)enable);

    if (ctx->pipeline != NULL) {
        if (enable) {
            g_object_set(ctx->pipeline, "buffer-size", (gint64)0, NULL);
            g_object_set(ctx->pipeline, "buffer-duration", (gint64)(50 * GST_MSECOND), NULL);
            if (g_object_class_find_property(G_OBJECT_GET_CLASS(ctx->pipeline),
                                              "buffer-time")) {
                g_object_set(ctx->pipeline, "buffer-time", (gint64)0, NULL);
            }
            tune_pipeline_queues(GST_BIN(ctx->pipeline));

            GstElement *source = NULL;
            g_object_get(ctx->pipeline, "source", &source, NULL);
            if (source != NULL) {
                if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "latency")) {
                    g_object_set(source, "latency", (guint64)0, NULL);
                }
                gst_object_unref(source);
            }
        }
    }
    g_mutex_unlock(&ctx->mutex);

    return 0;
}

int latency_player_set_decoder_type(LatencyPlayerContext *ctx, gint decoder_type) {
    if (ctx == NULL) return -1;

    ctx->decoder_type = decoder_type;
    LOGI("Decoder type set to: %d (0=HW, 1=SW, 2=Auto)", decoder_type);

    // 注意：decoder_type更改需要重启播放才能生效
    // 如果需要运行时切换，需要重新构建pipeline

    return 0;
}

int latency_player_set_flip(LatencyPlayerContext *ctx, gboolean flip_h, gboolean flip_v) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);

    ctx->flip_horizontal = flip_h;
    ctx->flip_vertical = flip_v;

    // 创建或更新videoflip元素作为playbin的video-filter
    if (ctx->pipeline != NULL) {
        // 如果已有flip元素，先移除
        if (ctx->video_flip != NULL) {
            g_object_set(ctx->pipeline, "video-filter", NULL, NULL);
            gst_object_unref(ctx->video_flip);
            ctx->video_flip = NULL;
        }

        // 如果需要翻转，创建videoflip元素
        if (flip_h || flip_v) {
            ctx->video_flip = gst_element_factory_make("videoflip", "video_flip");
            if (ctx->video_flip != NULL) {
                // 设置翻转方法
                int method = 0;
                if (flip_h && flip_v) {
                    method = 2; // rotate-180
                } else if (flip_h) {
                    method = 4; // flip-horizontal (GST_VIDEO_FLIP_METHOD_HORIZONTAL)
                } else if (flip_v) {
                    method = 5; // flip-vertical (GST_VIDEO_FLIP_METHOD_VERTICAL)
                }
                g_object_set(ctx->video_flip, "method", method, NULL);

                // 设置为playbin的video-filter
                g_object_set(ctx->pipeline, "video-filter", ctx->video_flip, NULL);
                LOGI("Video flip applied: h=%d, v=%d, method=%d", flip_h, flip_v, method);
            } else {
                LOGW("Failed to create videoflip element");
            }
        } else {
            LOGI("Video flip cleared");
        }
    }

    g_mutex_unlock(&ctx->mutex);
    return 0;
}

int latency_player_set_rotation(LatencyPlayerContext *ctx, gint degrees) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);

    ctx->rotation_degrees = degrees % 360;

    if (ctx->pipeline != NULL) {
        // 如果已有flip元素，先移除
        if (ctx->video_flip != NULL) {
            g_object_set(ctx->pipeline, "video-filter", NULL, NULL);
            gst_object_unref(ctx->video_flip);
            ctx->video_flip = NULL;
        }

        // 如果需要旋转，创建videoflip元素
        if (degrees != 0) {
            ctx->video_flip = gst_element_factory_make("videoflip", "video_flip");
            if (ctx->video_flip != NULL) {
                // 将角度转换为videoflip的method
                // GST_VIDEO_FLIP_METHOD_90R = 1
                // GST_VIDEO_FLIP_METHOD_180 = 2
                // GST_VIDEO_FLIP_METHOD_90L = 3
                int method = 0;
                switch (degrees) {
                    case 90:  method = 1; break; // rotate-90 (clockwise)
                    case 180: method = 2; break; // rotate-180
                    case 270: method = 3; break; // rotate-270 (counter-clockwise 90)
                    default:  method = 0; break; // no rotation
                }
                g_object_set(ctx->video_flip, "method", method, NULL);

                // 设置为playbin的video-filter
                g_object_set(ctx->pipeline, "video-filter", ctx->video_flip, NULL);
                LOGI("Video rotation applied: %d degrees (method=%d)", degrees, method);
            } else {
                LOGW("Failed to create videoflip element for rotation");
            }
        } else {
            LOGI("Video rotation cleared");
        }
    }

    g_mutex_unlock(&ctx->mutex);
    return 0;
}

int latency_player_save_snapshot(LatencyPlayerContext *ctx, const char *path) {
    if (ctx == NULL || path == NULL || path[0] == '\0')
        return -1;

    LOGI("Saving snapshot to: %s", path);

    /* Take the sample under ctx->mutex: stop() unrefs ctx->video_sink and the
       pipeline under this lock; reading them unlocked raced with teardown.
       Only the acquisition phase is locked — the sample keeps its own refs,
       so the (potentially multi-second) encode below runs unlocked. */
    g_mutex_lock(&ctx->mutex);
    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }

    /* last-sample 是 appsink/playbin 的属性，优先取我们创建的 video_sink */
    GstSample *sample = NULL;
    GObject *sample_owner = NULL;
    if (ctx->video_sink != NULL &&
        g_object_class_find_property(G_OBJECT_GET_CLASS(ctx->video_sink),
                                     "sample")) {
        sample_owner = G_OBJECT(ctx->video_sink);
    } else if (g_object_class_find_property(G_OBJECT_GET_CLASS(ctx->pipeline),
                                            "sample")) {
        sample_owner = G_OBJECT(ctx->pipeline);
    }
    if (sample_owner == NULL) {
        LOGW("Snapshot: no sample-capable element available.");
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(sample_owner),
                                     "enable-last-sample")) {
        gboolean last_sample = FALSE;
        g_object_get(sample_owner, "enable-last-sample", &last_sample, NULL);
        if (!last_sample)
            g_object_set(sample_owner, "enable-last-sample", TRUE, NULL);
    }

    g_object_get(sample_owner, "sample", &sample, NULL);
    g_mutex_unlock(&ctx->mutex);

    if (sample == NULL) {
        LOGW("Snapshot: no frame yet (pipeline not playing or no video output).");
        return -1;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    if (buffer == NULL || caps == NULL || gst_caps_is_empty(caps)) {
        LOGW("Snapshot: sample has no buffer/caps");
        gst_sample_unref(sample);
        return -1;
    }

    GstStructure *s = gst_caps_get_structure(caps, 0);
    gint width = 0, height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    LOGI("Snapshot size: %dx%d", width, height);

    gchar *lower = g_ascii_strdown(path, -1);
    gboolean is_jpeg = g_str_has_suffix(lower, ".jpg") ||
                       g_str_has_suffix(lower, ".jpeg");
    g_free(lower);

    /* 临时管线: appsrc(帧) -> videoconvert(RGB) -> pngenc/jpegenc -> filesink。
       location 走 g_object_set 而非解析字符串, 避免路径转义问题. */
    gchar *desc = g_strdup_printf(
        "appsrc name=src ! "
        "videoconvert ! video/x-raw,format=RGB ! "
        "%s ! filesink name=sink",
        is_jpeg ? "jpegenc quality=90" : "pngenc");
    GError *error = NULL;
    GstElement *snap = gst_parse_launch(desc, &error);
    g_free(desc);
    if (snap == NULL) {
        LOGW("Snapshot: pipeline build failed: %s",
             error ? error->message : "unknown");
        g_clear_error(&error);
        gst_sample_unref(sample);
        return -1;
    }

    GstElement *src = gst_bin_get_by_name(GST_BIN(snap), "src");
    GstElement *sink = gst_bin_get_by_name(GST_BIN(snap), "sink");
    g_object_set(sink, "location", path, NULL);
    g_object_set(src, "caps", caps, NULL);

    gst_element_set_state(snap, GST_STATE_PLAYING);

    /* push_buffer 获得 buffer 所有权, 先补一个引用; sample 持有原始引用 */
    gst_buffer_ref(buffer);
    GstFlowReturn flow = gst_app_src_push_buffer(GST_APP_SRC(src), buffer);
    if (flow != GST_FLOW_OK)
        LOGW("Snapshot: push_buffer flow=%s", gst_flow_get_name(flow));
    gst_app_src_end_of_stream(GST_APP_SRC(src));

    GstBus *bus = gst_element_get_bus(snap);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
                                                 GST_MESSAGE_EOS |
                                                 GST_MESSAGE_ERROR);
    int ret = 0;
    if (msg == NULL) {
        LOGW("Snapshot: timeout waiting for encode EOS");
        ret = -1;
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *err = NULL;
        gchar *dbg = NULL;
        gst_message_parse_error(msg, &err, &dbg);
        LOGW("Snapshot: encode error: %s [%s]",
             err ? err->message : "?", dbg ? dbg : "");
        g_clear_error(&err);
        g_free(dbg);
        ret = -1;
    }
    if (msg) gst_message_unref(msg);
    if (bus) gst_object_unref(bus);

    gst_element_set_state(snap, GST_STATE_NULL);
    gst_object_unref(snap);
    gst_object_unref(src);
    gst_object_unref(sink);
    gst_sample_unref(sample);

    if (ret == 0) {
        GStatBuf st;
        if (g_stat(path, &st) != 0 || st.st_size <= 0) {
            LOGW("Snapshot: file missing or empty: %s", path);
            ret = -1;
        } else {
            LOGI("Snapshot saved: %s (%" G_GINT64_FORMAT " bytes)",
                 path, (gint64)st.st_size);
        }
    }
    return ret;
}

gint64 latency_player_get_position(LatencyPlayerContext *ctx) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);
    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }

    gint64 position = 0;
    if (gst_element_query_position(ctx->pipeline, GST_FORMAT_TIME, &position)) {
        g_mutex_unlock(&ctx->mutex);
        return position / GST_MSECOND; // 转换为毫秒
    }
    g_mutex_unlock(&ctx->mutex);
    return -1;
}

gint64 latency_player_get_duration(LatencyPlayerContext *ctx) {
    if (ctx == NULL) return -1;

    g_mutex_lock(&ctx->mutex);
    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return -1;
    }

    gint64 duration = 0;
    if (gst_element_query_duration(ctx->pipeline, GST_FORMAT_TIME, &duration)) {
        g_mutex_unlock(&ctx->mutex);
        return duration / GST_MSECOND; // 转换为毫秒
    }
    g_mutex_unlock(&ctx->mutex);
    return -1;
}

int latency_player_set_rtmp_timeout(LatencyPlayerContext *ctx, gint timeout) {
    if (ctx == NULL) return -1;
    ctx->rtmp_timeout = CLAMP(timeout, 1, 60);
    return 0;
}

int latency_player_set_rtmp_retry(LatencyPlayerContext *ctx, gint retry_count) {
    if (ctx == NULL) return -1;
    ctx->rtmp_retry_count = CLAMP(retry_count, 0, 10);
    return 0;
}

int latency_player_switch_uri(LatencyPlayerContext *ctx, const char *uri) {
    if (ctx == NULL || uri == NULL) return -1;

    LOGI("Switching URI to: %s", uri);

    // 停止当前播放
    latency_player_stop(ctx);

    // 更新URI (under lock: on_buffering reads ctx->uri under the same lock)
    g_mutex_lock(&ctx->mutex);
    if (ctx->uri != NULL) {
        g_free(ctx->uri);
    }
    ctx->uri = g_strdup(uri);
    g_mutex_unlock(&ctx->mutex);

    // 重新播放
    return latency_player_play(ctx, uri);
}

gint64 latency_player_get_download_speed(LatencyPlayerContext *ctx) {
    if (ctx == NULL) return 0;

    gint64 current_time = g_get_monotonic_time();
    gint64 current_bytes = ctx->total_bytes_downloaded;

    if (ctx->last_speed_check_time == 0) {
        ctx->last_speed_check_time = current_time;
        ctx->last_bytes_at_check = current_bytes;
        return 0;
    }

    gint64 time_diff = current_time - ctx->last_speed_check_time;
    if (time_diff <= 0) return 0;

    gint64 bytes_diff = current_bytes - ctx->last_bytes_at_check;
    gint64 speed = (bytes_diff * G_USEC_PER_SEC) / time_diff; // bytes per second

    ctx->last_speed_check_time = current_time;
    ctx->last_bytes_at_check = current_bytes;

    return speed;
}

void latency_player_release(LatencyPlayerContext *ctx) {
    if (ctx == NULL) return;

    LOGI("Releasing LatencyPlayer context");

    // 停止播放
    latency_player_stop(ctx);

    // 释放URI
    if (ctx->uri != NULL) {
        g_free(ctx->uri);
        ctx->uri = NULL;
    }

    // 释放Native Window (window_mutex 与流线程 draw 同步)
    g_mutex_lock(&ctx->window_mutex);
    if (ctx->native_window != NULL) {
        ANativeWindow_release(ctx->native_window);
        ctx->native_window = NULL;
    }
    g_mutex_unlock(&ctx->window_mutex);

    // 释放video/audio sink
    if (ctx->video_sink != NULL) {
        gst_object_unref(ctx->video_sink);
        ctx->video_sink = NULL;
    }
    if (ctx->audio_sink != NULL) {
        gst_object_unref(ctx->audio_sink);
        ctx->audio_sink = NULL;
    }

    // 释放其他元素
    if (ctx->video_flip != NULL) {
        gst_object_unref(ctx->video_flip);
        ctx->video_flip = NULL;
    }
    if (ctx->video_scale != NULL) {
        gst_object_unref(ctx->video_scale);
        ctx->video_scale = NULL;
    }
    if (ctx->video_convert != NULL) {
        gst_object_unref(ctx->video_convert);
        ctx->video_convert = NULL;
    }

    // 释放互斥锁
    g_mutex_clear(&ctx->mutex);
    g_mutex_clear(&ctx->window_mutex);

    ctx->initialized = FALSE;
    LOGI("LatencyPlayer context released");
}

void latency_player_send_message(LatencyPlayerContext *ctx, int type, long param1, long param2) {
    if (ctx == NULL || ctx->java_vm == NULL) return;

    JNIEnv *env = NULL;
    gboolean attached = FALSE;

    // 获取当前线程的JNIEnv
    int status = (*ctx->java_vm)->GetEnv(ctx->java_vm, (void **)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if ((*ctx->java_vm)->AttachCurrentThread(ctx->java_vm, &env, NULL) == JNI_OK) {
            attached = TRUE;
        } else {
            LOGE("Failed to attach thread to JVM");
            return;
        }
    } else if (status != JNI_OK) {
        LOGE("Failed to get JNIEnv");
        return;
    }

    // 调用Java层的onGStreamerMessage方法
    jclass cls = (*env)->GetObjectClass(env, ctx->java_obj);
    jmethodID mid = (*env)->GetMethodID(env, cls, "onGStreamerMessage", "(IJJ)V");
    if (mid != NULL) {
        (*env)->CallVoidMethod(env, ctx->java_obj, mid, type, (jlong)param1, (jlong)param2);
    } else {
        LOGE("Failed to find onGStreamerMessage method");
    }

    // 检查异常
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }

    (*env)->DeleteLocalRef(env, cls);

    // 分离线程
    if (attached) {
        (*ctx->java_vm)->DetachCurrentThread(ctx->java_vm);
    }
}

/* ==============================================================================
 * GStreamer回调函数实现
 * ============================================================================== */

/* Answer prepare-window-handle on the streaming thread (GstVideoOverlay docs). */
static GstBusSyncReply on_bus_sync(GstBus *bus, GstMessage *message, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    (void)bus;

    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ELEMENT &&
        gst_message_has_name(message, "prepare-window-handle")) {
        GstVideoOverlay *overlay = NULL;
        if (ctx != NULL && ctx->pipeline != NULL && GST_IS_VIDEO_OVERLAY(ctx->pipeline)) {
            overlay = GST_VIDEO_OVERLAY(ctx->pipeline);
        } else if (ctx != NULL && ctx->video_sink != NULL &&
                   GST_IS_VIDEO_OVERLAY(ctx->video_sink)) {
            overlay = GST_VIDEO_OVERLAY(ctx->video_sink);
        }
        ANativeWindow *win = NULL;
        if (ctx != NULL) {
            g_mutex_lock(&ctx->window_mutex);
            win = ctx->native_window;
            g_mutex_unlock(&ctx->window_mutex);
        }
        if (overlay != NULL && win != NULL) {
            gst_video_overlay_set_window_handle(overlay, (guintptr)win);
            LOGI("prepare-window-handle: window set %p", win);
        } else {
            LOGW("prepare-window-handle: no window/overlay yet");
        }
        return GST_BUS_DROP;
    }
    return GST_BUS_PASS;
}

static gboolean message_is_from_current_pipeline(LatencyPlayerContext *ctx,
                                                  GstMessage *msg) {
    if (ctx->pipeline == NULL || msg->src == NULL) {
        return FALSE;
    }
    if (!GST_IS_OBJECT(msg->src) || !GST_IS_OBJECT(ctx->pipeline)) {
        return FALSE;
    }
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(ctx->pipeline)) {
        return TRUE;
    }
    return gst_object_has_as_ancestor(msg->src, GST_OBJECT(ctx->pipeline));
}

static void on_error(GstBus *bus, GstMessage *msg, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    GError *err = NULL;
    gchar *debug = NULL;

    /* Hold ctx->mutex: serializes against play/stop/set_surface tearing the
       pipeline down — message_is_from_current_pipeline dereferences
       ctx->pipeline, which stop() unrefs under the same lock. */
    g_mutex_lock(&ctx->mutex);

    /* Drop errors emitted by a pipeline that was already replaced during
       reconnect — a stale error must not abort the new attempt. */
    if (!message_is_from_current_pipeline(ctx, msg)) {
        LOGD("Ignoring error from stale pipeline element");
        g_mutex_unlock(&ctx->mutex);
        return;
    }

    gst_message_parse_error(msg, &err, &debug);

    LOGE("GStreamer Error: %s", err->message);
    if (debug != NULL) {
        LOGD("Debug info: %s", debug);
    }

    /* Map by domain/code carefully — GL/render failures must NOT become
       "RTMP stream not found" (1005). Only real resource errors map there. */
    int error_code = 1003; // 默认RTMP连接失败
    gboolean is_gl_error = (debug != NULL && (strstr(debug, "gl") != NULL ||
                            strstr(debug, "GL") != NULL ||
                            strstr(debug, "OpenGL") != NULL ||
                            strstr(debug, "EGL") != NULL)) ||
                           (err->message != NULL && (strstr(err->message, "OpenGL") != NULL ||
                            strstr(err->message, "OpenGL config") != NULL ||
                            strstr(err->message, "GST_GL") != NULL));
    if (is_gl_error) {
        error_code = 3001; // RENDER_INIT_FAILED
    } else if (err->domain == GST_STREAM_ERROR) {
        switch (err->code) {
            case GST_STREAM_ERROR_DECODE:
                error_code = 2001; // VIDEO_DECODE_FAILED
                break;
            case GST_STREAM_ERROR_CODEC_NOT_FOUND:
                error_code = 2002; // VIDEO_CODEC_NOT_SUPPORTED
                break;
            default:
                break;
        }
    } else if (err->domain == GST_RESOURCE_ERROR) {
        switch (err->code) {
            case GST_RESOURCE_ERROR_NOT_FOUND:
                error_code = 1005; // RTMP_STREAM_NOT_FOUND
                break;
            case GST_RESOURCE_ERROR_OPEN_READ:
            case GST_RESOURCE_ERROR_READ:
                error_code = 1003; // RTMP_CONNECT_FAILED
                break;
            default:
                break;
        }
    }

    latency_player_send_message(ctx, GST_MSG_ERROR, error_code, 0);

    g_error_free(err);
    g_free(debug);
    g_mutex_unlock(&ctx->mutex);
}

static void on_eos(GstBus *bus, GstMessage *msg, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;

    g_mutex_lock(&ctx->mutex);
    if (!message_is_from_current_pipeline(ctx, msg)) {
        LOGD("Ignoring EOS from stale pipeline element");
        g_mutex_unlock(&ctx->mutex);
        return;
    }

    LOGI("End of stream");

    ctx->is_playing = FALSE;
    latency_player_send_message(ctx, GST_MSG_EOS, 0, 0);
    g_mutex_unlock(&ctx->mutex);
}

static void on_state_changed(GstBus *bus, GstMessage *msg, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    GstState old_state, new_state, pending_state;

    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    /* Serialize with play/stop: stop() unrefs ctx->pipeline under this lock;
       comparing/dereferencing it unlocked crashed against a concurrent stop. */
    g_mutex_lock(&ctx->mutex);
    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return;
    }

    // 只处理pipeline的状态变化
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(ctx->pipeline)) {
        LOGD("State changed: %s -> %s",
             gst_element_state_get_name(old_state),
             gst_element_state_get_name(new_state));

        // 发送状态变化消息
        latency_player_send_message(ctx, GST_MSG_STATE_CHANGED, new_state, 0);

        // appsink: real FIRST_FRAME comes from on_appsink_new_sample only.
        if (!ctx->use_appsink &&
            new_state == GST_STATE_PLAYING && old_state != GST_STATE_PLAYING) {
            latency_player_send_message(ctx, GST_MSG_FIRST_FRAME, 0, 0);
        }
    }
    g_mutex_unlock(&ctx->mutex);
}

static void on_buffering(GstBus *bus, GstMessage *msg, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    gint percent = 0;
    (void)bus;

    gst_message_parse_buffering(msg, &percent);

    /* Hold ctx->mutex across the whole handler: the live branch calls
       set_state(PLAYING), which crashed when it raced latency_player_stop's
       set_state(NULL) teardown (tombstone_06: SIGSEGV in playbin
       activate_group via on_buffering). With the lock, stop() either
       completes first (pipeline == NULL here → skip) or the handler runs
       first on a healthy pipeline. */
    g_mutex_lock(&ctx->mutex);

    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return;
    }

    /* Live/RTMP: stay PLAYING; do NOT report intermediate queue2 percentages
       (live queue2 often sticks at ~20-35% and never reaches 100). */
    if (ctx->is_live_source || g_str_has_prefix(ctx->uri ? ctx->uri : "", "rtmp")) {
        /* Rate-limit: queue2 fires every buffer — flood kills logcat. */
        static gint64 last_log_time = 0;
        gint64 now = g_get_monotonic_time();
        if (now - last_log_time > G_USEC_PER_SEC) {
            LOGD("Buffering (live, ignore UI): %d%%", percent);
            last_log_time = now;
        }
        if (ctx->is_playing) {
            gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
        }
        if (ctx->low_latency && percent > 0 && percent < 100) {
            tune_pipeline_queues(GST_BIN(ctx->pipeline));
        }
        /* Clear UI buffering only once when fully ready (or first non-zero). */
        if (percent >= 100) {
            latency_player_send_message(ctx, GST_MSG_BUFFERING, 100, 0);
        }
        g_mutex_unlock(&ctx->mutex);
        return;
    }

    LOGD("Buffering: %d%% (live=%d)", percent, ctx->is_live_source);
    latency_player_send_message(ctx, GST_MSG_BUFFERING, percent, 0);

    if (percent < 100) {
        gst_element_set_state(ctx->pipeline, GST_STATE_PAUSED);
    } else {
        gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
    }
    g_mutex_unlock(&ctx->mutex);
}

static void on_duration_changed(GstBus *bus, GstMessage *msg, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    gint64 duration = 0;

    gst_message_parse_duration(msg, NULL, &duration);
    LOGD("Duration changed: %" G_GINT64_FORMAT, duration);

    latency_player_send_message(ctx, GST_MSG_DURATION_CHANGED, duration / GST_MSECOND, 0);
}

static void on_async_done(GstBus *bus, GstMessage *msg, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;
    (void)bus;
    (void)msg;
    LOGI("Pipeline async-done (preroll complete)");
    /* Same lock discipline as on_buffering: tune walks ctx->pipeline, which
       stop() frees under this lock. */
    g_mutex_lock(&ctx->mutex);
    if (ctx->pipeline == NULL) {
        g_mutex_unlock(&ctx->mutex);
        return;
    }
    if (ctx->low_latency) {
        tune_pipeline_queues(GST_BIN(ctx->pipeline));
    }
    /* appsink: do not fake FIRST_FRAME — wait for real draw. */
    if (!ctx->use_appsink && ctx->is_playing) {
        latency_player_send_message(ctx, GST_MSG_FIRST_FRAME, 0, 0);
    }
    g_mutex_unlock(&ctx->mutex);
}

static void on_video_size_changed(GstElement *element, gint width, gint height,
                                   gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;

    ctx->video_width = width;
    ctx->video_height = height;

    LOGI("Video size changed: %dx%d", width, height);
    latency_player_send_message(ctx, GST_MSG_VIDEO_SIZE, (long)width, (long)height);
}

static void on_first_video_frame(GstElement *element, GstPad *pad, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;

    LOGI("First video frame received");
    latency_player_send_message(ctx, GST_MSG_FIRST_FRAME, 0, 0);
}

static guint src_probe_count = 0;

static GstPadProbeReturn log_src_buffer(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    (void)pad;
    (void)user_data;
    guint n = (guint)g_atomic_int_add((volatile gint *)&src_probe_count, 1);
    if (n <= 3 || (n % 500) == 0) {
        LOGI("src pad buffer #%u size=%" G_GSIZE_FORMAT, n,
             gst_buffer_get_size(GST_PAD_PROBE_INFO_BUFFER(info)));
    }
    return GST_PAD_PROBE_OK;
}

static void on_source_setup(GstElement *playbin, GstElement *source, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;

    if (source == NULL) return;

    /* gst_plugin_feature_get_name() returns a const string owned by the
     * plugin feature (transfer none) -- never g_free() it. Freeing it corrupts
     * the allocator and later crashes on the next source-setup. */
    const gchar *source_name = gst_plugin_feature_get_name(
        GST_PLUGIN_FEATURE(gst_element_get_factory(source)));
    LOGI("Source setup: %s", source_name);

    // 针对RTMP source设置参数（不在URI上拼空格参数，避免uri属性解析失败）
    if (g_str_has_prefix(ctx->uri, "rtmp://") || g_str_has_prefix(ctx->uri, "rtmps://")) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "timeout")) {
            g_object_set(source, "timeout", (guint)ctx->rtmp_timeout, NULL);
        }
        // librtmp rtmpsrc: live streams need live=1; set via location space-arg
        if (g_strcmp0(source_name, "rtmpsrc") == 0 &&
            g_object_class_find_property(G_OBJECT_GET_CLASS(source), "location")) {
            gchar *loc = NULL;
            g_object_get(source, "location", &loc, NULL);
            if (loc != NULL && strstr(loc, "live=") == NULL) {
                gchar *live_loc = g_strdup_printf("%s live=1", loc);
                g_object_set(source, "location", live_loc, NULL);
                g_free(live_loc);
            }
            g_free(loc);
        }
        LOGI("RTMP source configured: timeout=%d name=%s low_latency=%d",
             ctx->rtmp_timeout, source_name, (int)ctx->low_latency);
    }

    if (g_str_has_prefix(ctx->uri, "rtsp://") || g_str_has_prefix(ctx->uri, "rtsps://")) {
        if (g_strcmp0(source_name, "rtspsrc") == 0 &&
            g_object_class_find_property(G_OBJECT_GET_CLASS(source), "latency")) {
            guint lat = ctx->low_latency ? 80u : 200u;
            g_object_set(source, "latency", lat, NULL);
            LOGI("RTSP source configured: rtspsrc latency=%ums", lat);
        }
        /* rtspsrc defaults udp-buffer-size to 512KB. The kernel caps the UDP
         * receive buffer at net.core.rmem_max, so gstudpsrc's forced
         * SO_RCVBUFFORCE fallback always fails on Android (no CAP_NET_ADMIN)
         * and its GError stays NULL, which crashes on opt_err->message.
         * udp-buffer-size=0 leaves udpsrc's buffer-size at its 0 default, so
         * the whole SO_RCVBUF/SO_RCVBUFFORCE block is skipped. */
        if (g_strcmp0(source_name, "rtspsrc") == 0 &&
            g_object_class_find_property(G_OBJECT_GET_CLASS(source), "udp-buffer-size")) {
            g_object_set(source, "udp-buffer-size", 0, NULL);
            LOGI("RTSP source configured: rtspsrc udp-buffer-size=0");
        }
    }

    if (g_str_has_prefix(ctx->uri, "http://") || g_str_has_prefix(ctx->uri, "https://")) {
        if (g_strcmp0(source_name, "souphttpsrc") == 0) {
            GParamSpec *ps = g_object_class_find_property(
                G_OBJECT_GET_CLASS(source), "timeout");
            if (ps != NULL && ps->value_type == G_TYPE_UINT64) {
                g_object_set(source, "timeout", (guint64)10, NULL);
            } else if (ps != NULL && ps->value_type == G_TYPE_UINT) {
                g_object_set(source, "timeout", (guint)10, NULL);
            }
            LOGI("HTTP source configured: souphttpsrc timeout prop type ok=%d",
                 ps != NULL);
        }
    }

    src_probe_count = 0;
    {
        GstPad *srcpad = gst_element_get_static_pad(source, "src");
        if (srcpad != NULL) {
            gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BUFFER,
                              log_src_buffer, NULL, NULL);
            gst_object_unref(srcpad);
            LOGI("src pad buffer probe attached");
        }
    }

    /* 低延迟：源上有 latency 则压到 0（jitterbuffer 等），rtspsrc 已按 ms 单独处理 */
    if (ctx->low_latency && g_strcmp0(source_name, "rtspsrc") != 0 &&
        g_object_class_find_property(G_OBJECT_GET_CLASS(source), "latency")) {
        g_object_set(source, "latency", (guint64)0, NULL);
        LOGI("source latency forced to 0 (low-latency)");
    }
}

static void on_video_decoder_changed(GstElement *element, GParamSpec *pspec, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;

    gchar *decoder_name = NULL;
    g_object_get(element, "current-audio", &decoder_name, NULL);

    if (decoder_name != NULL) {
        LOGI("Video decoder changed to: %s", decoder_name);
        g_free(decoder_name);
    }
}
