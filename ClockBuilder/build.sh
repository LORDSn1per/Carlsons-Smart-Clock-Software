#!/bin/bash
# Builds "Clock Builder.app".
#
# Deliberately no Xcode project: this Mac has only the Command Line Tools, and
# swiftc plus a hand-written Info.plist is enough for a single-window SwiftUI
# app. Keeping it a shell script also means the build works from a terminal on
# the NAS mount, the same way the firmware does.
#
# Version handling mirrors scripts/merge_firmware.py: VERSION holds the number
# this build will stamp, and it is bumped by 0.01 afterwards for the next one.

set -euo pipefail
cd "$(dirname "$0")"

VERSION="$(cat VERSION)"

# The version is part of the app's name, so each build lands as its own bundle
# rather than replacing the last one - the same "keep the history" idea as the
# BIN folder full of SmartClock_vX.XX.bin files.
APP_NAME="Clock Builder ${VERSION}"
BUNDLE_ID="com.carlson.clockbuilder"
DEST_DIR="${1:-/Volumes/home/Documents/Arduino/SmartClock/Software/Clock Builder}"
# Build on local disk, not on the NAS. An AFP share stores Mac metadata in
# sidecar files, and codesign refuses any bundle carrying them ("resource fork,
# Finder information, or similar detritus not allowed") no matter how often
# xattr -cr is run, because the share re-adds them. Signing locally and copying
# the finished bundle across sidesteps it entirely.
BUILD_DIR="${TMPDIR:-/tmp}/clockbuilder-build"
APP_DIR="${BUILD_DIR}/${APP_NAME}.app"

echo "Building ${APP_NAME}"

rm -rf "${APP_DIR}"
mkdir -p "${APP_DIR}/Contents/MacOS" "${APP_DIR}/Contents/Resources"

# -parse-as-library: without it a lone Swift file is treated as a script and
# @main is rejected ("cannot be used in a module that contains top-level code").
swiftc \
  -parse-as-library \
  -O \
  -target x86_64-apple-macosx12.0 \
  -o "${APP_DIR}/Contents/MacOS/ClockBuilder" \
  Sources/*.swift

cat > "${APP_DIR}/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key><string>${APP_NAME}</string>
    <key>CFBundleExecutable</key><string>ClockBuilder</string>
    <key>CFBundleIdentifier</key><string>${BUNDLE_ID}</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>${VERSION}</string>
    <key>CFBundleVersion</key><string>${VERSION}</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>LSApplicationCategoryType</key><string>public.app-category.developer-tools</string>
</dict>
</plist>
PLIST

# Ad-hoc signature, applied on local disk. Not notarised - the app never leaves
# this Mac - but signing stops Gatekeeper refusing to launch it outright.
codesign --force --deep --sign - "${APP_DIR}" \
  || echo "  (codesign failed; the app still runs locally)"

mkdir -p "${DEST_DIR}"
rm -rf "${DEST_DIR}/${APP_NAME}.app"
cp -R "${APP_DIR}" "${DEST_DIR}/${APP_NAME}.app"

NEXT="$(awk -v v="${VERSION}" 'BEGIN { printf "%.2f", v + 0.01 }')"
echo "${NEXT}" > VERSION

echo "Built  ${DEST_DIR}/${APP_NAME}.app"
echo "Version bumped: ${VERSION} -> ${NEXT} (takes effect next build)"
