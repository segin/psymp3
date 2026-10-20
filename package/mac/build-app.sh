#!/bin/sh
# package/mac/build-app.sh - build PsyMP3.app, and a .dmg holding it, on macOS.
#
# The app is built against Homebrew's libraries, which are then copied into
# the bundle (Contents/Frameworks) by dylibbundler, so it runs on a Mac with
# no Homebrew: on the same CPU architecture as the build, and on macOS as old
# as the oldest the bundled libraries allow (Info.plist says which). It is
# signed ad hoc, as Apple Silicon requires of anything it runs, but not with a
# Developer ID: Gatekeeper asks before opening it the first time.
#
# Like the other package builders this builds a pristine export of HEAD: what
# is COMMITTED, not what is in the working tree. Pass --worktree to build the
# tree as-is, which requires it to be distclean.
#
# Usage: package/mac/build-app.sh [--deps] [--worktree] [--output DIR]
#
#   --deps     brew install the build dependencies first
#
# PsyMP3.app's .dmg lands in DIR (default package/mac/out), with the app
# itself beside it.
set -eu

top=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
pkgdir="$top/package/mac"
outdir="$pkgdir/out"
source_mode=export
install_deps=no

while [ $# -gt 0 ]; do
    case "$1" in
        --deps) install_deps=yes; shift ;;
        --worktree) source_mode=worktree; shift ;;
        --output) outdir=$2; shift 2 ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "build-app.sh: unknown argument '$1'" >&2; exit 2 ;;
    esac
done

[ "$(uname -s)" = Darwin ] || { echo "build-app.sh: this builds on macOS" >&2; exit 1; }
command -v brew >/dev/null 2>&1 || { echo "build-app.sh: Homebrew is needed (https://brew.sh)" >&2; exit 1; }

if [ "$install_deps" = yes ]; then
    brew install autoconf automake libtool autoconf-archive pkgconf \
        sdl3 taglib freetype harfbuzz openssl@3 curl libogg opus fdk-aac speex \
        dylibbundler
fi
command -v dylibbundler >/dev/null 2>&1 || { echo "build-app.sh: dylibbundler not found; run with --deps" >&2; exit 1; }

# OpenSSL and curl are keg-only: Homebrew keeps them off pkg-config's path.
brew_prefix=$(brew --prefix)
PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix curl)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PKG_CONFIG_PATH

mkdir -p "$outdir"
outdir=$(CDPATH= cd -- "$outdir" && pwd)

version=$("$top/package/version.sh" upstream)
arch=$(uname -m)
jobs=$(sysctl -n hw.ncpu)
echo "build-app.sh: building psymp3 $version ($arch)"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
build="$work/psymp3-$version"
stage="$work/stage"
app="$work/PsyMP3.app"

mkdir -p "$build"
if [ "$source_mode" = export ] && git -C "$top" rev-parse --git-dir >/dev/null 2>&1; then
    git -C "$top" archive --format=tar HEAD | tar -x -C "$build"
else
    tar -c -C "$top" --exclude=.git --exclude=package/mac/out . | tar -x -C "$build"
fi

(
    cd "$build"
    ./autogen.sh
    # --enable-final builds one translation unit, much the fastest way to
    # build once. The prefix is a staging directory: the bundle is put
    # together from it below.
    ./configure --prefix="$stage" --enable-final || {
        # Why a check failed is in config.log, which goes with the work tree:
        # its last check, with the compiler's errors, up to the variable dump.
        awk '/^configure:[0-9]+: checking/ {buf = ""} /^## ----.*Cache variables/ {exit}
             {buf = buf $0 "\n"} END {printf "%s", buf}' config.log >&2
        exit 1
    }
    make -j"$jobs"
    make install
)

# The version strings, as configure.ac and the rc file give them.
full_version=$(sed -n 's/^AC_INIT(\[PsyMP3\],\[\([^]]*\)\].*/\1/p' "$build/configure.ac")
rc_fields=$(sed -n 's/^FILEVERSION \([0-9]*\),\([0-9]*\),\([0-9]*\),\([0-9]*\).*/\1 \2 \3 \4/p' "$build/res/psymp3.rc" | tr -d '\r')
set -- $rc_fields
short_version="$1.$2.$3"
build_number=$4

# The bundle: the program, its icon, and the data it looks for in
# Contents/Resources/data (the font, the window icon).
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" "$app/Contents/Frameworks"
cp "$stage/bin/psymp3" "$app/Contents/MacOS/psymp3"
cp "$build/res/psymp3.icns" "$app/Contents/Resources/psymp3.icns"
cp -R "$stage/share/psymp3/data" "$app/Contents/Resources/data"
printf 'APPL????' > "$app/Contents/PkgInfo"

# Homebrew's libraries, and theirs in turn, copied in and relinked to load
# from Contents/Frameworks.
dylibbundler -cd -of -b \
    -x "$app/Contents/MacOS/psymp3" \
    -d "$app/Contents/Frameworks" \
    -p @executable_path/../Frameworks/

# Nothing may still point into Homebrew: that would load only on this Mac.
if otool -L "$app/Contents/MacOS/psymp3" "$app"/Contents/Frameworks/*.dylib | grep -q "$brew_prefix"; then
    otool -L "$app/Contents/MacOS/psymp3" "$app"/Contents/Frameworks/*.dylib | grep "$brew_prefix" >&2
    echo "build-app.sh: libraries still load from $brew_prefix" >&2
    exit 1
fi

# The oldest macOS the bundle can run on is the newest minimum among its
# binaries (Homebrew builds its bottles for the macOS it runs on).
min_macos=$(for f in "$app/Contents/MacOS/psymp3" "$app"/Contents/Frameworks/*.dylib; do
                otool -l "$f" | awk '/LC_BUILD_VERSION/ {b=1} b && $1 == "minos" {print $2; b=0}'
            done | sort -t. -k1,1n -k2,2n | tail -n 1)
[ -n "$min_macos" ] || min_macos=11.0

sed -e "s/@VERSION@/$full_version/g" \
    -e "s/@SHORT_VERSION@/$short_version/g" \
    -e "s/@BUILD@/$build_number/g" \
    -e "s/@MIN_MACOS@/$min_macos/g" \
    "$pkgdir/Info.plist" > "$app/Contents/Info.plist"
plutil -lint "$app/Contents/Info.plist"

# Rewriting the libraries' load paths broke their signatures; Apple Silicon
# runs nothing unsigned. An ad-hoc signature satisfies it.
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

dmg="psymp3-$version-$arch.dmg"
rm -rf "$outdir/PsyMP3.app" "$outdir/$dmg"
cp -R "$app" "$outdir/PsyMP3.app"
hdiutil create -volname PsyMP3 -srcfolder "$app" -ov -format UDZO "$outdir/$dmg"

echo "build-app.sh: PsyMP3.app ($full_version, build $build_number, macOS $min_macos+) and $dmg in $outdir"
ls -la "$outdir"
