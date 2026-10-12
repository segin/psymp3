#!/bin/sh
# package/appimage/build-appimage.sh - build a PsyMP3 AppImage from a source tree.
#
# An AppImage is one executable file holding PsyMP3 and the libraries it
# needs. It runs on any distribution whose glibc is at least as new as the one
# it was built against, so build it on the oldest base worth supporting: the
# Ubuntu 22.04 image in the Dockerfile beside this script, which also has the
# tools this script calls (linuxdeploy and its Qt plugin). --docker builds
# that image and runs the rest of this script inside it.
#
# Like the .deb and .rpm builders this packages a pristine export of HEAD:
# what is COMMITTED, not what is in the working tree. Pass --worktree to
# package the tree as-is, which requires it to be distclean.
#
# Usage: package/appimage/build-appimage.sh [--docker] [--worktree] [--output DIR]
#
# The AppImage lands in DIR (default package/appimage/out).
set -eu

top=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
pkgdir="$top/package/appimage"
outdir="$pkgdir/out"
source_mode=export
use_docker=no

while [ $# -gt 0 ]; do
    case "$1" in
        --docker) use_docker=yes; shift ;;
        --worktree) source_mode=worktree; shift ;;
        --output) outdir=$2; shift 2 ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "build-appimage.sh: unknown argument '$1'" >&2; exit 2 ;;
    esac
done

mkdir -p "$outdir"
outdir=$(CDPATH= cd -- "$outdir" && pwd)

if [ "$use_docker" = yes ]; then
    image=${PSYMP3_APPIMAGE_IMAGE:-psymp3-appimage}
    docker build -t "$image" "$pkgdir"
    args=
    [ "$source_mode" = worktree ] && args=--worktree
    # Run as the calling user so the AppImage is not left owned by root; HOME
    # is somewhere that user can write in the container.
    exec docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
        -v "$top:/src" -v "$outdir:/out" "$image" \
        /src/package/appimage/build-appimage.sh --output /out $args
fi

for tool in linuxdeploy linuxdeploy-plugin-qt; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build-appimage.sh: $tool not found; run with --docker, or see the Dockerfile" >&2
        exit 1
    }
done

version=$("$top/package/version.sh" upstream)
arch=$(uname -m)
echo "build-appimage.sh: building psymp3 $version ($arch)"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
build="$work/psymp3-$version"
appdir="$work/AppDir"

mkdir -p "$build"
if [ "$source_mode" = export ] && git -C "$top" rev-parse --git-dir >/dev/null 2>&1; then
    git -C "$top" archive --format=tar HEAD | tar -x -C "$build"
else
    tar -c -C "$top" \
        --exclude=.git \
        --exclude=package/dpkg/out --exclude=package/rpm/out \
        --exclude=package/appimage/out \
        . | tar -x -C "$build"
fi

(
    cd "$build"
    ./autogen.sh
    # /usr is where the bundle keeps its files, under the AppDir. --enable-final
    # builds one translation unit, which is much the fastest way to build once.
    ./configure --prefix=/usr --enable-final
    make -j"$(nproc)"
    make install DESTDIR="$appdir"
)

# The data directory compiled into the binary is /usr/share/psymp3/data, which
# does not exist on the user's machine: the bundle is mounted somewhere new on
# every run. linuxdeploy's AppRun sources these hooks first, with APPDIR set
# to the mount point. A PSYMP3_DATADIR the user set themselves is left alone.
mkdir -p "$appdir/apprun-hooks"
cat > "$appdir/apprun-hooks/psymp3-datadir.sh" <<'EOF'
export PSYMP3_DATADIR="${PSYMP3_DATADIR:-$APPDIR/usr/share/psymp3/data}"
EOF

# linuxdeploy copies in every library the binary links against, except the
# ones every desktop is expected to have (glibc, libstdc++, X11, GL), and the
# Qt plugin adds the Qt plugins the file dialog needs. QMAKE names Qt 6.
# Both spellings of the output name are set: LDAI_OUTPUT is the current one,
# OUTPUT the one older linuxdeploy releases read.
output="psymp3-$version-$arch.AppImage"
(
    cd "$work"
    export QMAKE="${QMAKE:-$(command -v qmake6)}"
    export LINUXDEPLOY_OUTPUT_VERSION="$version"
    export LDAI_OUTPUT="$output" OUTPUT="$output"
    linuxdeploy --appdir "$appdir" \
        --executable "$appdir/usr/bin/psymp3" \
        --desktop-file "$appdir/usr/share/applications/psymp3.desktop" \
        --icon-file "$appdir/usr/share/icons/hicolor/128x128/apps/psymp3.png" \
        --plugin qt \
        --output appimage
)

[ -f "$work/$output" ] || { echo "build-appimage.sh: no AppImage produced" >&2; exit 1; }
mv -f "$work/$output" "$outdir/"

echo "build-appimage.sh: artifact in $outdir"
ls -la "$outdir/$output"
