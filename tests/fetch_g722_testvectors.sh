#!/bin/bash
#
# fetch_g722_testvectors.sh - Get the ITU-T G.722 Appendix II test sequences
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#
# The digital test sequences of Rec. ITU-T G.722 Appendix II are ITU's, and
# are distributed only inside ITU's software package for the Recommendation.
# They are not kept in this repository. This script downloads the package
# and extracts the main-body sequences (binary, 16-bit little-endian) into
# tests/data/g722, which test_g722_conformance reads. Only those data files
# are extracted: nothing else in the package is unpacked or used.
#
# ITU's server has been seen to drop the connection just before the end of
# the 139 MB download. The package is a plain zip whose sequences sit well
# before its end, so each wanted file is read through its own local header,
# and a download that stopped short still yields them intact. Every file is
# checked against the CRC-32 that ITU's own readme lists.
#
# Usage: fetch_g722_testvectors.sh [OUTPUT-DIR] [PACKAGE.zip]
#   OUTPUT-DIR   defaults to tests/data/g722
#   PACKAGE.zip  an already downloaded package; skips the download

set -e

OUT="${1:-tests/data/g722}"
PACKAGE="${2:-}"
URL='https://www.itu.int/rec/dologin_pub.asp?lang=e&id=T-REC-G.722-201209-I!!SOFT-ZST-E&type=items'

command -v python3 >/dev/null 2>&1 || { echo "ERROR: python3 is required" >&2; exit 1; }

cleanup=""
if [ -z "$PACKAGE" ]; then
    command -v curl >/dev/null 2>&1 || { echo "ERROR: curl is required" >&2; exit 1; }
    PACKAGE="$(mktemp)"
    cleanup="$PACKAGE"
    echo "Downloading the ITU-T G.722 (09/2012) software package (about 139 MB)"
    # A short read is expected and tolerated; see above.
    curl -fL --retry 3 -o "$PACKAGE" "$URL" || echo "  (download ended early; extracting what arrived)"
fi
trap '[ -n "$cleanup" ] && rm -f "$cleanup"' EXIT

mkdir -p "$OUT"
python3 - "$PACKAGE" "$OUT" <<'PY'
import os, struct, sys, zlib

package, out = sys.argv[1], sys.argv[2]
prefix = 'Software/G.722_MB-testvectors/g722-ts-le/bin/'
# CRC-32 and size of each sequence, from ITU's 00readme-g.722-testvectors-le.txt.
expected = {
    'bt1c1.xmt': (0x0C3BFCA7, 32832), 'bt1c2.xmt': (0x2D604685, 1600),
    'bt1d3.cod': (0x7398964F, 32832), 'bt2r1.cod': (0xD1DAA1D1, 32832),
    'bt2r2.cod': (0x344EA5D0, 1600),  'bt3h1.rc0': (0xE9250851, 32832),
    'bt3h2.rc0': (0x5330AE2E, 1600),  'bt3h3.rc0': (0x3731AD7F, 32832),
    'bt3l1.rc1': (0xED1B3993, 32832), 'bt3l1.rc2': (0x8E8C4E2B, 32832),
    'bt3l1.rc3': (0xB7AA5569, 32832), 'bt3l2.rc1': (0xAF00F31F, 1600),
    'bt3l2.rc2': (0x9143E92C, 1600),  'bt3l2.rc3': (0xAE855C07, 1600),
    'bt3l3.rc1': (0xA5374659, 32832), 'bt3l3.rc2': (0x687B250A, 32832),
    'bt3l3.rc3': (0x3605736B, 32832),
}
data = open(package, 'rb').read()
found = {}
pos = 0
while True:
    at = data.find(b'PK\x03\x04', pos)
    if at < 0 or at + 30 > len(data):
        break
    (_, _, method, _, _, _, csize, _, nlen, xlen) = struct.unpack('<HHHHHIIIHH', data[at + 4:at + 30])
    name = data[at + 30:at + 30 + nlen].decode('latin-1')
    start = at + 30 + nlen + xlen
    pos = start + csize if start + csize <= len(data) and csize > 0 else at + 4
    if not name.startswith(prefix):
        continue
    base = name[len(prefix):]
    if base not in expected or start + csize > len(data):
        continue
    raw = data[start:start + csize]
    body = zlib.decompress(raw, -15) if method == 8 else raw
    crc, size = expected[base]
    if (zlib.crc32(body) & 0xFFFFFFFF, len(body)) != (crc, size):
        sys.exit(f'ERROR: {base} does not match its published CRC-32')
    with open(os.path.join(out, base), 'wb') as f:
        f.write(body)
    found[base] = True
missing = sorted(set(expected) - set(found))
if missing:
    sys.exit('ERROR: not found in the package: ' + ', '.join(missing))
print(f'Extracted {len(found)} test sequences into {out}, all matching ITU\'s CRC-32 list.')
PY
