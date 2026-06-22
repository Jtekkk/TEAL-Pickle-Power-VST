#!/usr/bin/env bash
# ============================================================================
#  Pickle Power 🥒⚡  —  macOS installer (.pkg) builder
#
#  Build the plugin first:
#     cmake -B build -DCMAKE_BUILD_TYPE=Release
#     cmake --build build --config Release
#  Then run this script from the repo root:
#     bash packaging/macos/build_pkg.sh
#
#  Installs VST3 + AU to the system plug-in folders and the Standalone to
#  /Applications. (Unsigned — for distribution you'll want to codesign/notarize.)
# ============================================================================
set -euo pipefail

VERSION="1.0.0"
IDENTIFIER="com.pickleaudio.picklepower"
ART="build/PicklePower_artefacts/Release"

if [ ! -d "$ART" ]; then
  echo "error: $ART not found — build the plugin in Release first." >&2
  exit 1
fi

STAGE="$(mktemp -d)"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/VST3" \
         "$STAGE/Library/Audio/Plug-Ins/Components" \
         "$STAGE/Applications"

[ -d "$ART/VST3/Pickle Power.vst3" ]      && cp -R "$ART/VST3/Pickle Power.vst3"      "$STAGE/Library/Audio/Plug-Ins/VST3/"
[ -d "$ART/AU/Pickle Power.component" ]    && cp -R "$ART/AU/Pickle Power.component"   "$STAGE/Library/Audio/Plug-Ins/Components/"
[ -d "$ART/Standalone/Pickle Power.app" ] && cp -R "$ART/Standalone/Pickle Power.app" "$STAGE/Applications/"

OUT="PicklePower-${VERSION}-macOS.pkg"
pkgbuild --root "$STAGE" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location / \
         "$OUT"

rm -rf "$STAGE"
echo "Built $OUT"
