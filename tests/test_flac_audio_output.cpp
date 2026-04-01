#include "psymp3.h"

/*
 * test_flac_audio_output.cpp - FLAC codec rejection test for invalid synthetic data
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * The previous version of this test hand-built an invalid FLAC frame with
 * placeholder CRC values and then expected the decoder to emit audio. That was
 * not a technically correct playback test. This regression now verifies the
 * stricter native decoder behavior instead: malformed synthetic FLAC data must
 * be rejected cleanly without fabricating output.
 */

int main() {
    std::cout << "Testing FLAC rejection of invalid synthetic frame..." << std::endl;

    try {
        std::vector<uint8_t> flac_data;

        // FLAC signature.
        flac_data.insert(flac_data.end(), {'f', 'L', 'a', 'C'});

        // STREAMINFO metadata block (marked last).
        flac_data.push_back(0x80);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x22);

        // STREAMINFO payload.
        flac_data.push_back(0x10);
        flac_data.push_back(0x00);
        flac_data.push_back(0x10);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0xAC);
        flac_data.push_back(0x44);
        flac_data.push_back(0x11);
        flac_data.push_back(0xF0);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0xAC);
        flac_data.push_back(0x44);
        for (int i = 0; i < 16; i++) {
            flac_data.push_back(0x00);
        }

        // Intentionally malformed frame: placeholder header/footer CRC and a
        // synthetic payload that does not match a valid encoded FLAC frame.
        flac_data.push_back(0xFF);
        flac_data.push_back(0xF8);
        flac_data.push_back(0x69);
        flac_data.push_back(0x02);
        flac_data.push_back(0x00);
        flac_data.push_back(0x0F);
        flac_data.push_back(0xFF);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);

        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_samples = 44100;

        FLACCodec codec(stream_info);
        if (!codec.initialize()) {
            std::cerr << "ERROR: Failed to initialize FLAC codec" << std::endl;
            return 1;
        }

        MediaChunk chunk(1, flac_data);
        AudioFrame frame = codec.decode(chunk);

        if (!frame.samples.empty()) {
            std::cerr << "ERROR: Decoder produced audio for malformed FLAC input" << std::endl;
            return 1;
        }

        if (frame.channels != 0 || frame.sample_rate != 0) {
            std::cerr << "ERROR: Decoder returned non-empty frame metadata for malformed FLAC input" << std::endl;
            return 1;
        }

        std::cout << "Malformed synthetic FLAC was rejected cleanly" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Exception during test: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ERROR: Unknown exception during test" << std::endl;
        return 1;
    }
}
