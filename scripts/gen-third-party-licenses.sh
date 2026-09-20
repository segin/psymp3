#!/bin/sh
# gen-third-party-licenses.sh - regenerate the embedded third-party license header.
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#
# THIRD-PARTY-LICENSES.txt is the source of truth; this wraps it in a macro so
# a standalone executable can carry it. Run after editing the .txt and commit
# both.
#
# One string literal per line of text, rather than one raw string across all of
# them: a raw string spanning newlines inside a #define is accepted by newer
# compilers but not by GCC 10, which reports an unterminated raw string and
# then parses the licence text as code (NetBSD 10's base compiler is GCC 10.5).
# The macro has to stay a macro, because about.cpp concatenates it with the
# literals around it.
#
# Usage: scripts/gen-third-party-licenses.sh [srcdir]

set -eu

srcdir="${1:-$(dirname "$0")/..}"
in="$srcdir/THIRD-PARTY-LICENSES.txt"
out="$srcdir/include/core/third_party_licenses.h"

[ -f "$in" ] || { echo "$0: missing $in" >&2; exit 1; }

{
    cat <<EOF
/*
 * third_party_licenses.h - generated; do not edit.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The licence texts below belong to their respective projects and are
 * reproduced verbatim. Regenerate with scripts/gen-third-party-licenses.sh
 * after editing THIRD-PARTY-LICENSES.txt, which is the source of truth.
 *
 * Several of the licences PsyMP3 links under -- zlib, BSD, MIT, curl,
 * Apache-2.0, the FreeType Licence and the Fraunhofer FDK AAC licence --
 * require their text, not merely a copyright line, to accompany a binary
 * redistribution. Embedding it here is what lets a lone .exe satisfy that.
 */

#ifndef PSYMP3_THIRD_PARTY_LICENSES_H
#define PSYMP3_THIRD_PARTY_LICENSES_H

/* The leading "\n" is the blank line that separates this from whatever
 * about.cpp concatenates in front of it. */
#define PSYMP3_THIRD_PARTY_LICENSES \\
    "\\n" \\
EOF
    # Backslashes and quotes are escaped, each line becomes one literal, and
    # every line but the last carries the continuation.
    sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/    "/' -e 's/$/\\n" \\/' "$in"
    printf '    ""\n\n#endif // PSYMP3_THIRD_PARTY_LICENSES_H\n'
} > "$out.tmp"

mv -f "$out.tmp" "$out"
echo "wrote $out ($(wc -c < "$out") bytes)"
