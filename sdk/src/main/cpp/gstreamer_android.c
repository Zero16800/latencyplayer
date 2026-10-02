/**
 * GStreamer Android static plugin registration.
 *
 * For static builds, each plugin must be declared and registered
 * before calling gst_init().
 *
 * RTMP + RTSP + HTTP-FLV + HLS (kept in sync with CMakeLists.txt PLUGIN_ALLOW).
 */
#include <gst/gst.h>
#include <android/log.h>

#define LOG_TAG "GstInit"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* Declare plugin registration functions */
GST_PLUGIN_STATIC_DECLARE(coreelements);
GST_PLUGIN_STATIC_DECLARE(playback);
GST_PLUGIN_STATIC_DECLARE(typefindfunctions);
GST_PLUGIN_STATIC_DECLARE(autodetect);
GST_PLUGIN_STATIC_DECLARE(videoconvertscale);
GST_PLUGIN_STATIC_DECLARE(audioconvert);
GST_PLUGIN_STATIC_DECLARE(audioresample);
GST_PLUGIN_STATIC_DECLARE(volume);
GST_PLUGIN_STATIC_DECLARE(rawparse);
GST_PLUGIN_STATIC_DECLARE(flv);
GST_PLUGIN_STATIC_DECLARE(rtmp);
GST_PLUGIN_STATIC_DECLARE(rtmp2);
GST_PLUGIN_STATIC_DECLARE(androidmedia);
GST_PLUGIN_STATIC_DECLARE(openh264);
GST_PLUGIN_STATIC_DECLARE(app);
GST_PLUGIN_STATIC_DECLARE(pbtypes);
GST_PLUGIN_STATIC_DECLARE(videoparsersbad);
GST_PLUGIN_STATIC_DECLARE(audioparsers);
GST_PLUGIN_STATIC_DECLARE(rtsp);
GST_PLUGIN_STATIC_DECLARE(rtp);
GST_PLUGIN_STATIC_DECLARE(rtpmanager);
GST_PLUGIN_STATIC_DECLARE(udp);
GST_PLUGIN_STATIC_DECLARE(soup);
GST_PLUGIN_STATIC_DECLARE(hls);
GST_PLUGIN_STATIC_DECLARE(mpegtsdemux);
GST_PLUGIN_STATIC_DECLARE(isomp4);
GST_PLUGIN_STATIC_DECLARE(aes);
GST_PLUGIN_STATIC_DECLARE(mulaw);
GST_PLUGIN_STATIC_DECLARE(alaw);
GST_PLUGIN_STATIC_DECLARE(mpg123);
GST_PLUGIN_STATIC_DECLARE(libav);
GST_PLUGIN_STATIC_DECLARE(opensles);
/* snapshot file encode (pngenc / jpegenc) */
GST_PLUGIN_STATIC_DECLARE(png);
GST_PLUGIN_STATIC_DECLARE(jpeg);

/* Provide stderr/stdout/stdin symbols for GStreamer static libs */
#include <stdio.h>
#undef stderr
#undef stdout
#undef stdin
FILE *stderr = &__sF[2];
FILE *stdout = &__sF[1];
FILE *stdin  = &__sF[0];

/* __gnu_strerror_r: real implementation lives in gstreamer_stubs.c */

void gst_init_static_plugins(void)
{
    LOGI("Registering GStreamer static plugins...");

    GST_PLUGIN_STATIC_REGISTER(coreelements);
    GST_PLUGIN_STATIC_REGISTER(playback);
    GST_PLUGIN_STATIC_REGISTER(typefindfunctions);
    GST_PLUGIN_STATIC_REGISTER(autodetect);
    GST_PLUGIN_STATIC_REGISTER(videoconvertscale);
    GST_PLUGIN_STATIC_REGISTER(audioconvert);
    GST_PLUGIN_STATIC_REGISTER(audioresample);
    GST_PLUGIN_STATIC_REGISTER(volume);
    GST_PLUGIN_STATIC_REGISTER(rawparse);
    GST_PLUGIN_STATIC_REGISTER(flv);
    GST_PLUGIN_STATIC_REGISTER(rtmp);
    GST_PLUGIN_STATIC_REGISTER(rtmp2);
    GST_PLUGIN_STATIC_REGISTER(androidmedia);
    GST_PLUGIN_STATIC_REGISTER(openh264);
    GST_PLUGIN_STATIC_REGISTER(app);
    GST_PLUGIN_STATIC_REGISTER(pbtypes);
    GST_PLUGIN_STATIC_REGISTER(videoparsersbad);
    GST_PLUGIN_STATIC_REGISTER(audioparsers);
    GST_PLUGIN_STATIC_REGISTER(rtsp);
    GST_PLUGIN_STATIC_REGISTER(rtp);
    GST_PLUGIN_STATIC_REGISTER(rtpmanager);
    GST_PLUGIN_STATIC_REGISTER(udp);
    GST_PLUGIN_STATIC_REGISTER(soup);
    GST_PLUGIN_STATIC_REGISTER(hls);
    GST_PLUGIN_STATIC_REGISTER(mpegtsdemux);
    GST_PLUGIN_STATIC_REGISTER(isomp4);
    GST_PLUGIN_STATIC_REGISTER(aes);
    GST_PLUGIN_STATIC_REGISTER(mulaw);
    GST_PLUGIN_STATIC_REGISTER(alaw);
    GST_PLUGIN_STATIC_REGISTER(mpg123);
    GST_PLUGIN_STATIC_REGISTER(libav);
    GST_PLUGIN_STATIC_REGISTER(opensles);
    GST_PLUGIN_STATIC_REGISTER(png);
    GST_PLUGIN_STATIC_REGISTER(jpeg);

    /* Prefer librtmp-based rtmpsrc over rtmp2src (better server compatibility;
       rtmp2src hangs on some servers waiting for S0+S1+S2). */
    GstElementFactory *f;
    f = gst_element_factory_find("rtmpsrc");
    if (f) {
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(f), GST_RANK_PRIMARY);
        gst_object_unref(f);
    }
    f = gst_element_factory_find("rtmp2src");
    if (f) {
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(f), GST_RANK_NONE);
        gst_object_unref(f);
    }

    /* libgstlibav is linked only for AAC (avdec_aac), but it also registers
       avdec_h264 which outranks openh264dec and produced decode errors on
       HLS/TS streams. Keep openh264 as the primary H.264 decoder; avdec_h264
       stays as a lower-ranked fallback (e.g. unsupported profiles). */
    f = gst_element_factory_find("openh264dec");
    if (f) {
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(f), GST_RANK_PRIMARY + 1);
        gst_object_unref(f);
    }
    f = gst_element_factory_find("avdec_h264");
    if (f) {
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(f), GST_RANK_MARGINAL);
        gst_object_unref(f);
    }

    LOGI("All static plugins registered");
}
