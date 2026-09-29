/**
 * LatencyPlayer JNI - Java Native Interface实现
 *
 * 桥接Java层和Native层，提供JNI方法实现。
 */

#include <jni.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include <android/native_window_jni.h>
#include <pthread.h>

#include "latency_player_core.h"

/* Static plugin registration (defined in gstreamer_android.c) */
extern void gst_init_static_plugins(void);

#define LOG_TAG "LatencyPlayerJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

/* ==============================================================================
 * 全局变量
 * ============================================================================== */

/** GStreamer初始化标志 */
static gboolean gst_initialized = FALSE;

/** GStreamer初始化锁 */
static pthread_mutex_t gst_init_lock = PTHREAD_MUTEX_INITIALIZER;

static void gst_log_to_logcat(GstDebugCategory *category,
                              GstDebugLevel level,
                              const gchar *file,
                              const gchar *function,
                              gint line,
                              GObject *object,
                              GstDebugMessage *message,
                              gpointer user_data) {
    (void)file;
    (void)function;
    (void)line;
    (void)object;
    (void)user_data;

    if (level == GST_LEVEL_NONE) return;
    if (!gst_debug_category_get_threshold(category) ||
        level > gst_debug_category_get_threshold(category)) {
        return;
    }

    const gchar *msg = gst_debug_message_get(message);
    if (msg == NULL) return;

    int prio = ANDROID_LOG_DEBUG;
    if (level <= GST_LEVEL_ERROR) prio = ANDROID_LOG_ERROR;
    else if (level <= GST_LEVEL_WARNING) prio = ANDROID_LOG_WARN;
    else if (level <= GST_LEVEL_INFO) prio = ANDROID_LOG_INFO;

    __android_log_print(prio, "GstDebug", "[%s] %s",
                        gst_debug_category_get_name(category), msg);
}

/* ==============================================================================
 * GStreamer初始化函数
 * ============================================================================== */

