# librtmp (vendored) — provenance & patches

This directory contains the `librtmp` sources used by the iOS LatencyPlayer SDK.

* Upstream: https://github.com/mstorsjo/rtmpdump  (mirror of the original
  `rtmpdump` / RTMPDump project)
* License: LGPL-2.1 (see `COPYING` in this directory)
* Vendored files: `rtmp.c`, `rtmp.h`, `amf.c`, `amf.h`, `parseurl.c`,
  `log.c`, `log.h`, `rtmp_sys.h`, `handshake.h`, `bytes.h`, `dh.h`,
  `dhgroups.h`.

Not vendored on purpose: `hashswf.c` / `http.h` (only reachable through
`RTMP_HashSWF`, which is compiled out unless `CRYPTO` is defined — this SDK
pulls plain live RTMP and never performs SWF verification).

## How it is built

The SDK compiles librtmp **without** `CRYPTO`, `USE_POLARSSL` or
`USE_GNUTLS`, and **with** `USE_COMMONCRYPTO`. This:

* drops every OpenSSL / PolarSSL / GnuTLS dependency, and
* uses Apple's CommonCrypto for the HMAC-SHA256 handshake digests.

## Patches applied to upstream

### 1. `handshake.h` — CommonCrypto backend (`USE_COMMONCRYPTO`)

Added a new `#elif defined(USE_COMMONCRYPTO)` branch (between the GnuTLS and
OpenSSL branches) that maps librtmp's crypto API onto CommonCrypto:

* `HMAC_CTX` / `HMAC_setup` / `HMAC_crunch` / `HMAC_finish` are emulated on top
  of the one-shot `CCHmac(kCCHmacAlgSHA256, …)`. Because `CCHmac` is one-shot,
  the (tiny, ≤ 1536-byte) handshake message is buffered until `HMAC_finish`.
* The `RC4_*` macros are compiled as no-ops. RC4 is used by librtmp **only**
  for RTMPE (encrypted) sessions, which this SDK never negotiates.

To regenerate the diff against upstream:

```
git clone https://github.com/mstorsjo/rtmpdump /tmp/rtmpdump
diff -u /tmp/rtmpdump/librtmp/handshake.h handshake.h
```
