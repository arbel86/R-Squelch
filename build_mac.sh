#!/bin/bash
# R-Squelch — build on a Mac (needs Xcode or the Xcode Command Line Tools, and CMake: `brew install cmake`).
# Builds a universal (Apple Silicon + Intel) VST3, AU and standalone app, then the installer.
set -euo pipefail
cd "$(dirname "$0")"
VER=0.1.0
# the GitHub copy keeps images as text (assets_b64); turn them back into files if needed
mkdir -p assets
for f in assets_b64/*.b64; do
  [ -e "$f" ] || continue
  out="assets/$(basename "${f%.b64}")"
  [ -f "$out" ] || base64 -D -i "$f" -o "$out"
done
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac --config Release --target RSquelch_VST3 RSquelch_AU RSquelch_Standalone -j "$(sysctl -n hw.ncpu)"
ART=build-mac/RSquelch_artefacts/Release
for b in "$ART/VST3/R-Squelch.vst3" "$ART/AU/R-Squelch.component" "$ART/Standalone/R-Squelch.app"; do
  codesign --force --deep --sign - "$b"      # ad-hoc signature (needed on Apple Silicon)
done
installer/mac/make_pkg.sh "$ART" "$VER" Output-mac
echo "Done: Output-mac/R-Squelch-$VER-Mac.pkg"
