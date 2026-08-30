/*
 * stb_vorbis_impl.c - builds the vendored stb_vorbis decoder as a C object
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * stb_vorbis defines collision-prone global typedefs and macros (uint8,
 * TRUE/FALSE, CHECK, a static function named `error`), so unlike kjmp2 it is
 * NOT #included into a C++ translation unit: it gets its own C object here,
 * and C++ callers see only the declaration block via STB_VORBIS_HEADER_ONLY.
 * The --enable-final unity build compiles this same file alongside
 * psymp3.final.cpp.
 */

/* Pushdata is the only API the codec uses; this also strips stdio, seeking,
 * and the integer-conversion helpers (which error out in push mode anyway). */
#define STB_VORBIS_NO_PULLDATA_API

/* Vorbis I permits up to 255 channels and the replaced libvorbis decoder had
 * no cap; stb's default of 16 would make VorbisCodec::canDecode() a liar. */
#define STB_VORBIS_MAX_CHANNELS 255

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wshadow"
/* clang trips -Wtautological-compare on stb's defensive pointer checks */
#pragma GCC diagnostic ignored "-Wtautological-compare"
#endif

#include "../../../third_party/stb/stb_vorbis.c"
