# vendor/ — vendored GStreamer sources with LatencyPlayer patches

Files copied from the GStreamer **1.28** sources (gst-plugins-base
`playback` element, `gst-plugins-base/gst` internals, and gst-plugins-bad
`ext/rtmp` pinned to **1.28.7**), byte-identical
to the sources used to build the prebuilt static libraries in
`NDK-CMake-GStreamer` except for the patches below. They are compiled
into `liblatencyplayer.so` directly; because CMake places objects before
archives on the link line and links with `--allow-multiple-definition`,
these objects win over the copies inside `libgstplayback-1.0.a` /
`libgstcoreelements-1.0.a` / `libgstrtmp-1.0.a`.

## Files

| File | Upstream |
|------|----------|
| `gstdecodebin2.c` | gst-plugins-base `playback/gstdecodebin2.c` |
| `gstmultiqueue.c` / `gstmultiqueue.h` | gst-plugins-base `playback/gstmultiqueue.c` |
| `gstplaybackelements.h`, `gstplaybackutils.h`, `gstplay-enum.h`, `gstrawcaps.h` | gst-plugins-base `playback/` private headers |
| `gstcoreelementselements.h` | gst-plugins-base `coreelements` registration header |
| `rtmp/gstrtmpsrc.c`, `rtmp/gstrtmpsrc.h`, `rtmp/gstrtmpelements.h` | gst-plugins-bad `ext/rtmp/` (1.28.7) |
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
3. **`gstrtmpsrc.c` stop/switch SIGSEGV (fault `0x38`)** — upstream
   `unlock()` called `RTMP_Close()`, whose `CloseInternal()` `free()`s
   and NULLs `r->m_vec` while the streaming thread may still be inside
   `RTMP_ReadPacket` (`r->m_vec[channel]`, channel=7 → 0x38 → null
   deref; observed on device with com.woned.daylong, 1.0.6, two crashes
   in 3 minutes). Windows: stopping/switching a stream mid-packet-read.
   Patch:
   - `unlock()` only `shutdown()`s the socket — wakes a blocked `recv()`
     without freeing anything the reader still uses; fd stays open (no
     fd-reuse window), so `stop()` owns the full teardown;
   - `stop()` (called by GstBaseSrc only after the streaming task has
     joined) does `RTMP_Close()` + `RTMP_Free()` — zero leak;
   - `start()` error path also `RTMP_Close()`s (fixes an upstream leak;
     `RTMP_Alloc()` is `calloc()`, so Close on a fresh context is safe).
   Trade-off: a flush-seek no longer auto-reconnects (upstream relied on
   the socket being closed in `unlock()`); irrelevant here because the
   SDK always plays RTMP with `live=1` (seekable=FALSE). Paired with
   `signal(SIGPIPE, SIG_IGN)` in `latency_player_jni.c` init: `stop()`'s
   `SendDeleteStream` write to the already-shutdown socket returns EPIPE
   instead of killing the process.

Build requirements (see `CMakeLists.txt`): `-DGETTEXT_PACKAGE="gstreamer-1.0"`
for `gstdecodebin2.c`, and `vendor/include-shim` on the include path for
`<gst/glib-compat-private.h>`.
