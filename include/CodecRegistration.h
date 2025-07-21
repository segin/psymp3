/*
 * CodecRegistration.h - Centralized codec and demuxer registration functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef CODECREGISTRATION_H
#define CODECREGISTRATION_H

/**
 * @brief Register all available audio codecs with the CodecRegistry
 * 
 * This function contains all conditional compilation logic for codecs.
 * It registers codec factory functions based on compile-time configuration:
 * 
 * Always registered:
 * - PCM codec (8-bit, 16-bit, 24-bit, 32-bit integer and float)
 * - A-law codec (ITU-T G.711)
 * - μ-law codec (ITU-T G.711)
 * 
 * Conditionally registered (based on #ifdef):
 * - Vorbis codec and passthrough (HAVE_VORBIS)
 * - Opus codec and passthrough (HAVE_OPUS)
 * - Ogg FLAC passthrough (HAVE_FLAC && HAVE_OGGDEMUXER)
 * - Speex codec (HAVE_OGGDEMUXER)
 * 
 * Legacy Stream architecture (not registered with CodecRegistry):
 * - MP3 codec (HAVE_MP3) - uses Libmpg123 class
 * - FLAC codec (HAVE_FLAC) - uses Flac class
 * 
 * This function should be called once at application startup before
 * any codec creation is attempted.
 */
void registerAllCodecs();

/**
 * @brief Register all available demuxers with the DemuxerRegistry
 * 
 * This function contains all conditional compilation logic for demuxers.
 * It registers demuxer factory functions based on compile-time configuration:
 * 
 * Always registered:
 * - RIFF demuxer (for WAV, AVI containers) - uses ChunkDemuxer
 * - AIFF demuxer (for AIFF containers) - uses ChunkDemuxer  
 * - MP4/ISO demuxer (for MP4, M4A, MOV containers) - uses ISODemuxer
 * - Raw audio demuxer (for PCM, A-law, μ-law files) - uses RawAudioDemuxer
 * 
 * Conditionally registered (based on #ifdef):
 * - Ogg demuxer (HAVE_VORBIS || HAVE_OPUS || (HAVE_FLAC && HAVE_OGG)) - uses OggDemuxer
 * 
 * Legacy Stream architecture (no demuxer registration needed):
 * - FLAC (HAVE_FLAC) - uses Flac class directly
 * 
 * The Ogg demuxer is only registered if at least one Ogg-compatible
 * codec is available. This ensures proper FLAC codec and demuxer coupling
 * as specified in the requirements.
 * 
 * This function should be called once at application startup before
 * any demuxer creation is attempted.
 */
void registerAllDemuxers();

#endif // CODECREGISTRATION_H