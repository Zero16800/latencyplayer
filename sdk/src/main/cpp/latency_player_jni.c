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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

#include <gio/gio.h>

#include "latency_player_core.h"

/* Static plugin registration (defined in gstreamer_android.c) */
extern void gst_init_static_plugins(void);

/* Defined in glib-networking's openssl backend (libgioopenssl.a), which has
   no installed header. module=NULL selects the static-registration path. */
extern void g_tls_backend_openssl_register(void *module);

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

/* GLib/GIO logs normally go to fd 2, which Android discards, so criticals
   never show up in logcat. Route them here instead. Also abort on the
   g_error_new_literal(NULL-message) critical: it is immediately followed by
   g_task_return_error(NULL), which silently swallows the task result and
   leaves the GTask pending forever (souphttpsrc read never completes). */
static GLogWriterOutput
glib_log_to_logcat(GLogLevelFlags log_level,
                   const GLogField *fields,
                   gsize n_fields,
                   gpointer user_data) {
    const gchar *domain = NULL;
    const gchar *msg = NULL;

    for (gsize i = 0; i < n_fields; i++) {
        if (g_strcmp0(fields[i].key, "GLIB_DOMAIN") == 0)
            domain = (const gchar *)fields[i].value;
        else if (g_strcmp0(fields[i].key, "MESSAGE") == 0)
            msg = (const gchar *)fields[i].value;
    }

    if (msg == NULL)
        msg = "(no message)";
    if (domain == NULL)
        domain = "GLib";

    /* Match GLib's default handler: debug/info are opt-in via G_MESSAGES_DEBUG,
       otherwise GIO's per-packet chatter floods logcat. Warnings and above
       are what we actually need to see. */
    if (log_level & (G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO))
        return G_LOG_WRITER_HANDLED;

    int prio = ANDROID_LOG_DEBUG;
    if (log_level & G_LOG_LEVEL_ERROR) prio = ANDROID_LOG_FATAL;
    else if (log_level & G_LOG_LEVEL_CRITICAL) prio = ANDROID_LOG_ERROR;
    else if (log_level & G_LOG_LEVEL_WARNING) prio = ANDROID_LOG_WARN;

    __android_log_print(prio, "GLib", "%s: %s", domain, msg);

    /* A NULL message here means g_set_error_literal() will fail its
       message != NULL assertion and the caller's GTask will never be
       returned (souphttpsrc read hangs). Root cause is fixed in
       __gnu_strerror_r; log loudly if it ever recurs. */
    if ((log_level & G_LOG_LEVEL_CRITICAL) &&
        strstr(msg, "g_error_new_literal") != NULL) {
        LOGE("g_error_new_literal got NULL message -- GTask will hang: %s",
             msg);
    }

    return G_LOG_WRITER_HANDLED;
}

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
 * HTTPS信任锚 (SSL_CERT_FILE)
 * ============================================================================== */

/* OpenSSL's compiled-in OPENSSLDIR points at the cerbero build host
   (/home/nirbheek/.../ssl), which does not exist on Android. GLib's openssl
   backend builds its GTlsDatabase with X509_STORE_set_default_paths(), so it
   loads zero roots and every https:// handshake is rejected with
   G_TLS_CERTIFICATE_UNKNOWN_CA ("Unacceptable TLS certificate").
   Concatenate the Android system CA store into one PEM bundle in the app
   cache dir and point SSL_CERT_FILE at it. Must run before the first TLS
   handshake (i.e. before any play() on an https URI). */
