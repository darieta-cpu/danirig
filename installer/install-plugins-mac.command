#!/bin/bash
# Installa i plugin DaniRig (VST3 + AU) per l'utente corrente e toglie la quarantena di macOS.
cd "$(dirname "$0")"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"
if [ -d "DaniRig.vst3" ]; then
  rm -rf "$VST3_DIR/DaniRig.vst3"; cp -R "DaniRig.vst3" "$VST3_DIR/"
  xattr -cr "$VST3_DIR/DaniRig.vst3"; echo "Installato VST3: $VST3_DIR/DaniRig.vst3"
fi
if [ -d "DaniRig.component" ]; then
  rm -rf "$AU_DIR/DaniRig.component"; cp -R "DaniRig.component" "$AU_DIR/"
  xattr -cr "$AU_DIR/DaniRig.component"; echo "Installato AU:   $AU_DIR/DaniRig.component"
fi
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Fatto. Riavvia la DAW (in Reaper: Options > Preferences > Plug-ins > VST/AU > Re-scan)."
