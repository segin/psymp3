#!/bin/sh
# package/mac/make-dmg.sh - pack PsyMP3.app into a .dmg beside a link to
# /Applications, so it installs by dragging the one onto the other.
#
# Usage: package/mac/make-dmg.sh PsyMP3.app OUT.dmg
set -eu

[ $# -eq 2 ] || { sed -n '2,6p' "$0"; exit 2; }
app=$1
dmg=$2
[ -d "$app/Contents" ] || { echo "make-dmg.sh: $app is no app bundle" >&2; exit 1; }

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
cp -R "$app" "$stage/PsyMP3.app"
# A symlink, which Finder shows with the Applications folder's own icon.
ln -s /Applications "$stage/Applications"

rm -f "$dmg"
hdiutil create -volname PsyMP3 -srcfolder "$stage" -ov -format UDZO "$dmg"