static void append_certs_from_dir(const char *dir_path, FILE *out, int *count) {
    DIR *d = opendir(dir_path);
    struct dirent *ent;

    if (d == NULL)
        return;

    while ((ent = readdir(d)) != NULL) {
        char path[512];
        FILE *in;
        long size;
        char *buf;
        size_t got;

        if (ent->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
        in = fopen(path, "rb");
        if (in == NULL)
            continue;

        if (fseek(in, 0, SEEK_END) != 0 || (size = ftell(in)) <= 0 || size > (1 << 20)) {
            fclose(in);
            continue;
        }
        rewind(in);

        buf = malloc((size_t)size + 1);
        if (buf == NULL) {
            fclose(in);
            continue;
        }
        got = fread(buf, 1, (size_t)size, in);
        fclose(in);
        buf[got] = '\0';

        if (strstr(buf, "-----BEGIN CERTIFICATE-----") != NULL) {
            fwrite(buf, 1, got, out);
            if (got == 0 || buf[got - 1] != '\n')
                fputc('\n', out);
            (*count)++;
        }
        free(buf);
    }
    closedir(d);
}

static void setup_ca_bundle(JNIEnv *env, jobject context) {
    static const char *const ca_dirs[] = {
        "/apex/com.android.conscrypt/cacerts", /* Android 14+ */
        "/system/etc/security/cacerts",        /* Android <= 13 */
        NULL
    };
    jclass ctx_cls, file_cls;
    jmethodID get_cache_dir, get_abs_path;
    jobject cache_dir_obj;
    jstring jpath;
    const char *cache_dir;
    char bundle_path[640];
    FILE *out;
    int count = 0;
    int i;

    if (g_getenv("SSL_CERT_FILE") != NULL) {
        LOGI("SSL_CERT_FILE already set: %s", g_getenv("SSL_CERT_FILE"));
        return;
    }

    ctx_cls = (*env)->GetObjectClass(env, context);
    get_cache_dir = (*env)->GetMethodID(env, ctx_cls, "getCacheDir", "()Ljava/io/File;");
    if (get_cache_dir == NULL) {
        (*env)->ExceptionClear(env);
        LOGE("CA bundle: getCacheDir not found");
        return;
    }
    cache_dir_obj = (*env)->CallObjectMethod(env, context, get_cache_dir);
    if ((*env)->ExceptionCheck(env) || cache_dir_obj == NULL) {
        (*env)->ExceptionClear(env);
        LOGE("CA bundle: getCacheDir failed");
        return;
    }

    file_cls = (*env)->GetObjectClass(env, cache_dir_obj);
    get_abs_path = (*env)->GetMethodID(env, file_cls, "getAbsolutePath", "()Ljava/lang/String;");
    if (get_abs_path == NULL) {
        (*env)->ExceptionClear(env);
        LOGE("CA bundle: getAbsolutePath not found");
        return;
    }
    jpath = (*env)->CallObjectMethod(env, cache_dir_obj, get_abs_path);
    if ((*env)->ExceptionCheck(env) || jpath == NULL) {
        (*env)->ExceptionClear(env);
        LOGE("CA bundle: getAbsolutePath failed");
        return;
    }
    cache_dir = (*env)->GetStringUTFChars(env, jpath, NULL);
    if (cache_dir == NULL)
        return;

    snprintf(bundle_path, sizeof(bundle_path), "%s/latencyplayer-ca.pem", cache_dir);
    (*env)->ReleaseStringUTFChars(env, jpath, cache_dir);

    out = fopen(bundle_path, "wb");
    if (out == NULL) {
        LOGE("CA bundle: cannot write %s", bundle_path);
        return;
    }
    for (i = 0; ca_dirs[i] != NULL; i++)
        append_certs_from_dir(ca_dirs[i], out, &count);
    fclose(out);

    if (count == 0) {
        remove(bundle_path);
        LOGE("CA bundle: no system CAs found, https will fail verification");
        return;
    }

    g_setenv("SSL_CERT_FILE", bundle_path, FALSE);
    g_setenv("SSL_CERT_DIR", "/system/etc/security/cacerts", FALSE);
    LOGI("CA bundle ready: %s (%d certificates)", bundle_path, count);
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

    /* Must be set before the first g_log* call. */
    g_log_set_writer_func(glib_log_to_logcat, NULL, NULL);

    /* Point OpenSSL at the Android system CA store before any TLS handshake
       (SSL_CERT_FILE is read lazily by X509_STORE_set_default_paths). */
    setup_ca_bundle(env, context);

    /* Static builds never scan lib/gio/modules, so no GTlsBackend is ever
       registered and souphttpsrc rejects https:// with
       "TLS support is not available". Register the OpenSSL backend by hand
       (GIOModule* = NULL => static registration path). */
    g_tls_backend_openssl_register(NULL);
    {
        /* g_tls_backend_get_default() is (transfer none): GLib owns the
           singleton and caches a bare pointer. Unref'ing it here frees the
           object while that cache still points at it, so the next
           g_tls_backend_get_default() (from libsoup) is a use-after-free.
           Instantiate it eagerly, but never release it. */
        GTlsBackend *tlsb = g_tls_backend_get_default();
        LOGI("GIO TLS backend available: %d", tlsb != NULL ? 1 : 0);
    }

    /* Prefer GLES2 via EGL on Android/emulators; avoid desktop GL / ES3 mismatch
       that can cause "called unimplemented OpenGL ES API" and black screen.
       Do NOT set GST_GL_CONFIG here: a wrong format breaks glimagesink with
       "could not construct OpenGL config from GST_GL_CONFIG". */
    g_setenv("GST_GL_API", "gles2", FALSE);
    g_setenv("GST_GL_PLATFORM", "egl", FALSE);

    /* Do NOT call gst_init_static_plugins() here. gst_init_check() invokes it
       itself once _gst_plugin_inited is set; calling it before init registers
       nothing (30 assertion failures) and calling it after init registers
       every plugin a second time ("cannot register existing type") and
       crashes the queue2 source thread. */
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
