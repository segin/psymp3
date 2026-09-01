#!/bin/sh
# package/version.sh - derive a distro-legal package version from the tree.
#
# PsyMP3's own version string (configure.ac AC_INIT) is a human label like
# "2-CURRENT" or "2.0-RC2". Neither dpkg nor rpm accepts those verbatim as an
# upstream version in the form we want:
#   - dpkg forbids '-' in the upstream part of a non-native version (it is the
#     revision separator), so "2.0-RC2" would parse as upstream "2.0" rev "RC2".
#   - rpm forbids '-' in Version entirely.
# Both accept '~' (dpkg) / a tilde-bearing Version (rpm >= 4.10) to mark a
# pre-release that sorts BEFORE the final release, which is exactly what an RC
# or a rolling development snapshot is.
#
# Mapping:
#   2.0-RC2    -> 2.0~rc2                    (sorts before 2.0)
#   2.0-BETA4  -> 2.0~beta4
#   2-CURRENT  -> 2.0~snapshot<build>        (rolling master; <build> is the
#                                             monotonic res/psymp3.rc counter,
#                                             so successive snapshots upgrade)
#   1.99.17-RELEASE -> 1.99.17
#
# Usage: package/version.sh [upstream|build|full]
#   upstream (default) - the version alone, e.g. 2.0~rc2
#   build              - the res/psymp3.rc build counter, e.g. 1343
#   full               - upstream-1, ready for a .deb changelog entry
set -eu

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

raw=$(sed -n 's/^AC_INIT(\[PsyMP3\],\[\([^]]*\)\].*/\1/p' "$top/configure.ac" | head -1)
[ -n "$raw" ] || { echo "version.sh: cannot read AC_INIT from configure.ac" >&2; exit 1; }

# res/psymp3.rc has CRLF endings; strip the CR or it lands inside the version.
build=$(sed -n 's/^FILEVERSION [0-9]*,[0-9]*,[0-9]*,\([0-9]*\).*/\1/p' "$top/res/psymp3.rc" \
        | tr -d '\r' | head -1)
[ -n "$build" ] || build=0

case "$raw" in
    *-CURRENT)
        # Rolling development build off master. Base it on the next release
        # series so a later real 2.0 still upgrades cleanly over a snapshot.
        series=${raw%-CURRENT}
        case "$series" in *.*) ;; *) series="$series.0" ;; esac
        upstream="$series~snapshot$build"
        ;;
    *-RELEASE)
        upstream=${raw%-RELEASE}
        ;;
    *-*)
        # 2.0-RC2 / 2.0-BETA4 style pre-releases.
        base=${raw%%-*}
        pre=${raw#*-}
        pre=$(printf '%s' "$pre" | tr '[:upper:]' '[:lower:]')
        upstream="$base~$pre"
        ;;
    *)
        upstream="$raw"
        ;;
esac

case "${1:-upstream}" in
    upstream) printf '%s\n' "$upstream" ;;
    build)    printf '%s\n' "$build" ;;
    full)     printf '%s-1\n' "$upstream" ;;
    *) echo "usage: $0 [upstream|build|full]" >&2; exit 2 ;;
esac
