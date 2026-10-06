#!/bin/bash
# Double-click (or run in Terminal) to install WallVox for Logic Pro and Ableton Live.
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/Components/WallVox.component ~/Library/Audio/Plug-Ins/VST3/WallVox.vst3
cp -R WallVox.component ~/Library/Audio/Plug-Ins/Components/
cp -R WallVox.vst3 ~/Library/Audio/Plug-Ins/VST3/
xattr -cr ~/Library/Audio/Plug-Ins/Components/WallVox.component ~/Library/Audio/Plug-Ins/VST3/WallVox.vst3
killall -9 AudioComponentRegistrar 2>/dev/null
echo "WallVox installed. Restart Logic / Ableton."
