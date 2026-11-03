#!/bin/bash
# macOS installer for gensexuell: one .pkg with a choice per format (app, VST3, AU, CLAP, LV2).
# Needs pkgbuild and productbuild (Xcode command line tools). After building the plugins, run:
#   installer/mac/build-pkg.sh [folder]
# [folder] holds gensexuell.app, .vst3, .component, .clap and .lv2 directly (the signed copies the workflow packs);
# without it the bundles come from build/Seedling_artefacts/Release. The .pkg lands in build/installer.
# Set MAC_INSTALLER_IDENTITY to a "Developer ID Installer: …" identity to sign it; otherwise it is unsigned.
set -euo pipefail
cd "$(dirname "$0")/../.."

VERSION=$(sed -n 's/^#define AppVersion "\(.*\)"/\1/p' installer/gensexuell.iss | tr -d '\r')
SRC=${1:-}
A=build/Seedling_artefacts/Release
OUT=build/installer
WORK=$OUT/mac
# the package ids identify the parts for upgrades and the uninstaller: never change them
ID=com.beatsexuell.gensexuell

bundle() { # file name -> where it is
  if [ -n "$SRC" ]; then echo "$SRC/$1"; return; fi
  case "$1" in
    *.app) echo "$A/Standalone/$1" ;;
    *.vst3) echo "$A/VST3/$1" ;;
    *.component) echo "$A/AU/$1" ;;
    *.clap) echo "$A/CLAP/$1" ;;
    *.lv2) echo "$A/LV2/$1" ;;
  esac
}

# part: id, the file or folder to install, install location, optional scripts folder
part() {
  local name=$1 file=$2 location=$3 scripts=${4:-}
  local root="$WORK/root/$name" plist="$WORK/$name.plist"
  local from; from=$(bundle "$file")
  [ -e "$from" ] || { echo "missing: $from" >&2; exit 1; }
  mkdir -p "$root"
  ditto "$from" "$root/$file"
  # keep every bundle where it belongs: by default Installer "relocates" it to wherever an older copy was found
  pkgbuild --analyze --root "$root" "$plist" >/dev/null
  local i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
    # the key is not always there, so replace instead of set
    /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" >/dev/null 2>&1 || true
    /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
    i=$((i + 1))
  done
  local args=(--root "$root" --identifier "$ID.$name" --version "$VERSION" --install-location "$location")
  [ "$i" -gt 0 ] && args+=(--component-plist "$plist")
  [ -n "$scripts" ] && args+=(--scripts "$scripts")
  pkgbuild "${args[@]}" "$WORK/pkgs/$name.pkg" >/dev/null
}

rm -rf "$WORK"
mkdir -p "$WORK/pkgs" "$WORK/resources"

part app gensexuell.app /Applications
part vst3 gensexuell.vst3 /Library/Audio/Plug-Ins/VST3
part au gensexuell.component /Library/Audio/Plug-Ins/Components installer/mac/scripts/au
part clap gensexuell.clap /Library/Audio/Plug-Ins/CLAP
part lv2 gensexuell.lv2 /Library/Audio/Plug-Ins/LV2

# manual, licenses, the source (AGPL) and the uninstaller always come along
[ -f "$OUT/gensexuell-$VERSION-source.zip" ] || python3 installer/make-source.py
DOCS="$WORK/root/docs"
mkdir -p "$DOCS"
cp installer/USER_MANUAL.html "$DOCS/User Manual.html"
cp LICENSE "$DOCS/LICENSE.txt"
cp THIRD-PARTY-LICENSES.txt "$OUT/gensexuell-$VERSION-source.zip" "$DOCS/"
cp installer/mac/uninstall.command "$DOCS/Uninstall gensexuell.command"
chmod 755 "$DOCS/Uninstall gensexuell.command"
pkgbuild --root "$DOCS" --identifier "$ID.docs" --version "$VERSION" \
  --install-location "/Library/Application Support/beatsexuell/gensexuell" "$WORK/pkgs/docs.pkg" >/dev/null

# texts of the installer window, with the version filled in
for lang in en de; do
  mkdir -p "$WORK/resources/$lang.lproj"
  for f in installer/mac/$lang.lproj/*; do
    sed "s/@VERSION@/$VERSION/g" "$f" > "$WORK/resources/$lang.lproj/$(basename "$f")"
  done
done
sed "s/@VERSION@/$VERSION/g" installer/mac/Distribution.xml > "$WORK/Distribution.xml"

PKG="$OUT/gensexuell-beta-$VERSION-macos-universal.pkg"
sign=()
[ -n "${MAC_INSTALLER_IDENTITY:-}" ] && sign=(--sign "$MAC_INSTALLER_IDENTITY")
productbuild --distribution "$WORK/Distribution.xml" --package-path "$WORK/pkgs" \
  --resources "$WORK/resources" "${sign[@]+"${sign[@]}"}" "$PKG"
echo "$PKG $(($(stat -f %z "$PKG") / 1024)) KB"
