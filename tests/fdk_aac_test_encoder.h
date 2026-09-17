/*
 * fdk_aac_test_encoder.h - AAC streams for tests, made with FDK's encoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The encoder ships in the same library PsyMP3 decodes AAC with, so tests
 * that include this need nothing more than the AAC build does.
 */

#ifndef FDK_AAC_TEST_ENCODER_H
#define FDK_AAC_TEST_ENCODER_H

#include <fdk-aac/aacenc_lib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace FdkTestEncoder {

constexpr int kInputRate = 44100;

/// Raw access units and the AudioSpecificConfig FDK wrote for them.
struct Encoded {
    std::vector<uint8_t> asc;
    std::vector<std::vector<uint8_t>> units;
};

/// @p total_ms of 44.1 kHz audio -- silence, then a 1 kHz tone from
/// @p tone_start_ms -- as @p aot: 2 is AAC-LC, 5 HE-AAC and 29 HE-AACv2. The
/// signalling is implicit, so for HE-AAC the config names only the AAC-LC
/// core at half the rate, as most such files do. Empty on failure.
inline Encoded encode(int aot, int channels, int tone_start_ms, int total_ms)
{
    Encoded out;
    HANDLE_AACENCODER encoder = nullptr;
    if (aacEncOpen(&encoder, 0, channels) != AACENC_OK) {
        return out;
    }
    bool ok = aacEncoder_SetParam(encoder, AACENC_AOT, aot) == AACENC_OK
           && aacEncoder_SetParam(encoder, AACENC_SAMPLERATE, kInputRate) == AACENC_OK
           && aacEncoder_SetParam(encoder, AACENC_CHANNELMODE, channels == 1 ? MODE_1 : MODE_2) == AACENC_OK
           && aacEncoder_SetParam(encoder, AACENC_BITRATE, aot == 2 ? 96000 : 32000) == AACENC_OK
           && aacEncoder_SetParam(encoder, AACENC_TRANSMUX, TT_MP4_RAW) == AACENC_OK
           && aacEncoder_SetParam(encoder, AACENC_SIGNALING_MODE, 0) == AACENC_OK
           && aacEncEncode(encoder, nullptr, nullptr, nullptr, nullptr) == AACENC_OK;
    AACENC_InfoStruct info{};
    ok = ok && aacEncInfo(encoder, &info) == AACENC_OK;
    if (ok) {
        out.asc.assign(info.confBuf, info.confBuf + info.confSize);
    }

    const int total = kInputRate * total_ms / 1000;
    const int tone_from = kInputRate * tone_start_ms / 1000;
    std::vector<INT_PCM> pcm(static_cast<size_t>(info.frameLength) * static_cast<size_t>(channels));
    std::vector<UCHAR> buffer(1 << 16);
    for (int position = 0; ok;) {
        int frames = 0;
        if (position < total) {
            frames = std::min<int>(static_cast<int>(info.frameLength), total - position);
            for (int i = 0; i < frames; ++i) {
                const int t = position + i;
                const double value = t < tone_from ? 0.0 : 16000.0 * std::sin(2.0 * M_PI * 1000.0 * t / kInputRate);
                for (int c = 0; c < channels; ++c) {
                    pcm[static_cast<size_t>(i * channels + c)] = static_cast<INT_PCM>(value);
                }
            }
            position += frames;
        }
        void* in_ptr = pcm.data();
        void* out_ptr = buffer.data();
        INT in_id = IN_AUDIO_DATA;
        INT in_size = frames * channels * static_cast<INT>(sizeof(INT_PCM));
        INT in_el = sizeof(INT_PCM);
        INT out_id = OUT_BITSTREAM_DATA;
        INT out_size = static_cast<INT>(buffer.size());
        INT out_el = 1;
        AACENC_BufDesc in_desc{};
        in_desc.numBufs = 1;
        in_desc.bufs = &in_ptr;
        in_desc.bufferIdentifiers = &in_id;
        in_desc.bufSizes = &in_size;
        in_desc.bufElSizes = &in_el;
        AACENC_BufDesc out_desc{};
        out_desc.numBufs = 1;
        out_desc.bufs = &out_ptr;
        out_desc.bufferIdentifiers = &out_id;
        out_desc.bufSizes = &out_size;
        out_desc.bufElSizes = &out_el;
        AACENC_InArgs in_args{};
        in_args.numInSamples = frames > 0 ? frames * channels : -1;
        AACENC_OutArgs out_args{};
        const AACENC_ERROR error = aacEncEncode(encoder, &in_desc, &out_desc, &in_args, &out_args);
        if (error == AACENC_ENCODE_EOF) {
            break;
        }
        if (error != AACENC_OK) {
            ok = false;
            break;
        }
        if (out_args.numOutBytes > 0) {
            out.units.emplace_back(buffer.begin(), buffer.begin() + out_args.numOutBytes);
        }
    }
    aacEncClose(&encoder);
    if (!ok) {
        out = Encoded{};
    }
    return out;
}

} // namespace FdkTestEncoder

#endif // FDK_AAC_TEST_ENCODER_H
