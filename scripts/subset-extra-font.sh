#!/bin/sh
# subset-extra-font.sh - regenerate res/extra.ttf, the bundled CJK fallback font.
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#
# res/extra.ttf is Noto Sans CJK JP Regular (Sans 2.004, SIL OFL 1.1), cut
# down to the CJK ideographs, Kana and Hangul that DejaVu Sans lacks. The
# full face is about 16 MB; this one is about 10 MB. Latin, Greek and
# Cyrillic are dropped because vera.ttf already draws them, and the vertical
# writing and per-language (locl) features are dropped because PsyMP3 sets
# text horizontally and never tags it with a language. The Hangul jamo
# features stay, so old Hangul written as conjoining jamo still composes.
#
# The source is the .ttc from <https://github.com/notofonts/noto-cjk>
# (Sans/OTC/NotoSansCJK-Regular.ttc), which distributions also ship, e.g.
# Arch's noto-fonts-cjk as /usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc.
# Its first face is the JP one. Needs fontTools (pyftsubset). Commit the
# result.
#
# Usage: scripts/subset-extra-font.sh NotoSansCJK-Regular.ttc [out]

set -eu

src=${1:?usage: $0 NotoSansCJK-Regular.ttc [out]}
out=${2:-$(dirname "$0")/../res/extra.ttf}

# CJK: radicals, ideographic description, symbols and punctuation, Bopomofo,
# Kanbun, strokes, enclosed and compatibility characters, the unified
# ideographs and their extensions, compatibility ideographs, vertical and
# compatibility forms, half- and full-width forms, enclosed ideographic
# supplement.
cjk="U+2E80-2FDF,U+2FF0-2FFF,U+3000-303F,U+3100-312F,U+3190-31EF,U+3200-33FF"
cjk="$cjk,U+3400-4DBF,U+4E00-9FFF,U+F900-FAFF,U+FE10-FE1F,U+FE30-FE4F"
cjk="$cjk,U+FF00-FFEF,U+1F200-1F2FF,U+20000-2A6DF,U+2A700-2EBEF,U+2F800-2FA1F"
# Kana: Hiragana, Katakana and its phonetic extensions, Kana supplements.
kana="U+3040-30FF,U+31F0-31FF,U+1B000-1B16F"
# Hangul: Jamo and its extensions, compatibility Jamo, syllables.
hangul="U+1100-11FF,U+3130-318F,U+A960-A97F,U+AC00-D7AF,U+D7B0-D7FF"

pyftsubset "$src" --font-number=0 \
    --unicodes="$cjk,$kana,$hangul" \
    --layout-features='ccmp,ljmo,vjmo,tjmo' \
    --drop-tables+=vhea,vmtx,VORG \
    --output-file="$out"
echo "wrote $out ($(wc -c < "$out") bytes)"
