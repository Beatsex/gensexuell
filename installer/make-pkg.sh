#!/bin/sh
# Builds the macOS installer from a packaged build folder (the one CI zips):
#   installer/make-pkg.sh <package-dir> <version> [output.pkg]
# Installs the app to /Applications, the plugins to /Library/Audio/Plug-Ins, manuals,
# licenses and presets to /Library/Application Support/beatsexuell/gensexuell; the
# postinstall script copies the presets to the user's Documents/Seedling/Presets.
set -eu
SRC=${1:?package dir}; VERSION=${2:?version}
OUT=${3:-gensexuell-beta-$VERSION-macos-universal.pkg}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
ROOT=$WORK/root; P=$ROOT/Library/Audio/Plug-Ins
SUP="$ROOT/Library/Application Support/beatsexuell/gensexuell"
mkdir -p "$P/VST3" "$P/Components" "$P/CLAP" "$P/LV2" "$ROOT/Applications" "$SUP"

ditto "$SRC/gensexuell.vst3"      "$P/VST3/gensexuell.vst3"
ditto "$SRC/gensexuell.component" "$P/Components/gensexuell.component"
ditto "$SRC/gensexuell.clap"      "$P/CLAP/gensexuell.clap"
ditto "$SRC/gensexuell.lv2"       "$P/LV2/gensexuell.lv2"
ditto "$SRC/gensexuell.app"       "$ROOT/Applications/gensexuell.app"
ditto "$SRC/Presets"              "$SUP/Presets"
cp "$SRC/LICENSE" "$SRC/THIRD-PARTY-LICENSES.txt" "$SRC/User Manual.html" "$SRC/Benutzerhandbuch.html" "$SRC/README.txt" "$SUP/"
xattr -cr "$ROOT"; find "$ROOT" -name .DS_Store -delete

# keep the bundles at their paths (pkgbuild would otherwise follow moved copies)
pkgbuild --analyze --root "$ROOT" "$WORK/comp.plist" >/dev/null
sed -i '' '/BundleIsRelocatable/{n;s/<true\/>/<false\/>/;}' "$WORK/comp.plist"

pkgbuild --root "$ROOT" --component-plist "$WORK/comp.plist" --scripts "$HERE/macos" \
  --identifier com.beatsexuell.gensexuell --version "$VERSION" --install-location / "$OUT"
