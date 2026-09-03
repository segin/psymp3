/*
 * mlp_decoder_impl.cpp - translation unit for the bundled MLP/TrueHD decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The decoder this compiles (third_party/mlp) is used under the Apache
 * License, Version 2.0.
 */

// Both the codec and the demuxer use the decoder -- the demuxer decodes the
// first access unit to learn the rate and channel layout -- so it is compiled
// once here rather than being #included into either of them. Keeping the
// wrapper inside src/ keeps the object inside the build tree; the third_party
// source itself cannot be named directly from an automake SOURCES list without
// putting its object above the build root.
//
// This is also why psymp3.final.cpp does not #include it: like
// stb_vorbis_impl.c and xhe_fdk.cpp, it stays its own translation unit in the
// --enable-final build.
#include "../../../third_party/mlp/mlp_decoder.cpp"
