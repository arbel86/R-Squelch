#!/bin/bash
# R-Squelch — builds the macOS installer (.pkg) and a .zip from the built plug-ins.
# usage: installer/mac/make_pkg.sh <artefacts dir (…/RSquelch_artefacts/Release)> <version> <output dir>
# The .pkg installs:
#   R-Squelch.vst3       -> /Library/Audio/Plug-Ins/VST3
#   R-Squelch.component  -> /Library/Audio/Plug-Ins/Components   (Audio Unit: Logic, GarageBand, Ableton…)
#   R-Squelch.app        -> /Applications                         (standalone)
set -euo pipefail
ART="$1"; VER="$2"; OUT="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
WORK="$(mktemp -d)"
mkdir -p "$WORK/vst3/Library/Audio/Plug-Ins/VST3" "$WORK/au/Library/Audio/Plug-Ins/Components" "$WORK/app/Applications" "$WORK/pkgs" "$WORK/res"
cp -R "$ART/VST3/R-Squelch.vst3" "$WORK/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/R-Squelch.component" "$WORK/au/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/Standalone/R-Squelch.app" "$WORK/app/Applications/"

for part in vst3 au app; do
  # always install to the fixed location (don't "relocate" to an older copy somewhere else)
  pkgbuild --analyze --root "$WORK/$part" "$WORK/$part.plist"
  n=$(/usr/libexec/PlistBuddy -c "Print" "$WORK/$part.plist" | grep -c "RootRelativeBundlePath" || true)
  for ((i=0; i<n; i++)); do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$part.plist"
  done
  pkgbuild --root "$WORK/$part" --component-plist "$WORK/$part.plist" \
           --identifier "com.rbell.rsquelch.$part" --version "$VER" --install-location / "$WORK/pkgs/rsquelch-$part.pkg"
done

cp "$HERE/welcome.html" "$WORK/res/welcome.html"
cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>R-Squelch $VER by RBell</title>
  <welcome file="welcome.html" mime-type="text/html"/>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="vst3"/>
    <line choice="au"/>
    <line choice="app"/>
  </choices-outline>
  <choice id="vst3" title="VST3 plug-in" description="For Cubase, Ableton Live, Bitwig, FL Studio, Reaper…"><pkg-ref id="com.rbell.rsquelch.vst3"/></choice>
  <choice id="au" title="Audio Unit plug-in" description="For Logic Pro, GarageBand, Ableton Live…"><pkg-ref id="com.rbell.rsquelch.au"/></choice>
  <choice id="app" title="Standalone app" description="R-Squelch.app in Applications"><pkg-ref id="com.rbell.rsquelch.app"/></choice>
  <pkg-ref id="com.rbell.rsquelch.vst3" version="$VER">rsquelch-vst3.pkg</pkg-ref>
  <pkg-ref id="com.rbell.rsquelch.au" version="$VER">rsquelch-au.pkg</pkg-ref>
  <pkg-ref id="com.rbell.rsquelch.app" version="$VER">rsquelch-app.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" --resources "$WORK/res" \
             "$OUT/R-Squelch-$VER-Mac.pkg"

# plain zip too (for people who prefer to copy the plug-ins by hand)
ZIPDIR="$WORK/zip/R-Squelch-$VER-Mac"
mkdir -p "$ZIPDIR"
cp -R "$ART/VST3/R-Squelch.vst3" "$ART/AU/R-Squelch.component" "$ART/Standalone/R-Squelch.app" "$ZIPDIR/"
cp "$HERE/README_MAC.txt" "$ZIPDIR/"
(cd "$WORK/zip" && ditto -c -k --keepParent "R-Squelch-$VER-Mac" "$OUT/R-Squelch-$VER-Mac.zip")
ls -la "$OUT"
