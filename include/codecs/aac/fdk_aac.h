/*
 * fdk_aac.h - Minimal C wrapper around the FDK-AAC decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * FDK decodes the whole AAC family -- LC, HE-AACv1, HE-AACv2 and xHE-AAC
 * (MPEG-D USAC) -- through one decoder, so one wrapper serves all of them.
 * The FDK headers are confined to fdk_aac.cpp, which is built as its own
 * object, so that <fdk-aac/FDK_audio.h> never reaches the rest of the player
 * (and, with --enable-final, the single translation unit the player becomes).
 */

#ifndef FDK_AAC_H
#define FDK_AAC_H

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes. Negative values are errors. */
#define PSYMP3_FDK_OK              0
#define PSYMP3_FDK_NEED_MORE_DATA  1
#define PSYMP3_FDK_ERROR          (-1)

/*
 * MPEG-4 audio object types, as reported by psymp3_fdk_decode. These are
 * defined by the standard rather than by FDK, so naming them here costs
 * nothing and keeps the FDK headers out of the caller.
 */
#define PSYMP3_FDK_AOT_AAC_MAIN    1
#define PSYMP3_FDK_AOT_AAC_LC      2
#define PSYMP3_FDK_AOT_AAC_SSR     3
#define PSYMP3_FDK_AOT_AAC_LTP     4
#define PSYMP3_FDK_AOT_SBR         5
#define PSYMP3_FDK_AOT_ER_AAC_LD  23
#define PSYMP3_FDK_AOT_PS         29
#define PSYMP3_FDK_AOT_ER_AAC_ELD 39
#define PSYMP3_FDK_AOT_USAC       42

/** Open a decoder for raw access units. NULL on failure. */
void* psymp3_fdk_open(void);

/** Feed the AudioSpecificConfig from the container. */
int psymp3_fdk_configure(void* handle, const unsigned char* asc, unsigned asc_len);

/**
 * Set the loudness normalisation target, in dBFS (e.g. -16).
 *
 * Only xHE-AAC wants this: it carries programme loudness metadata and expects
 * the decoder to normalise, so without it those tracks sit at a different
 * level from the rest of a library. Plain AAC-LC/HE-AAC is left alone, since
 * applying it there would change levels relative to every other decoder.
 */
int psymp3_fdk_set_target_loudness(void* handle, int target_dbfs);

/**
 * Decode one access unit.
 *
 * out receives interleaved 16-bit samples; out_capacity is in samples, not
 * frames. On PSYMP3_FDK_OK, *frame_size is the per-channel frame length and
 * *rate / *channels describe the output.
 *
 * *aot is the core object type and *ext_aot the extension one -- SBR or PS
 * when either is present. Both are only known after a frame has decoded,
 * because HE-AACv1's SBR and HE-AACv2's Parametric Stereo are usually
 * signalled implicitly and are discovered in the bitstream rather than
 * declared in the AudioSpecificConfig. Any of these out-parameters may be
 * NULL.
 */
int psymp3_fdk_decode(void* handle,
                      const unsigned char* packet, unsigned packet_len,
                      short* out, int out_capacity,
                      int* frame_size, int* rate, int* channels,
                      int* aot, int* ext_aot);

/** Drop decoder history without discarding the configuration (for seeks). */
void psymp3_fdk_reset(void* handle);

/** Close and free. Safe on NULL. */
void psymp3_fdk_close(void* handle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FDK_AAC_H
