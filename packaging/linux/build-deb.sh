#!/bin/bash
# Builds "Music Creative Lab Lite" (no VST hosting) and packages it as a .deb
# that installs on ChromeOS (Linux / Crostini, Debian 12) and Debian/Ubuntu.
# Usage: packaging/linux/build-deb.sh [output-dir]
# Optional env: JUCE_DIR=/path/to/JUCE (skip the download)
set -euo pipefail
SRC="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$SRC/dist}"
BUILD="${BUILD_DIR:-$SRC/build-lite}"
ARCH="$(dpkg --print-architecture)"
VER="$(sed -n 's/^project(MusicCreativeLab VERSION \([0-9.]*\).*/\1/p' "$SRC/CMakeLists.txt")"
EXTRA=()
[ -n "${JUCE_DIR:-}" ] && EXTRA+=("-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_DIR")

cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DMCL_LITE=ON "${EXTRA[@]}"
cmake --build "$BUILD" --config Release -j"$(nproc)"
BIN="$BUILD/MusicCreativeLab_artefacts/Release/Music Creative Lab Lite"
strip "$BIN"

# self-test (renders the demos offline and checks they make sound)
xvfb-run -a "$BIN" --selftest

PKG="$(mktemp -d)/pkg"
mkdir -p "$PKG/DEBIAN" "$PKG/opt/music-creative-lab-lite" "$PKG/usr/bin" \
         "$PKG/usr/share/applications" "$PKG/usr/share/icons/hicolor/256x256/apps" "$PKG/usr/share/doc/music-creative-lab-lite"
install -m755 "$BIN" "$PKG/opt/music-creative-lab-lite/music-creative-lab-lite"
ln -s /opt/music-creative-lab-lite/music-creative-lab-lite "$PKG/usr/bin/music-creative-lab-lite"
python3 - "$SRC/assets/icon.png" "$PKG/usr/share/icons/hicolor/256x256/apps/music-creative-lab-lite.png" 2>/dev/null <<'PY' || cp "$SRC/assets/icon.png" "$PKG/usr/share/icons/hicolor/256x256/apps/music-creative-lab-lite.png"
import sys
from PIL import Image
Image.open(sys.argv[1]).convert("RGBA").resize((256, 256)).save(sys.argv[2])
PY
cp "$SRC/LICENSE" "$PKG/usr/share/doc/music-creative-lab-lite/copyright"
cat > "$PKG/usr/share/applications/music-creative-lab-lite.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=Music Creative Lab Lite
Comment=Estudio de musica en directo: loops, synths, bateria y efectos
Exec=music-creative-lab-lite %f
Icon=music-creative-lab-lite
Terminal=false
Categories=AudioVideo;Audio;Music;
MimeType=application/x-music-creative-lab;
DESK
cat > "$PKG/DEBIAN/control" <<CTRL
Package: music-creative-lab-lite
Version: $VER
Section: sound
Priority: optional
Architecture: $ARCH
Depends: libasound2, libfreetype6, libfontconfig1, libx11-6, libxrandr2, libxinerama1, libxcursor1, libxext6
Maintainer: Music Creative Lab <noreply@example.com>
Description: Music Creative Lab Lite - live looping studio (no VST)
 Lightweight edition for ChromeOS (Linux) and low-RAM computers:
 loopers, Lab Synth, Lab Drums, 16 effects, piano roll, scenes,
 MIDI learn and set recording. No VST hosting.
CTRL
mkdir -p "$OUT"
DEB="$OUT/music-creative-lab-lite_${VER}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$PKG" "$DEB"
echo "Built $DEB"
