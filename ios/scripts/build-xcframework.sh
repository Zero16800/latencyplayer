#!/usr/bin/env bash
#
# Builds LatencyPlayer.xcframework (device + simulator slices) from the
# XcodeGen project.
#
# Requirements (macOS only):
#   * Xcode 14+
#   * xcodegen  (brew install xcodegen)
#
# Usage:
#   ./scripts/build-xcframework.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v xcodegen >/dev/null 2>&1; then
    echo "error: xcodegen not found. Install with: brew install xcodegen" >&2
    exit 1
fi

echo "==> Generating Xcode project"
xcodegen generate --spec project.yml

OUT="$ROOT/build"
rm -rf "$OUT/LatencyPlayer.xcframework"
mkdir -p "$OUT"

archive() {
    local destination="$1"
    local name="$2"
    echo "==> Archiving $name ($destination)"
    xcodebuild archive \
        -project LatencyPlayer.xcodeproj \
        -scheme LatencyPlayer \
        -configuration Release \
        -destination "generic/platform=$destination" \
        -archivePath "$OUT/$name.xcarchive" \
        SKIP_INSTALL=NO \
        BUILD_LIBRARY_FOR_DISTRIBUTION=YES \
        | xcbeautify 2>/dev/null || return $?
}

archive iOS LatencyPlayer-iOS
archive iOSSimulator LatencyPlayer-iOSSim

DEVICE_FW="$OUT/LatencyPlayer-iOS.xcarchive/Products/Library/Frameworks/LatencyPlayer.framework"
SIM_FW="$OUT/LatencyPlayer-iOSSim.xcarchive/Products/Library/Frameworks/LatencyPlayer.framework"

echo "==> Creating XCFramework"
xcodebuild -create-xcframework \
    -framework "$DEVICE_FW" \
    -framework "$SIM_FW" \
    -output "$OUT/LatencyPlayer.xcframework"

echo "==> Done: $OUT/LatencyPlayer.xcframework"
