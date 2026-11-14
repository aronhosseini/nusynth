#!/usr/bin/env bash
# NuSynth installer for macOS (Gatekeeper friendly).
#
#   curl -fsSL https://github.com/YOUR_GITHUB_USER/NuSynth/releases/latest/download/install.sh | bash
#
# Why this works: files downloaded with curl are NOT marked "quarantined" by macOS, so Gatekeeper does not block them.
# The script also removes the quarantine flag (xattr) and ad-hoc signs the plugin, which Apple Silicon requires.
#
# Options:   --system      install for all users (/Library/..., asks for your password)
#            --uninstall   remove NuSynth
#            NUSYNTH_REPO=owner/repo  NUSYNTH_VERSION=1.2.3   (environment variables)
set -euo pipefail

REPO="${NUSYNTH_REPO:-YOUR_GITHUB_USER/NuSynth}"
VERSION="${NUSYNTH_VERSION:-latest}"
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
SUDO=""

for arg in "$@"; do
  case "$arg" in
    --system)    DEST="/Library/Audio/Plug-Ins/VST3"; SUDO="sudo" ;;
    --uninstall) ;;
  esac
done

if [[ " $* " == *" --uninstall "* ]]; then
  echo "Removing NuSynth..."
  rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/NuSynth.vst3"
  sudo rm -rf "/Library/Audio/Plug-Ins/VST3/NuSynth.vst3" 2>/dev/null || true
  echo "Done."
  exit 0
fi

if [[ "$VERSION" == "latest" ]]; then
  URL="https://github.com/${REPO}/releases/latest/download/NuSynth-macOS.zip"
else
  URL="https://github.com/${REPO}/releases/download/v${VERSION}/NuSynth-${VERSION}-macOS.zip"
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading NuSynth from ${URL} ..."
curl -fL --progress-bar -o "$TMP/NuSynth.zip" "$URL"

echo "Unpacking ..."
/usr/bin/unzip -q "$TMP/NuSynth.zip" -d "$TMP"
[[ -d "$TMP/NuSynth.vst3" ]] || { echo "NuSynth.vst3 not found in the download"; exit 1; }

echo "Installing to ${DEST} ..."
$SUDO mkdir -p "$DEST"
$SUDO rm -rf "$DEST/NuSynth.vst3"
$SUDO cp -R "$TMP/NuSynth.vst3" "$DEST/"

# remove the "downloaded from the internet" flag and sign ad-hoc (needed on Apple Silicon)
$SUDO xattr -dr com.apple.quarantine "$DEST/NuSynth.vst3" 2>/dev/null || true
$SUDO codesign --force --deep -s - "$DEST/NuSynth.vst3" 2>/dev/null || true

echo
echo "NuSynth is installed:  $DEST/NuSynth.vst3"
echo "Restart your DAW and rescan plugins."
