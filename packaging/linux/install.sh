#!/usr/bin/env bash
# ============================================================================
#  Pickle Power 🥒⚡  —  Linux user install
#
#  Build the plugin first:
#     cmake -B build -DCMAKE_BUILD_TYPE=Release
#     cmake --build build
#  Then run from the repo root:
#     bash packaging/linux/install.sh
#
#  Copies the VST3 into the per-user plug-in folder (~/.vst3).
# ============================================================================
set -euo pipefail

ART="build/PicklePower_artefacts/Release"
SRC="$ART/VST3/TEAL-Pickle-Power-VST.vst3"
DEST="${HOME}/.vst3"

if [ ! -d "$SRC" ]; then
  echo "error: $SRC not found — build the plugin in Release first." >&2
  exit 1
fi

mkdir -p "$DEST"
rm -rf "$DEST/TEAL-Pickle-Power-VST.vst3"
cp -R "$SRC" "$DEST/"
echo "Installed: $DEST/TEAL-Pickle-Power-VST.vst3"
