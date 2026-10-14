cmake_minimum_required(VERSION 3.22.1)
project(LatencyPlayer LANGUAGES C CXX)

if(NOT DEFINED GSTREAMER_ROOT_ANDROID)
    if(DEFINED ENV{GSTREAMER_ROOT_ANDROID})
        set(GSTREAMER_ROOT_ANDROID $ENV{GSTREAMER_ROOT_ANDROID})
    else()
        message(FATAL_ERROR "GSTREAMER_ROOT_ANDROID is not set!")
    endif()
endif()

set(GST_PREFIX ${GSTREAMER_ROOT_ANDROID}/${ANDROID_ABI})
set(GST_LIB ${GST_PREFIX}/lib)
set(GST_PLUGIN_LIB ${GST_LIB}/gstreamer-1.0)
set(GST_INCLUDE ${GST_PREFIX}/include)

set(LATENCY_PLAYER_SOURCES
    latency_player_jni.c
    latency_player_core.c
    gstreamer_android.c
    gstreamer_stubs.c
    # LatencyPlayer patch: deferred-overrun grace in decodebin so flvdemux's
    # late video pad is not rejected after a multiqueue overrun expose.
    # Compiled as a regular object so its gst_decode_bin_* symbols take
    # precedence over the copy inside libgstplayback-1.0.a (linker sees
    # objects before archives; --allow-multiple-definition tolerates both).
    vendor/gstdecodebin2.c
    # LatencyPlayer patch: check-full/overrun diagnostics (GST_DEBUG only).
    # Same object-wins mechanism vs libgstcoreelements-1.0.a.
    vendor/gstmultiqueue.c
    # LatencyPlayer patch: rtmpsrc unlock() must not call RTMP_Close() --
    # librtmp frees r->m_vec while the streaming thread may still be in
    # RTMP_ReadPacket (SIGSEGV fault 0x38 on stop/switch; seen with
    # com.woned.daylong on 1.0.6). unlock() now only shutdown()s the
    # socket; stop() does the full Close after the task has joined.
    # Same object-wins mechanism vs libgstrtmp-1.0.a (whole-archive +
    # --allow-multiple-definition; objects precede archives so ours wins).
    vendor/rtmp/gstrtmpsrc.c
)

add_library(latencyplayer SHARED ${LATENCY_PLAYER_SOURCES})

target_compile_definitions(latencyplayer PRIVATE
    # vendor/gstdecodebin2.c includes <glib/gi18n-lib.h> which hard-errors
    # without GETTEXT_PACKAGE. Only *_(); gettext macro — no translation
    # catalog is actually installed.
    GETTEXT_PACKAGE=\"gstreamer-1.0\"
)

# vendor/gstmultiqueue.c includes <gst/glib-compat-private.h>, a private
# header that the prebuilt SDK does not install. Shim it from vendor/.
target_include_directories(latencyplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/include-shim
)

target_include_directories(latencyplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GST_INCLUDE}
    ${GST_INCLUDE}/gstreamer-1.0
    ${GST_INCLUDE}/gstreamer-1.0/gst
    ${GST_INCLUDE}/glib-2.0
    ${GST_LIB}/glib-2.0/include
)

# Strip only final ".a" (NAME_WE would turn libgstreamer-1.0.a into libgstreamer-1)
function(gst_lib_stem lib out_var)
    get_filename_component(_n "${lib}" NAME)
    string(REGEX REPLACE "\\.a$" "" _n "${_n}")
    set(${out_var} "${_n}" PARENT_SCOPE)
endfunction()

file(GLOB GST_LIBS ${GST_LIB}/lib*.a)
file(GLOB GST_PLUGIN_LIBS ${GST_PLUGIN_LIB}/lib*.a)

# Core/static deps: normal archive link (pull used objs only) — must come AFTER objects
set(CORE_ALLOW
    libgstreamer-1.0
    libgstbase-1.0
    libgstvideo-1.0
    libgstaudio-1.0
    libgstapp-1.0
    libgstpbutils-1.0
    libgsttag-1.0
    libgstcodecparsers-1.0
    libgstallocators-1.0
    libgstcontroller-1.0
    libgstnet-1.0
    libgstgl-1.0
    libgstphotography-1.0
    libgobject-2.0
    libglib-2.0
    libgmodule-2.0
    libgio-2.0
    libgthread-2.0
    libffi
    libpcre2-8
    libintl
    libiconv
    liborc-0.4
    librtmp
    libcrypto
    libssl
    libopenh264
    libz
    # RTSP client stack (rtspsrc -> libgstrtsp/sdp)
    libgstrtsp-1.0
    libgstsdp-1.0
    # HTTP source (souphttpsrc)
    libsoup-3.0
    libpsl
    libnghttp2
    # HLS/TS demux
    libgstrtp-1.0
    libgstriff-1.0
    libgsturidownloader-1.0
    libgstadaptivedemux-1.0
    libgsttag-1.0
    libgstmpegts-1.0
    libxml2
    # audio decode paths (FLV/TS/RTP)
    libmpg123
    # AAC decode (libgstlibav -> avdec_aac)
    libavcodec
    libavutil
    libswresample
    libavformat
    libavfilter
    libswscale
    libbz2
    # snapshot encode (pngenc/jpegenc)
    libpng16
    libjpeg
)