/**
 * 初始化GStreamer引擎
 * 由GStreamerInitializer.java调用
 *
 * 注意：GStreamer Android库(gstreamer_android.so)由CMake构建系统自动链接，
 * 包含所有配置的插件。gst_init()会自动初始化GStreamer核心和插件。
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_internal_GStreamerInitializer_nativeInit(
    JNIEnv *env, jclass clazz, jobject context) {

    pthread_mutex_lock(&gst_init_lock);

    if (gst_initialized) {
        LOGI("GStreamer already initialized");
        pthread_mutex_unlock(&gst_init_lock);
        return;
    }

    LOGI("Initializing GStreamer engine...");

    /* Prefer GLES2 via EGL on Android/emulators; avoid desktop GL / ES3 mismatch
       that can cause "called unimplemented OpenGL ES API" and black screen.
       Do NOT set GST_GL_CONFIG here: a wrong format breaks glimagesink with
       "could not construct OpenGL config from GST_GL_CONFIG". */
    g_setenv("GST_GL_API", "gles2", FALSE);
    g_setenv("GST_GL_PLATFORM", "egl", FALSE);

    // 先注册静态插件，再初始化GStreamer
    gst_init_static_plugins();

    if (!gst_init_check(NULL, NULL, NULL)) {
        LOGE("Failed to initialize GStreamer");
        pthread_mutex_unlock(&gst_init_lock);
        return;
    }

    /* Default WARNING. Per-name LOG only where needed — heavy DEBUG
       categories (flvdemux/h264parse/openh264/decodebin/...) flood logcat
       and chatty expires LatencyPlayerCore startup lines. */
    gst_debug_set_default_threshold(GST_LEVEL_WARNING);
    gst_debug_add_log_function(gst_log_to_logcat, NULL, NULL);
    gst_debug_set_threshold_for_name("LatencyPlayerCore", GST_LEVEL_LOG);
    gst_debug_set_threshold_for_name("rtmpsrc", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("rtmp2src", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("typefind", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("flvdemux", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("playbin", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("uridecodebin", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("decodebin", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("autovideosink", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("glimagesink", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("glsinkbin", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("glcontext", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("glwindow", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("glfilter", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("glupload", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("openh264dec", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("h264parse", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("basesrc", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("queue2", GST_LEVEL_WARNING);
    gst_debug_set_threshold_for_name("multiqueue", GST_LEVEL_WARNING);

    /* adb shell setprop debug.latencyplayer.gst "3,soup*:5,flvdemux:5" */
    char gst_dbg[PROP_VALUE_MAX];
    if (__system_property_get("debug.latencyplayer.gst", gst_dbg) > 0 && gst_dbg[0] != '\0') {
        gst_debug_set_threshold_from_string(gst_dbg, TRUE);
        LOGI("GST debug overridden from property: %s", gst_dbg);
    }

    gst_initialized = TRUE;
    LOGI("GStreamer engine initialized successfully, version: %s", gst_version_string());

    pthread_mutex_unlock(&gst_init_lock);
}

/**
 * 获取GStreamer版本信息
 */
JNIEXPORT jstring JNICALL
Java_com_latencyplayer_sdk_internal_GStreamerInitializer_nativeGetVersion(
    JNIEnv *env, jclass clazz) {

    const gchar *version = gst_version_string();
    return (*env)->NewStringUTF(env, version);
}

/* ==============================================================================
 * LatencyPlayerManager JNI方法实现
 * ============================================================================== */

/**
 * 创建播放器实例
 * 返回native句柄
 */
JNIEXPORT jlong JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeCreate(
    JNIEnv *env, jobject thiz) {

    LOGI("Creating native player instance");

    // 分配播放器上下文
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)malloc(sizeof(LatencyPlayerContext));
    if (ctx == NULL) {
        LOGE("Failed to allocate memory for player context");
        return 0;
    }

    // 获取JavaVM
    JavaVM *java_vm = NULL;
    (*env)->GetJavaVM(env, &java_vm);
    if (java_vm == NULL) {
        LOGE("Failed to get JavaVM");
        free(ctx);
        return 0;
    }

    // 创建全局引用，防止被GC回收
    jobject global_obj = (*env)->NewGlobalRef(env, thiz);

    // 初始化播放器上下文
    int ret = latency_player_init(ctx, java_vm, global_obj);
    if (ret != 0) {
        LOGE("Failed to initialize player context");
        (*env)->DeleteGlobalRef(env, global_obj);
        free(ctx);
        return 0;
    }

    LOGI("Native player instance created: %p", ctx);
    return (jlong)(intptr_t)ctx;
}

/**
 * 设置Surface
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetSurface(
    JNIEnv *env, jobject thiz, jlong handle, jobject surface) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    ANativeWindow *window = NULL;
    if (surface != NULL) {
        window = ANativeWindow_fromSurface(env, surface);
    }

    latency_player_set_surface(ctx, window);
    /* latency_player_set_surface owns window (acquire/release); JNI ref from
       ANativeWindow_fromSurface is consumed there for the new-window path.
       For the same-window early-return path it releases the JNI ref itself. */
}

/**
 * 开始播放
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativePlay(
    JNIEnv *env, jobject thiz, jlong handle, jstring uri) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL || uri == NULL) return;

    const char *uri_str = (*env)->GetStringUTFChars(env, uri, NULL);
    if (uri_str == NULL) return;

    latency_player_play(ctx, uri_str);

    (*env)->ReleaseStringUTFChars(env, uri, uri_str);
}

/**
 * 停止播放
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeStop(
    JNIEnv *env, jobject thiz, jlong handle) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_stop(ctx);
}

/**
 * 暂停播放
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativePause(
    JNIEnv *env, jobject thiz, jlong handle) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_pause(ctx);
}

/**
 * 恢复播放
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeResume(
    JNIEnv *env, jobject thiz, jlong handle) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_resume(ctx);
}

/**
 * 设置静音
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetMute(
    JNIEnv *env, jobject thiz, jlong handle, jboolean mute) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_set_mute(ctx, mute ? TRUE : FALSE);
}

/**
 * 设置音量
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetVolume(
    JNIEnv *env, jobject thiz, jlong handle, jint volume) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    // Java层传入0-100，转换为0.0-1.0
    gdouble vol = (gdouble)volume / 100.0;
    latency_player_set_volume(ctx, vol);
}

/**
 * 设置缓冲时间
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetBufferTime(
    JNIEnv *env, jobject thiz, jlong handle, jint bufferTimeMs) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_set_buffer_time(ctx, bufferTimeMs);
}

/**
 * 设置低延迟模式
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetLowLatency(
    JNIEnv *env, jobject thiz, jlong handle, jboolean enable) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_set_low_latency(ctx, enable ? TRUE : FALSE);
}

/**
 * 设置播放方向
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetOrientation(
    JNIEnv *env, jobject thiz, jlong handle, jint orientation) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    // 根据方向设置旋转角度
    gint degrees = 0;
    switch (orientation) {
        case 1: degrees = 0; break;   // 竖屏
        case 2: degrees = 90; break;  // 横屏
        default: degrees = 0; break;
    }

    latency_player_set_rotation(ctx, degrees);
}

/**
 * 设置视频翻转
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetFlip(
    JNIEnv *env, jobject thiz, jlong handle, jboolean horizontal, jboolean vertical) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_set_flip(ctx, horizontal ? TRUE : FALSE, vertical ? TRUE : FALSE);
}

/**
 * 设置视频旋转角度
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSetRotation(
    JNIEnv *env, jobject thiz, jlong handle, jint degrees) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    latency_player_set_rotation(ctx, degrees);
}

/**
 * 保存截图
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeSaveSnapshot(
    JNIEnv *env, jobject thiz, jlong handle, jstring path) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL || path == NULL) return;

    const char *path_str = (*env)->GetStringUTFChars(env, path, NULL);
    if (path_str == NULL) return;

    latency_player_save_snapshot(ctx, path_str);

    (*env)->ReleaseStringUTFChars(env, path, path_str);
}

/**
 * 释放播放器资源
 */
JNIEXPORT void JNICALL
Java_com_latencyplayer_sdk_LatencyPlayerManager_nativeRelease(
    JNIEnv *env, jobject thiz, jlong handle) {

    LatencyPlayerContext *ctx = (LatencyPlayerContext *)(intptr_t)handle;
    if (ctx == NULL) return;

    LOGI("Releasing native player: %p", ctx);

    // 释放Java全局引用
    if (ctx->java_obj != NULL) {
        (*env)->DeleteGlobalRef(env, ctx->java_obj);
        ctx->java_obj = NULL;
    }

    // 释放播放器上下文
    latency_player_release(ctx);
    free(ctx);
}
