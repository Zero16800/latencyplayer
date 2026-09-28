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
)

add_library(latencyplayer SHARED ${LATENCY_PLAYER_SOURCES})

target_include_directories(latencyplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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
)

set(CORE_LIST "")
foreach(lib ${GST_LIBS})
    gst_lib_stem("${lib}" _stem)
    if(_stem IN_LIST CORE_ALLOW)
        list(APPEND CORE_LIST ${lib})
    endif()
endforeach()

# RTMP-only playback plugins (whole-archive for static registration)
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
    android log c m dl EGL GLESv2 z
    # openh264.a is C++; target is built as C so the C driver does not
    # auto-link the STL — add both archives explicitly.
    c++_static c++abi
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
