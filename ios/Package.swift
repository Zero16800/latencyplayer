// swift-tools-version:5.7
//
// LatencyPlayer iOS SDK
// ----------------------
// Source-based Swift Package used for development and to build the
// distributable LatencyPlayer.xcframework (see scripts/build-xcframework.sh).
//
// Products:
//   * LatencyPlayer          — Objective-C static library
// Dependencies:
//   * CLibrtmp               — vendored librtmp (LGPL-2.1), built with the
//                              CommonCrypto backend (no OpenSSL).
//
// NOTE: Objective-C sources require ARC, which SwiftPM can only request via
// `unsafeFlags(["-fobjc-arc"])`. Packages that use unsafeFlags cannot be
// consumed directly as a remote SPM dependency; consumers should depend on the
// prebuilt `LatencyPlayer.xcframework` instead (see README).

import PackageDescription

let package = Package(
    name: "LatencyPlayer",
    platforms: [
        .iOS(.v12)
    ],
    products: [
        .library(
            name: "LatencyPlayer",
            targets: ["LatencyPlayer"]
        )
    ],
    targets: [
        .target(
            name: "CLibrtmp",
            path: "third_party/librtmp",
            publicHeadersPath: ".",
            cSettings: [
                .define("USE_COMMONCRYPTO"),
                .headerSearchPath(".")
            ]
        ),
        .target(
            name: "LatencyPlayer",
            dependencies: ["CLibrtmp"],
            path: "Sources/LatencyPlayer",
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("include"),
                .headerSearchPath("Internal"),
                .unsafeFlags(["-fobjc-arc"])
            ],
            linkerSettings: [
                .linkedFramework("VideoToolbox"),
                .linkedFramework("CoreMedia"),
                .linkedFramework("CoreVideo"),
                .linkedFramework("AVFoundation"),
                .linkedFramework("CoreFoundation"),
                .linkedFramework("Security")
            ]
        )
    ]
)
