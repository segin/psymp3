#!/bin/sh
# package/dpkg/build-deb.sh - build PsyMP3 .deb packages from a source tree.
#
# Builds from a pristine export of HEAD rather than the working tree, for the
# same reasons build-rpm.sh does: a configured tree carries generated
# Makefiles holding absolute paths, and dh_auto_clean's `make distclean`
# fails on them as soon as the tree is built anywhere but where it was
# configured. Exporting also keeps untracked build output out of the package.
#
# Consequence: this packages what is COMMITTED, not what is in the working
# tree. Commit first (or pass --worktree to package the tree as-is, which
# requires it to be distclean).
#
# Usage: package/dpkg/build-deb.sh [--worktree] [--output DIR]
#
# Artifacts land in DIR (default package/dpkg/out).
set -eu

top=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
pkgdir="$top/package/dpkg"
outdir="$pkgdir/out"
source_mode=export

while [ $# -gt 0 ]; do
    case "$1" in
        --worktree) source_mode=worktree; shift ;;
        --output) outdir=$2; shift 2 ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "build-deb.sh: unknown argument '$1'" >&2; exit 2 ;;
    esac
done

version=$("$top/package/version.sh" upstream)
# Source format is 3.0 (native): the version carries no Debian revision.
echo "build-deb.sh: building psymp3 $version"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
build="$work/psymp3-$version"

if [ "$source_mode" = export ] && git -C "$top" rev-parse --git-dir >/dev/null 2>&1; then
    mkdir -p "$build"
    git -C "$top" archive --format=tar HEAD | tar -x -C "$build"
else
    mkdir -p "$build"
    tar -c -C "$top" \
        --exclude=.git --exclude=debian \
        --exclude=package/dpkg/out --exclude=package/rpm/out \
        . | tar -x -C "$build"
fi

cp -a "$pkgdir/debian" "$build/debian"
chmod +x "$build/debian/rules"

# An UNRELEASED distribution makes some tooling refuse the package; name the
# suite we are actually building on when we can tell.
suite=unstable
if [ -r /etc/os-release ]; then
    . /etc/os-release
    case "${ID:-}" in
        debian) suite=${VERSION_CODENAME:-unstable} ;;
        ubuntu) suite=${VERSION_CODENAME:-noble} ;;
    esac
fi

cat > "$build/debian/changelog" <<EOF
psymp3 ($version) $suite; urgency=medium

  * Automated build from the PsyMP3 source tree.
    See https://github.com/segin/psymp3/releases for release notes.

 -- Kirn Gill II <segin2005@gmail.com>  $(date -R)
EOF

# -b: binary only. -us -uc: unsigned (no maintainer key in CI).
(cd "$build" && dpkg-buildpackage -b -us -uc)

mkdir -p "$outdir"
# dpkg drops its artifacts in the PARENT of the build tree.
found=no
for f in "$work"/psymp3_"$version"_*.deb "$work"/psymp3_"$version"_*.buildinfo \
         "$work"/psymp3_"$version"_*.changes; do
    [ -e "$f" ] || continue
    mv -f "$f" "$outdir/"
    found=yes
done
[ "$found" = yes ] || { echo "build-deb.sh: no artifacts produced" >&2; exit 1; }

echo "build-deb.sh: artifacts in $outdir"
ls -la "$outdir"
