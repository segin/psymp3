/*
 * xhe_fdk.h - Minimal C wrapper around FDK-AAC's USAC decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * FDK-AAC's <fdk-aac/FDK_audio.h> and faad2's <neaacdec.h> both define
 * ID_SCE, ID_CPE, ID_LFE and friends, so the two headers cannot appear in one
 * translation unit -- and with --enable-final the whole player IS one
 * translation unit. This header therefore exposes the decoder through plain
 * C types only; the FDK headers are confined to xhe_fdk.cpp, which is built
 * as its own object (the same treatment stb_vorbis gets, and for the same
 * reason).
 */

#ifndef XHE_FDK_H
#define XHE_FDK_H

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes. Negative values are errors. */
#define PSYMP3_XHE_OK              0
#define PSYMP3_XHE_NEED_MORE_DATA  1
#define PSYMP3_XHE_ERROR          (-1)

/** Open a USAC (xHE-AAC) decoder for raw access units. NULL on failure. */
void* psymp3_xhe_open(void);

/** Feed the AudioSpecificConfig from the container. */
int psymp3_xhe_configure(void* handle, const unsigned char* asc, unsigned asc_len);

/**
 * Set the loudness normalisation target, in dBFS (e.g. -16).
 * xHE-AAC carries programme loudness metadata and expects the decoder to
 * normalise; without this these tracks sit at a different level from the
 * rest of a library.
 */
int psymp3_xhe_set_target_loudness(void* handle, int target_dbfs);

/**
 * Decode one access unit.
 * out receives interleaved 16-bit samples; out_capacity is in samples, not
 * frames. On PSYMP3_XHE_OK, *frame_size is the per-channel frame length and
 * *rate / *channels describe the output.
 */
int psymp3_xhe_decode(void* handle,
                      const unsigned char* packet, unsigned packet_len,
                      short* out, int out_capacity,
                      int* frame_size, int* rate, int* channels);

/** Drop decoder history without discarding the configuration (for seeks). */
void psymp3_xhe_reset(void* handle);

/** Close and free. Safe on NULL. */
void psymp3_xhe_close(void* handle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XHE_FDK_H
