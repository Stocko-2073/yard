#!/bin/sh
# Explicit bootstrap for this project's current Apple Silicon development host.
set -eu
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Darwin ] || [ "$(uname -m)" != arm64 ]; then
    echo 'This bootstrap currently supports Apple Silicon macOS only.' >&2
    exit 1
fi
revision=11d0cf678105d614d675e6d9bd2aaf3eeff12f8c
checksum=92db37975ad7ff3c3c9bc27cba1503287377cb287ebabf60d1c6b597abfa3244
mkdir -p tools/bin
if [ -f tools/bin/sokol-shdc ] &&
    [ "$(shasum -a 256 tools/bin/sokol-shdc | awk '{print $1}')" = "$checksum" ]; then
    chmod +x tools/bin/sokol-shdc
    echo 'Pinned sokol-shdc is already installed.'
    exit 0
fi
download=$(mktemp tools/bin/sokol-shdc.XXXXXX)
trap 'rm -f "$download"' EXIT HUP INT TERM
curl --fail --location --retry 3 \
    "https://raw.githubusercontent.com/floooh/sokol-tools-bin/$revision/bin/osx_arm64/sokol-shdc" \
    --output "$download"
printf '%s  %s\n' "$checksum" "$download" | shasum -a 256 -c -
chmod +x "$download"
mv "$download" tools/bin/sokol-shdc
echo "Installed sokol-shdc from sokol-tools-bin $revision."
