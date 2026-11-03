#!/bin/bash
# Removes gensexuell from this Mac (double-click opens it in Terminal).
# Patches and renders in Documents/Seedling stay on purpose.
echo "Removes gensexuell: app, VST3, AU, CLAP, LV2, manual and source."
echo "Entfernt gensexuell: App, VST3, AU, CLAP, LV2, Handbuch und Quellcode."
echo "Your patches in Documents/Seedling stay. / Deine Patches in Dokumente/Seedling bleiben."
read -r -p "Continue / Weiter? [y/N] " answer
case "$answer" in y|Y|j|J) ;; *) exit 0 ;; esac
sudo rm -rf /Applications/gensexuell.app \
  /Library/Audio/Plug-Ins/VST3/gensexuell.vst3 \
  /Library/Audio/Plug-Ins/Components/gensexuell.component \
  /Library/Audio/Plug-Ins/CLAP/gensexuell.clap \
  /Library/Audio/Plug-Ins/LV2/gensexuell.lv2 \
  "/Library/Application Support/beatsexuell/gensexuell"
sudo rmdir "/Library/Application Support/beatsexuell" 2>/dev/null || true
for p in $(pkgutil --pkgs='com\.beatsexuell\.gensexuell\..*'); do sudo pkgutil --forget "$p" >/dev/null; done
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Done. / Fertig."
