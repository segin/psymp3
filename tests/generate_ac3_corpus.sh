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
# Each stream is encoded exactly once, as a raw elementary stream, and then
# stream-copied into every container. So every container holds bit-identical
# syncframes, and a decode through any demuxer must match the raw decode
# sample for sample -- a difference is a demuxer bug, never an encoder one.
# The script checks that property itself before it finishes.
#
# Layout under the output directory:
#   raw/       .ac3 / .eac3 elementary streams
#   avi/       RIFF AVI, AC-3 as WAVE_FORMAT_DOLBY_AC3 (0x2000)
#   wav/       RIFF WAVE, same format tag
#   mp4/       ISO base media, 'ac-3' / 'ec-3' sample entries
#   m4a/       ISO base media with the M4A brand (AC-3 only, see below)
#   mka/       Matroska, A_AC3 / A_EAC3
#   ref/       ffmpeg's decode of each raw stream, s16le WAV
#
# E-AC-3 is left out of both RIFF containers: it has no registered RIFF
# format tag, and ffmpeg writes 0 (WAVE_FORMAT_UNKNOWN), so such a file would
# test a mapping nothing else produces. ffmpeg's M4A muxer refuses E-AC-3.
#
# The generated files are not checked in; this reproduces them exactly.

set -e

OUT="${1:-tests/data/ac3}"
FFMPEG="${FFMPEG:-ffmpeg}"

if ! command -v "$FFMPEG" >/dev/null 2>&1; then
    echo "ERROR: $FFMPEG not found; it generates the sample streams." >&2
    echo "       Set FFMPEG=/path/to/ffmpeg if it lives elsewhere." >&2
    exit 1
fi

mkdir -p "$OUT"/raw "$OUT"/avi "$OUT"/wav "$OUT"/mp4 "$OUT"/m4a "$OUT"/mka "$OUT"/ref

ff() { "$FFMPEG" -nostdin -loglevel error -y "$@"; }

# encode NAME CODEC SOURCE-ARGS... -- one raw elementary stream.
encode() {
    local name="$1" codec="$2"; shift 2
    echo "  raw/$name.$codec"
    ff "$@" -c:a "$codec" -f "$codec" "$OUT/raw/$name.$codec"
}

echo "Encoding elementary streams in $OUT/raw"

# A plain 48 kHz stereo tone: the common case, two channels, no coupling.
encode ac3_stereo ac3 -f lavfi -i "sine=frequency=440:duration=1.5:sample_rate=48000" \
    -ac 2 -b:a 192k

# 5.1 at 44.1 kHz. Exercises coupling, the LFE channel and the widest
# per-channel bookkeeping the bit allocator has to do. Each channel gets its
# own tone so a channel-order mistake is audible and measurable, not masked
# by six copies of the same signal.
encode ac3_51 ac3 -f lavfi -i "aevalsrc=sin(2*PI*220*t)|sin(2*PI*330*t)|sin(2*PI*440*t)|sin(2*PI*55*t)|sin(2*PI*550*t)|sin(2*PI*660*t):s=44100:d=1.5:c=5.1" \
    -b:a 448k

# Mono, so that a desynchronised block cannot hide behind a second channel.
encode ac3_mono ac3 -f lavfi -i "sine=frequency=1000:duration=1.5:sample_rate=48000" \
    -ac 1 -b:a 96k

# Digital silence. Every exponent saturates and the allocator is handed the
# degenerate case, which is where an off-by-one in the bit budget shows up
# most clearly.
encode ac3_silence ac3 -f lavfi -i "anullsrc=r=48000:cl=mono" -t 1.5 -b:a 96k

# The lowest bitrate the encoder will take for one channel. Frames are short
# enough that the six audio blocks must land within a few bits of the frame
# end, so any surplus or shortfall is immediately visible.
encode ac3_lowrate ac3 -f lavfi -i "sine=frequency=440:duration=1.5:sample_rate=48000" \
    -ac 1 -b:a 32k

# E-AC-3 (A/52 Annex E): a different frame layout reached through the same
# bsid field, used to check that the two are told apart before parsing.
encode eac3_stereo eac3 -f lavfi -i "sine=frequency=440:duration=1.5:sample_rate=48000" \
    -ac 2 -b:a 192k
encode eac3_51 eac3 -f lavfi -i "aevalsrc=sin(2*PI*220*t)|sin(2*PI*330*t)|sin(2*PI*440*t)|sin(2*PI*55*t)|sin(2*PI*550*t)|sin(2*PI*660*t):s=48000:d=1.5:c=5.1" \
    -b:a 384k

AC3_STREAMS="ac3_stereo ac3_51 ac3_mono ac3_silence ac3_lowrate"
EAC3_STREAMS="eac3_stereo eac3_51"

# wrap NAME CODEC DIR EXT MUXER -- stream-copy a raw stream into a container.
wrap() {
    local name="$1" codec="$2" dir="$3" ext="$4" muxer="$5"
    echo "  $dir/$name.$ext"
    ff -i "$OUT/raw/$name.$codec" -c:a copy -f "$muxer" "$OUT/$dir/$name.$ext"
}

echo "Wrapping into containers"
for s in $AC3_STREAMS; do
    wrap "$s" ac3 avi avi avi
    wrap "$s" ac3 wav wav wav
    wrap "$s" ac3 mp4 mp4 mp4
    wrap "$s" ac3 m4a m4a ipod
    wrap "$s" ac3 mka mka matroska
done
for s in $EAC3_STREAMS; do
    wrap "$s" eac3 mp4 mp4 mp4
    wrap "$s" eac3 mka mka matroska
done

# Reference PCM, from the raw streams only: the containers carry the same
# frames, so one reference per stream covers every container it is in.
echo "Decoding references"
for s in $AC3_STREAMS; do
    echo "  ref/$s.wav"
    ff -i "$OUT/raw/$s.ac3" -c:a pcm_s16le "$OUT/ref/$s.wav"
done
for s in $EAC3_STREAMS; do
    echo "  ref/$s.wav"
    ff -i "$OUT/raw/$s.eac3" -c:a pcm_s16le "$OUT/ref/$s.wav"
done

# Check the property the whole layout rests on: pull the frames back out of
# every container and compare them with the raw stream byte for byte.
echo "Verifying every container carries the raw stream unchanged"
failed=0
check() {
    local file="$1" codec="$2" raw="$3"
    local tmp
    tmp="$(mktemp)"
    ff -i "$file" -c:a copy -f "$codec" "$tmp"
    if cmp -s "$tmp" "$raw"; then
        echo "  ok    $file"
    else
        echo "  DIFF  $file" >&2
        failed=1
    fi
    rm -f "$tmp"
}
for s in $AC3_STREAMS; do
    for c in avi/$s.avi wav/$s.wav mp4/$s.mp4 m4a/$s.m4a mka/$s.mka; do
        check "$OUT/$c" ac3 "$OUT/raw/$s.ac3"
    done
done
for s in $EAC3_STREAMS; do
    for c in mp4/$s.mp4 mka/$s.mka; do
        check "$OUT/$c" eac3 "$OUT/raw/$s.eac3"
    done
done

if [ "$failed" -ne 0 ]; then
    echo "ERROR: a container did not preserve its stream; see DIFF above." >&2
    exit 1
fi

count=$(find "$OUT" -type f ! -path "$OUT/ref/*" | wc -l)
echo "Done. $count stream files and $(ls -1 "$OUT"/ref | wc -l) references in $OUT."
