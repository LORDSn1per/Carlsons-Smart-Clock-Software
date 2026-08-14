#!/bin/bash
# Cross-builds the native Windows edition of Clock Builder from macOS.

set -euo pipefail
cd "$(dirname "$0")"

VERSION="$(tr -d '[:space:]' < VERSION)"
APP_NAME="Clock Builder ${VERSION}"
DEST_DIR="${1:-/Volumes/home/Documents/Arduino/SmartClock/Software/Clock Builder/Windows}"

GO_VERSION="1.26.4"
GO_SHA256="05dc9b5f9997744520aaebb3d5deaa7c755371aebbfb7f97c2511a9f3367538d"
GO_ARCHIVE="${TMPDIR:-/tmp}/go${GO_VERSION}.darwin-amd64.tar.gz"
GO_ROOT="${TMPDIR:-/tmp}/clockbuilder-go-${GO_VERSION}"
GO_BIN="${GO_ROOT}/bin/go"

ESPTOOL_VERSION="5.3.1"
ESPTOOL_SHA256="2b4a73c45db27426685896f64ce3e557f63a64f43cc100cb65c0cc3486af96d3"
ESPTOOL_ARCHIVE="${TMPDIR:-/tmp}/esptool-v${ESPTOOL_VERSION}-windows-amd64.zip"

BUILD_DIR="${TMPDIR:-/tmp}/clockbuilder-windows-build"
GO_CACHE="${TMPDIR:-/tmp}/clockbuilder-go-cache"
GO_PATH="${TMPDIR:-/tmp}/clockbuilder-go-path"

checksum() {
    shasum -a 256 "$1" | awk '{print $1}'
}

if [[ ! -x "${GO_BIN}" ]]; then
    if [[ ! -f "${GO_ARCHIVE}" ]] || [[ "$(checksum "${GO_ARCHIVE}")" != "${GO_SHA256}" ]]; then
        curl -L --fail --output "${GO_ARCHIVE}" \
            "https://go.dev/dl/go${GO_VERSION}.darwin-amd64.tar.gz"
    fi
    if [[ "$(checksum "${GO_ARCHIVE}")" != "${GO_SHA256}" ]]; then
        echo "Go download failed its SHA-256 check" >&2
        exit 1
    fi
    rm -rf "${GO_ROOT}"
    mkdir -p "${GO_ROOT}"
    tar -xzf "${GO_ARCHIVE}" -C "${GO_ROOT}" --strip-components=1
fi

if [[ ! -f "${ESPTOOL_ARCHIVE}" ]] || [[ "$(checksum "${ESPTOOL_ARCHIVE}")" != "${ESPTOOL_SHA256}" ]]; then
    curl -L --fail --output "${ESPTOOL_ARCHIVE}" \
        "https://github.com/espressif/esptool/releases/download/v${ESPTOOL_VERSION}/esptool-v${ESPTOOL_VERSION}-windows-amd64.zip"
fi
if [[ "$(checksum "${ESPTOOL_ARCHIVE}")" != "${ESPTOOL_SHA256}" ]]; then
    echo "Espressif esptool download failed its SHA-256 check" >&2
    exit 1
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" "${DEST_DIR}"

echo "Building ${APP_NAME}.exe"
GOOS=windows GOARCH=amd64 CGO_ENABLED=0 GO111MODULE=off \
GOCACHE="${GO_CACHE}" GOPATH="${GO_PATH}" \
    "${GO_BIN}" build \
    -trimpath \
    -ldflags "-H=windowsgui -s -w -X main.appVersion=${VERSION}" \
    -o "${BUILD_DIR}/${APP_NAME}.exe" \
    ./Sources

unzip -j -o "${ESPTOOL_ARCHIVE}" \
    '*/esptool.exe' '*/LICENSE' \
    -d "${BUILD_DIR}" >/dev/null
mv "${BUILD_DIR}/LICENSE" "${BUILD_DIR}/ESPTOOL-LICENSE.txt"

rm -f "${DEST_DIR}/${APP_NAME}.exe"
cp "${BUILD_DIR}/${APP_NAME}.exe" "${DEST_DIR}/${APP_NAME}.exe"
cp "${BUILD_DIR}/esptool.exe" "${DEST_DIR}/esptool.exe"
cp "${BUILD_DIR}/ESPTOOL-LICENSE.txt" "${DEST_DIR}/ESPTOOL-LICENSE.txt"

NEXT="$(awk -v v="${VERSION}" 'BEGIN { printf "%.2f", v + 0.01 }')"
printf '%s\n' "${NEXT}" > VERSION

echo "Built  ${DEST_DIR}/${APP_NAME}.exe"
echo "Bundled Espressif esptool ${ESPTOOL_VERSION} beside the app"
echo "Version bumped: ${VERSION} -> ${NEXT} (takes effect next build)"
