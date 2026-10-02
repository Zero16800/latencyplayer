# vendor/ — vendored GStreamer sources with LatencyPlayer patches

Files copied from the GStreamer **1.28** branch (gst-plugins-base
`playback` element and `gst-plugins-base/gst` internals), byte-identical
to the sources used to build the prebuilt static libraries in
`NDK-CMake-GStreamer` except for the patches below. They are compiled
into `liblatencyplayer.so` directly; because CMake places objects before
archives on the link line and links with `--allow-multiple-definition`,
these objects win over the copies inside `libgstplayback-1.0.a` /
`libgstcoreelements-1.0.a`.

## Files

| File | Upstream |
|------|----------|
| `gstdecodebin2.c` | gst-plugins-base `playback/gstdecodebin2.c` |
| `gstmultiqueue.c` / `gstmultiqueue.h` | gst-plugins-base `playback/gstmultiqueue.c` |
| `gstplaybackelements.h`, `gstplaybackutils.h`, `gstplay-enum.h`, `gstrawcaps.h` | gst-plugins-base `playback/` private headers |
| `gstcoreelementselements.h` | gst-plugins-base `coreelements` registration header |
| `include-shim/gst/glib-compat-private.h` | shim for a private header the prebuilt SDK does not install |

## Patches (all marked `LatencyPlayer patch` in the sources)

1. **Deferred-overrun grace in `gstdecodebin2.c`** — fixes intermittent
   RTMP playback with audio but no video. When a decodebin group overruns
   while it still has a single (audio-only) child chain:
   - raise the multiqueue limits (`1000` buffers / `4 MB` / `5 s`) on
     every overrun so the blocked `gst_data_queue_push()` in flvdemux's
     thread wakes up and can parse the pending video tag;
   - arm a 2500 ms grace timer (re-armed to 300 ms when a pad joins,
     absolute cap 5 s) instead of exposing the audio-only group;
   - when the late video pad joins the group, expose with both pads.
2. **`gstmultiqueue.c` diagnostics** — `check-full` reason line (only
   when TRUE) and `emitting overrun` dimensions, at `GST_INFO` so they
   show up with the default `LP_GST` threshold.

Build requirements (see `CMakeLists.txt`): `-DGETTEXT_PACKAGE="gstreamer-1.0"`
for `gstdecodebin2.c`, and `vendor/include-shim` on the include path for
`<gst/glib-compat-private.h>`.
