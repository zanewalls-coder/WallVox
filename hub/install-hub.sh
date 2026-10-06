#!/bin/bash
# One-line installer:  curl -fsSL https://github.com/zanewalls-coder/WallVox/releases/download/latest/install-hub.sh | bash
set -e
TMP=$(mktemp -d)
echo "Downloading Wall Hub..."
curl -fsSL -o "$TMP/hub.zip" "https://github.com/zanewalls-coder/WallVox/releases/download/latest/WallHub-Mac.zip"
/usr/bin/ditto -x -k "$TMP/hub.zip" "$TMP/x"
DEST="$HOME/Applications"
mkdir -p "$DEST"
rm -rf "$DEST/Wall Hub.app"
/usr/bin/ditto "$TMP/x/Wall Hub.app" "$DEST/Wall Hub.app"
/usr/bin/xattr -cr "$DEST/Wall Hub.app"
rm -rf "$TMP"
echo "Wall Hub installed in $DEST. Opening it now..."
open "$DEST/Wall Hub.app"
