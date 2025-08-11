/*
 * ALawCodec.h - A-law (G.711 A-law) audio codec
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

#ifndef ALAWCODEC_H
#define ALAWCODEC_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief A-law (G.711 A-law) audio codec
 * 
 * Decodes A-law compressed audio data into 16-bit PCM samples according to
 * ITU-T G.711 specification. Used primarily for European telephony
 * systems and international telecommunications.
 * 
 * Features:
 * - ITU-T G.711 A-law compliant decoding
 * - Lookup table-based conversion for optimal performance
 * - Support for 8 kHz telephony standard and other sample rates
 * - Proper handling of A-law closest-to-silence encoding (0x55 maps to -8)
 * - Multi-channel support with sample interleaving
 */
class ALawCodec : public SimplePCMCodec {
public:
    /**
     * @brief Construct A-law codec with stream information
     * @param stream_info Stream information containing format parameters
     */
    explicit ALawCodec(const StreamInfo& stream_info);
    
    /**
     * @brief Check if this codec can decode the given stream
     * @param stream_info Stream information to validate
     * @return true if stream contains A-law audio data
     */
    bool canDecode(const StreamInfo& stream_info) const override;
    
    /**
     * @brief Get codec name identifier
     * @return "alaw" codec name
     */
    std::string getCodecName() const override;
    
    /**
     * @brief Initialize codec with enhanced error handling
     * @return true if initialization successful
     */
    bool initialize() override;
    
    /**
     * @brief Decode audio chunk with comprehensive error handling
     * @param chunk Input data chunk from demuxer
     * @return Decoded audio frame, or empty frame on error
     */
    AudioFrame decode(const MediaChunk& chunk) override;

protected:
    /**
     * @brief Convert A-law samples to 16-bit PCM using lookup table
     * @param input_data Raw A-law encoded data
     * @param output_samples Output vector to fill with 16-bit PCM samples
     * @return Number of samples converted
     */
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) override;
    
    /**
     * @brief Get number of bytes per A-law sample
     * @return 1 (A-law uses 8-bit samples)
     */
    size_t getBytesPerInputSample() const override;

private:
    /**
     * @brief A-law to 16-bit PCM conversion lookup table
     * 
     * Static table shared across all codec instances for memory efficiency.
     * Contains 256 entries mapping each possible A-law value to its
     * corresponding 16-bit signed PCM sample according to ITU-T G.711.
     */
    static const int16_t ALAW_TO_PCM[256];
    
    /**
     * @brief Initialize A-law lookup table
     * 
     * Called once to populate the conversion table with ITU-T G.711
     * compliant values. Uses static initialization to ensure thread-safe
     * one-time setup. Includes error handling for initialization failures.
     */
    static void initializeALawTable();
    
    /**
     * @brief Flag to track table initialization
     */
    static bool s_table_initialized;
};

#ifdef ENABLE_ALAW_CODEC
/**
 * @brief Register A-law codec with AudioCodecFactory
 * 
 * Registers the codec for multiple format identifiers:
 * - "alaw" - Primary identifier
 * - "pcm_alaw" - Alternative identifier for PCM A-law
 * - "g711_alaw" - ITU-T G.711 A-law identifier
 */
void registerALawCodec();
#endif

#endif // ALAWCODEC_H