set(CORE_LIST "")
foreach(lib ${GST_LIBS})
    gst_lib_stem("${lib}" _stem)
    if(_stem IN_LIST CORE_ALLOW)
        list(APPEND CORE_LIST ${lib})
    endif()
endforeach()

# glib-networking TLS backend (openssl) lives under lib/gio/modules/ and is
# therefore missed by the ${GST_LIB}/lib*.a glob. Without it GIO has no
# GTlsBackend at all and every https:// request fails with
# "TLS support is not available".
set(GIO_OPENSSL_BACKEND ${GST_LIB}/gio/modules/libgioopenssl.a)
if(EXISTS ${GIO_OPENSSL_BACKEND})
    list(APPEND CORE_LIST ${GIO_OPENSSL_BACKEND})
    message(STATUS "GIO TLS backend: ${GIO_OPENSSL_BACKEND}")
else()
    message(WARNING "GIO TLS backend NOT found: ${GIO_OPENSSL_BACKEND} - https:// will fail")
endif()

# Playback plugins (whole-archive for static registration).
# RTMP + RTSP + HTTP-FLV + HLS.
set(PLUGIN_ALLOW
    libgstcoreelements
    libgstplayback
    libgsttypefindfunctions
    libgstautodetect
    libgstvideoconvertscale
    libgstaudioconvert
    libgstaudioresample
    libgstvolume
    libgstrawparse
    libgstflv
    libgstrtmp
    libgstrtmp2
    libgstandroidmedia
    libgstopenh264
    libgstapp
    libgstpbtypes
    libgstvideoparsersbad
    libgstaudioparsers
    # RTSP: rtspsrc + RTP depayload/jitterbuffer
    libgstrtsp
    libgstrtp
    libgstrtpmanager
    # udpsrc/udpsink for RTSP RTP transports (rtspsrc "failed to allocate udp ports" without it)
    libgstudp
    # HTTP(S) source
    libgstsoup
    # HLS
    libgsthls
    libgstmpegtsdemux
    libgstisomp4
    libgstaes
    # audio: G.711 (RTSP), MP3 (FLV/TS), AAC (FLV/RTMP)
    libgstmulaw
    libgstalaw
    libgstmpg123
    libgstlibav
    # audio output (playbin autoaudiotestsrc -> openslessink)
    libgstopensles
    # snapshot file encode (pngenc / jpegenc)
    libgstpng
    libgstjpeg
)

set(PLUGIN_LIST "")
foreach(lib ${GST_PLUGIN_LIBS})
    gst_lib_stem("${lib}" _stem)
    if(_stem IN_LIST PLUGIN_ALLOW)
        list(APPEND PLUGIN_LIST ${lib})
    endif()
endforeach()

message(STATUS "Core libs:")
foreach(lib ${CORE_LIST})
    message(STATUS "  ${lib}")
endforeach()
message(STATUS "Plugin libs:")
foreach(lib ${PLUGIN_LIST})
    message(STATUS "  ${lib}")
endforeach()

# Libraries must be on target_link_libraries so CMake places them AFTER objects.
# target_link_options/@rsp lands before .o files and archives never resolve.
target_link_libraries(latencyplayer PRIVATE
    "-Wl,--allow-multiple-definition"
    "-Wl,--whole-archive"
    ${PLUGIN_LIST}
    ${PLUGIN_LIST}
    "-Wl,--no-whole-archive"
    "-Wl,--start-group"
    ${CORE_LIST}
    ${CORE_LIST}
    "-Wl,--end-group"
    android log c m dl EGL GLESv2 z OpenSLES
    # openh264.a is C++; target is built as C so the C driver does not
    # auto-link the STL — add both archives explicitly.
    c++_static c++abi
    # FFmpeg aarch64/arm asm references ff_tx_tab_* tables with non-GOT
    # ADRP/ADD relocations; localizing them makes those relocs valid in
    # a shared library (they are internal to libavutil anyway).
    "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/ff_localize.map"
    "-Wl,--gc-sections"
    "-Wl,--no-undefined"
)

# 16 KB page-size support (Android 15+). NDK r25 has no
# ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES (r27+), so set the linker flags
# directly for 64-bit ABIs (16 KB applies to arm64/x86_64 only).
if(ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "x86_64")
    target_link_libraries(latencyplayer PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384"
    )
endif()

target_compile_options(latencyplayer PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
    -O2
    -ffunction-sections -fdata-sections
)
