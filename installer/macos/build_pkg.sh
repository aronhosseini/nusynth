#!/usr/bin/env bash
# Usage: build_pkg.sh <version> <path/to/NuSynth.vst3> [installer-signing-identity]
set -euo pipefail

VERSION="$1"
VST3="$2"
SIGN_ID="${3:-}"

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="dist"
STAGE="build/pkg"
mkdir -p "$OUT" "$STAGE"

pkgbuild \
  --component "$VST3" \
  --install-location "/Library/Audio/Plug-Ins/VST3" \
  --identifier "com.aronh.nusynth.vst3" \
  --version "$VERSION" \
  "$STAGE/NuSynth-vst3.pkg"

PKG="$OUT/NuSynth-${VERSION}-macOS.pkg"
if [[ -n "$SIGN_ID" ]]; then
  productbuild --distribution "$HERE/distribution.xml" --package-path "$STAGE" \
               --sign "$SIGN_ID" "$PKG"
else
  productbuild --distribution "$HERE/distribution.xml" --package-path "$STAGE" "$PKG"
fi
echo "Created $PKG"
