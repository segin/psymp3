#!/bin/sh
# package/rpm/build-rpm.sh - build PsyMP3 RPMs from a source tree.
#
# Creates a tarball of the tree (git archive when available, so untracked
# build droppings never leak into the package), then runs rpmbuild against
# package/rpm/psymp3.spec with the version derived by package/version.sh.
#
# Usage: package/rpm/build-rpm.sh [--output DIR]
#
# Artifacts land in DIR (default package/rpm/out).
set -eu

top=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
pkgdir="$top/package/rpm"
outdir="$pkgdir/out"

while [ $# -gt 0 ]; do
    case "$1" in
        --output) outdir=$2; shift 2 ;;
        -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
        *) echo "build-rpm.sh: unknown argument '$1'" >&2; exit 2 ;;
    esac
done

version=$("$top/package/version.sh" upstream)
echo "build-rpm.sh: building psymp3 $version"

# rpmbuild refuses '-' in Version; version.sh already maps to '~', but guard
# against a future mapping change rather than emitting a confusing rpm error.
case "$version" in
    *-*) echo "build-rpm.sh: version '$version' contains '-', which rpm forbids" >&2; exit 1 ;;
esac

build_root=$(mktemp -d)
trap 'rm -rf "$build_root"' EXIT
mkdir -p "$build_root"/SOURCES "$build_root"/SPECS

tarball="$build_root/SOURCES/psymp3-$version.tar.gz"
if git -C "$top" rev-parse --git-dir >/dev/null 2>&1; then
    git -C "$top" archive --format=tar --prefix="psymp3-$version/" HEAD | gzip -9 > "$tarball"
else
    # Not a git checkout (e.g. an extracted release tarball): pack the tree,
    # excluding build output that would bloat or break the package.
    tar -czf "$tarball" -C "$(dirname "$top")" \
        --transform="s,^$(basename "$top"),psymp3-$version," \
        --exclude=.git --exclude='*.o' --exclude='*.a' \
        --exclude=package/rpm/out --exclude=package/dpkg/out \
        "$(basename "$top")"
fi

cp "$pkgdir/psymp3.spec" "$build_root/SPECS/"

rpmbuild \
    --define "_topdir $build_root" \
    --define "psymp3_version $version" \
    -ba "$build_root/SPECS/psymp3.spec"

mkdir -p "$outdir"
find "$build_root/RPMS" "$build_root/SRPMS" -name '*.rpm' -exec mv -f {} "$outdir/" \;

echo "build-rpm.sh: artifacts in $outdir"
ls -la "$outdir"
