#!/bin/bash
#
# generate_ac3_corpus.sh - Build the (E-)AC-3 sample set with ffmpeg
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#
# PsyMP3's AC-3 decoder is written from ATSC A/52 and shares no code with
# any other implementation. ffmpeg is used here purely as an oracle: it
# produces conforming streams to decode, and its own output is the thing
# to compare against. Nothing in it is read or copied.
#
# The files are deliberately not checked in -- they are a few hundred
# kilobytes of generated data that this script reproduces exactly. Run it
# before the AC-3 integration tests.

set -e

OUT="${1:-tests/data}"
FFMPEG="${FFMPEG:-ffmpeg}"

if ! command -v "$FFMPEG" >/dev/null 2>&1; then
    echo "ERROR: $FFMPEG not found; it generates the sample streams." >&2
    echo "       Set FFMPEG=/path/to/ffmpeg if it lives elsewhere." >&2
    exit 1
fi

mkdir -p "$OUT"

gen() {
    local name="$1"; shift
    echo "  $name"
    "$FFMPEG" -loglevel error -y "$@" "$OUT/$name"
}

echo "Generating AC-3 samples in $OUT"

# A plain 48 kHz stereo tone: the common case, two channels, no coupling.
gen ac3_stereo.ac3 -f lavfi -i "sine=frequency=440:duration=1.5:sample_rate=48000" \
    -ac 2 -c:a ac3 -b:a 192k

# 5.1 at 44.1 kHz. Exercises coupling, the LFE channel and the widest
# per-channel bookkeeping the bit allocator has to do.
gen ac3_51.ac3 -f lavfi -i "sine=frequency=220:duration=1.5:sample_rate=44100" \
    -af "pan=5.1|c0=c0|c1=c0|c2=c0|c3=c0|c4=c0|c5=c0" -c:a ac3 -b:a 448k

# Mono, so that a desynchronised block cannot hide behind a second channel.
gen ac3_mono.ac3 -f lavfi -i "sine=frequency=1000:duration=1.5:sample_rate=48000" \
    -ac 1 -c:a ac3 -b:a 96k

# Digital silence. Every exponent saturates and the allocator is handed the
# degenerate case, which is where an off-by-one in the bit budget shows up
# most clearly.
gen ac3_silence.ac3 -f lavfi -i "anullsrc=r=48000:cl=mono" -t 1.5 -c:a ac3 -b:a 96k

# The lowest bitrate the encoder will take for one channel. Frames are short
# enough that the six audio blocks must land within a few bits of the frame
# end, so any surplus or shortfall is immediately visible.
gen ac3_lowrate.ac3 -f lavfi -i "sine=frequency=440:duration=1.5:sample_rate=48000" \
    -ac 1 -c:a ac3 -b:a 32k

# E-AC-3 (A/52 Annex E): a different frame layout reached through the same
# bsid field, used to check that the two are told apart before parsing.
gen eac3_stereo.eac3 -f lavfi -i "sine=frequency=440:duration=1.5:sample_rate=48000" \
    -ac 2 -c:a eac3 -b:a 192k

# Reference PCM for each, so a decoder change can be diffed against what the
# oracle believes the stream decodes to.
for f in ac3_stereo ac3_51 ac3_mono ac3_silence ac3_lowrate; do
    ext=ac3
    echo "  $f.ref.wav"
    "$FFMPEG" -loglevel error -y -i "$OUT/$f.$ext" -c:a pcm_s16le "$OUT/$f.ref.wav"
done

echo "Done. $(ls -1 "$OUT"/*.ac3 "$OUT"/*.eac3 2>/dev/null | wc -l) streams generated."
