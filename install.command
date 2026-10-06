#!/bin/bash
# Double-click (or run in Terminal) to install the plugins in this folder for Logic Pro and Ableton Live.
cd "$(dirname "$0")"
AU=~/Library/Audio/Plug-Ins/Components
VST=~/Library/Audio/Plug-Ins/VST3
mkdir -p "$AU" "$VST"
for b in *.component; do [ -e "$b" ] || continue; rm -rf "$AU/$b"; cp -R "$b" "$AU/"; xattr -cr "$AU/$b"; echo "Installed $b"; done
for b in *.vst3; do [ -e "$b" ] || continue; rm -rf "$VST/$b"; cp -R "$b" "$VST/"; xattr -cr "$VST/$b"; echo "Installed $b"; done
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Done. Restart Logic / Ableton."
