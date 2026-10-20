#!/bin/sh
# package/mac/make-universal.sh - join an Apple Silicon and an Intel
# PsyMP3.app, each made by build-app.sh, into one universal PsyMP3.app, and
# a .dmg holding it.
#
# Homebrew on each kind of Mac has only that kind's libraries, so each half
# is built on its own Mac; here every program and library the two share is
# joined with lipo. A library only one half uses is kept as that half has
# it: only that half loads it. Info.plist gets each half's oldest macOS
# (LSMinimumSystemVersionByArchitecture), and the bundle is signed again.
#
# Usage: package/mac/make-universal.sh ARM64.app X86_64.app [--output DIR]
#
# PsyMP3.app and psymp3-<version>-universal.dmg land in DIR (default
# package/mac/out).
set -eu

top=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
outdir="$top/package/mac/out"
[ $# -ge 2 ] || { sed -n '2,15p' "$0"; exit 2; }
arm=$1
intel=$2
shift 2
while [ $# -gt 0 ]; do
    case "$1" in
        --output) outdir=$2; shift 2 ;;
        *) echo "make-universal.sh: unknown argument '$1'" >&2; exit 2 ;;
    esac
done

for half in "$arm" "$intel"; do
    [ -x "$half/Contents/MacOS/psymp3" ] || { echo "make-universal.sh: $half is no PsyMP3.app" >&2; exit 1; }
done
[ "$(lipo -archs "$arm/Contents/MacOS/psymp3")" = arm64 ] || { echo "make-universal.sh: $arm is not arm64" >&2; exit 1; }
[ "$(lipo -archs "$intel/Contents/MacOS/psymp3")" = x86_64 ] || { echo "make-universal.sh: $intel is not x86_64" >&2; exit 1; }

mkdir -p "$outdir"
outdir=$(CDPATH= cd -- "$outdir" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
app="$work/PsyMP3.app"

# The Apple Silicon bundle is the base: its resources and Info.plist.
cp -R "$arm" "$app"

lipo -create "$arm/Contents/MacOS/psymp3" "$intel/Contents/MacOS/psymp3" -output "$app/Contents/MacOS/psymp3"
for lib in "$intel"/Contents/Frameworks/*.dylib; do
    name=$(basename "$lib")
    if [ -f "$arm/Contents/Frameworks/$name" ]; then
        lipo -create "$arm/Contents/Frameworks/$name" "$lib" -output "$app/Contents/Frameworks/$name"
    else
        cp "$lib" "$app/Contents/Frameworks/$name" # the Intel half's alone
    fi
done

# Each half may run on a different oldest macOS: say so per architecture,
# and the lower of the two overall.
plist="$app/Contents/Info.plist"
min_arm=$(plutil -extract LSMinimumSystemVersion raw "$arm/Contents/Info.plist")
min_intel=$(plutil -extract LSMinimumSystemVersion raw "$intel/Contents/Info.plist")
min_all=$(printf '%s\n%s\n' "$min_arm" "$min_intel" | sort -t. -k1,1n -k2,2n | head -n 1)
plutil -replace LSMinimumSystemVersion -string "$min_all" "$plist"
plutil -replace LSMinimumSystemVersionByArchitecture -json \
    "{\"arm64\":\"$min_arm\",\"x86_64\":\"$min_intel\"}" "$plist"
plutil -lint "$plist"

codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

version=$("$top/package/version.sh" upstream)
dmg="psymp3-$version-universal.dmg"
rm -rf "$outdir/PsyMP3.app" "$outdir/$dmg"
cp -R "$app" "$outdir/PsyMP3.app"
"$top/package/mac/make-dmg.sh" "$app" "$outdir/$dmg"

echo "make-universal.sh: PsyMP3.app (arm64 from macOS $min_arm, x86_64 from macOS $min_intel) and $dmg in $outdir"
lipo -archs "$outdir/PsyMP3.app/Contents/MacOS/psymp3"
