/*
 * xhe_fdk.cpp - FDK-AAC USAC decoder wrapper (isolated translation unit)
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This file deliberately does NOT include psymp3.h: that would pull in
 * faad2's <neaacdec.h>, which defines ID_SCE/ID_CPE/ID_LFE and other symbols
 * that also come from FDK's <fdk-aac/FDK_audio.h>. Keeping the FDK headers
 * alone in their own object is what lets both AAC decoders coexist, including
 * in the --enable-final unity build.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_XHE_AAC

#include "codecs/aac/xhe_fdk.h"

#include <fdk-aac/aacdecoder_lib.h>
#include <cstddef>

// The wrapper hands back 16-bit samples; FDK must be built to match.
static_assert(sizeof(INT_PCM) == 2, "PsyMP3 expects an FDK-AAC built for 16-bit output");

extern "C" {

void* psymp3_xhe_open(void)
{
    // TT_MP4_RAW: bare access units, configuration supplied out of band from
    // the esds rather than in ADTS/LOAS framing.
    return aacDecoder_Open(TT_MP4_RAW, 1);
}

int psymp3_xhe_configure(void* handle, const unsigned char* asc, unsigned asc_len)
{
    if (!handle || !asc || asc_len == 0) {
        return PSYMP3_XHE_ERROR;
    }
    UCHAR* config[1] = { const_cast<UCHAR*>(asc) };
    UINT config_len[1] = { asc_len };
    AAC_DECODER_ERROR err =
        aacDecoder_ConfigRaw(static_cast<HANDLE_AACDECODER>(handle), config, config_len);
    return (err == AAC_DEC_OK) ? PSYMP3_XHE_OK : PSYMP3_XHE_ERROR;
}

int psymp3_xhe_set_target_loudness(void* handle, int target_dbfs)
{
    if (!handle) {
        return PSYMP3_XHE_ERROR;
    }
    HANDLE_AACDECODER dec = static_cast<HANDLE_AACDECODER>(handle);
    // AAC_DRC_REFERENCE_LEVEL is the target level in steps of -0.25 dB, so
    // -16 dBFS is 64.
    const int level = -4 * target_dbfs;
    bool ok = aacDecoder_SetParam(dec, AAC_DRC_REFERENCE_LEVEL, level) == AAC_DEC_OK;
    // Apply the stream's DRC at full strength, which is what plain playback
    // (as opposed to a limited-range environment) is supposed to do.
    ok = ok && aacDecoder_SetParam(dec, AAC_DRC_BOOST_FACTOR, 127) == AAC_DEC_OK;
    ok = ok && aacDecoder_SetParam(dec, AAC_DRC_ATTENUATION_FACTOR, 127) == AAC_DEC_OK;
    return ok ? PSYMP3_XHE_OK : PSYMP3_XHE_ERROR;
}

int psymp3_xhe_decode(void* handle,
                      const unsigned char* packet, unsigned packet_len,
                      short* out, int out_capacity,
                      int* frame_size, int* rate, int* channels)
{
    if (!handle || !packet || packet_len == 0 || !out || out_capacity <= 0) {
        return PSYMP3_XHE_ERROR;
    }
    HANDLE_AACDECODER dec = static_cast<HANDLE_AACDECODER>(handle);

    UCHAR* buf[1] = { const_cast<UCHAR*>(packet) };
    UINT buf_size[1] = { packet_len };
    UINT valid[1] = { packet_len };
    if (aacDecoder_Fill(dec, buf, buf_size, valid) != AAC_DEC_OK) {
        return PSYMP3_XHE_ERROR;
    }

    AAC_DECODER_ERROR err =
        aacDecoder_DecodeFrame(dec, reinterpret_cast<INT_PCM*>(out),
                               static_cast<INT>(out_capacity), 0);
    if (err == AAC_DEC_NOT_ENOUGH_BITS) {
        return PSYMP3_XHE_NEED_MORE_DATA;
    }
    if (err != AAC_DEC_OK) {
        return PSYMP3_XHE_ERROR;
    }

    CStreamInfo* info = aacDecoder_GetStreamInfo(dec);
    if (!info || info->frameSize <= 0 || info->numChannels <= 0) {
        return PSYMP3_XHE_ERROR;
    }
    if (frame_size) *frame_size = info->frameSize;
    if (rate) *rate = info->sampleRate;
    if (channels) *channels = info->numChannels;
    return PSYMP3_XHE_OK;
}

void psymp3_xhe_reset(void* handle)
{
    if (handle) {
        aacDecoder_SetParam(static_cast<HANDLE_AACDECODER>(handle),
                            AAC_TPDEC_CLEAR_BUFFER, 1);
    }
}

void psymp3_xhe_close(void* handle)
{
    if (handle) {
        aacDecoder_Close(static_cast<HANDLE_AACDECODER>(handle));
    }
}

} // extern "C"

#endif // HAVE_XHE_AAC
