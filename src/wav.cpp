/*
 * wav.cpp - RIFF WAVE format decoder implementation.
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

#include "psymp3.h"

// RIFF FourCC codes (in little-endian)
constexpr uint32_t RIFF_ID = 0x46464952; // "RIFF"
constexpr uint32_t WAVE_ID = 0x45564157; // "WAVE"
constexpr uint32_t FMT_ID  = 0x20746d66; // "fmt "
constexpr uint32_t DATA_ID = 0x61746164; // "data"

// WAVE format tags
constexpr uint16_t WAVE_FORMAT_PCM = 0x0001;
constexpr uint16_t WAVE_FORMAT_MPEGLAYER3 = 0x0055;
constexpr uint16_t WAVE_FORMAT_IEEE_FLOAT = 0x0003;
constexpr uint16_t WAVE_FORMAT_ALAW = 0x0006;
constexpr uint16_t WAVE_FORMAT_MULAW = 0x0007;

/**
 * @brief Helper function to read a little-endian value from the file stream.
 * @tparam T The integer type to read.
 * @param file The input file stream.
 * @return The value read from the stream.
 */
template<typename T>
T read_le(std::ifstream& file) {
    T value;
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

/**
 * @brief Converts an 8-bit A-law sample to a 16-bit linear PCM sample.
 * @param alawbyte The 8-bit A-law encoded sample.
 * @return The 16-bit signed linear PCM sample.
 */
static int16_t alaw2linear(uint8_t alawbyte)
{
	int16_t sign, exponent, mantissa, sample;

	alawbyte ^= 0x55;
	sign = (alawbyte & 0x80);
	exponent = (alawbyte >> 4) & 0x07;
	mantissa = alawbyte & 0x0F;
	if(exponent == 0)
		sample = mantissa << 4;
	else
		sample = ( (mantissa << 4) | 0x100 ) << (exponent - 1);

	if(sign == 0)
		sample = -sample;

	return sample;
}

/**
 * @brief Converts an 8-bit mu-law sample to a 16-bit linear PCM sample.
 * @param ulawbyte The 8-bit mu-law encoded sample.
 * @return The 16-bit signed linear PCM sample.
 */
static int16_t ulaw2linear(uint8_t ulawbyte)
{
	static int exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
	int sign, exponent, mantissa, sample;
	ulawbyte = ~ulawbyte;
	sign = (ulawbyte & 0x80);
	exponent = (ulawbyte >> 4) & 0x07;
	mantissa = ulawbyte & 0x0F;
	sample = exp_lut[exponent] + (mantissa << (exponent + 3));
	return sign ? -sample : sample;
}

/**
 * @brief Constructs a WaveStream object from a file path.
 *
 * This opens the file and immediately calls parseHeaders() to validate the
 * RIFF/WAVE format and extract stream information like sample rate, channels,
 * and the location of the audio data chunk.
 * @param path The file path of the WAVE file to open.
 * @throws InvalidMediaException if the file cannot be opened.
 */
WaveStream::WaveStream(const TagLib::String& path) : Stream(path) {
    m_file.open(path.toCString(true), std::ios::binary);
    if (!m_file.is_open()) {
        throw InvalidMediaException("WaveStream: Could not open file.");
    }
    try {
        parseHeaders();
    } catch (...) {
        m_file.close();
        throw; // Re-throw the exception after closing the file.
    }
}

/**
 * @brief Destroys the WaveStream object.
 *
 * Ensures that the file handle is properly closed if it was opened.
 */
WaveStream::~WaveStream() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

/**
 * @brief Parses the RIFF/WAVE file headers.
 *
 * This method reads through the file's chunks to find the 'fmt ' and 'data' chunks, extracting essential metadata and validating the audio format.
 * @throws WrongFormatException if the file is not a valid RIFF/WAVE file.
 * @throws BadFormatException if the WAVE format is unsupported or the file is malformed.
 */
void WaveStream::parseHeaders() {
    if (read_le<uint32_t>(m_file) != RIFF_ID) throw WrongFormatException("Not a RIFF file.");
    read_le<uint32_t>(m_file); // Overall file size, unused.
    if (read_le<uint32_t>(m_file) != WAVE_ID) throw WrongFormatException("Not a WAVE file.");

    bool found_fmt = false;

    while (!m_file.eof()) {
        uint32_t chunk_id = read_le<uint32_t>(m_file);
        uint32_t chunk_size = read_le<uint32_t>(m_file);
        if (m_file.eof()) break;

        std::streampos chunk_start_pos = m_file.tellg();

        if (chunk_id == FMT_ID) {
            uint16_t format_tag = read_le<uint16_t>(m_file);
            if (format_tag == WAVE_FORMAT_PCM) m_encoding = WaveEncoding::PCM;
            else if (format_tag == WAVE_FORMAT_IEEE_FLOAT) m_encoding = WaveEncoding::IEEE_FLOAT;
            else if (format_tag == WAVE_FORMAT_ALAW) m_encoding = WaveEncoding::ALAW;
            else if (format_tag == WAVE_FORMAT_MULAW) m_encoding = WaveEncoding::MULAW;
            else if (format_tag == WAVE_FORMAT_MPEGLAYER3) throw BadFormatException("MP3 in WAVE container is not yet supported.");
            // For any other format, we throw WrongFormatException to allow other decoders to try.
            else throw WrongFormatException("Unsupported WAVE format tag: " + std::to_string(format_tag));

            m_channels = read_le<uint16_t>(m_file);
            m_rate = read_le<uint32_t>(m_file);
            m_file.seekg(6, std::ios_base::cur); // Skip ByteRate and BlockAlign
            m_bits_per_sample = read_le<uint16_t>(m_file);

            switch (m_encoding) {
                case WaveEncoding::PCM:
                    if (m_bits_per_sample != 8 && m_bits_per_sample != 16 && m_bits_per_sample != 24)
                        throw BadFormatException("Unsupported PCM bit depth.");
                    break;
                case WaveEncoding::IEEE_FLOAT:
                    if (m_bits_per_sample != 32) throw BadFormatException("Only 32-bit IEEE Float is supported.");
                    break;
                case WaveEncoding::ALAW:
                case WaveEncoding::MULAW:
                    if (m_bits_per_sample != 8) throw BadFormatException("A-law/mu-law must be 8-bit.");
                    break;
                default:
                    throw BadFormatException("Unsupported WAVE format.");
            }
            m_bytes_per_sample = m_bits_per_sample / 8;
            found_fmt = true;
        } else if (chunk_id == DATA_ID) {
            if (!found_fmt) throw BadFormatException("WAVE 'data' chunk found before 'fmt ' chunk.");
            m_data_chunk_offset = chunk_start_pos;
            m_data_chunk_size = chunk_size;
            m_slength = m_data_chunk_size / (m_channels * m_bytes_per_sample);
            m_length = (m_slength * 1000) / m_rate;
            m_file.seekg(m_data_chunk_offset); // Position at start of data
            return; // Found both, we're done parsing.
        }

        // Seek to the next chunk, accounting for padding byte if chunk size is odd.
        m_file.seekg(chunk_start_pos + static_cast<std::streamoff>(chunk_size));
        if (chunk_size % 2 != 0) {
            m_file.seekg(1, std::ios_base::cur);
        }
    }
    throw BadFormatException("WAVE file is missing 'data' chunk.");
}

/**
 * @brief Reads decoded audio data from the WAVE stream.
 *
 * This method reads the raw sample data from the file's 'data' chunk. It performs
 * on-the-fly conversion from the source format (e.g., 8-bit, 24-bit, 32-bit float,
 * A-law, mu-law) into the standard 16-bit signed PCM format required by the
 * application's audio pipeline.
 * @param len The number of bytes requested for the output buffer.
 * @param buf A pointer to the buffer where the 16-bit PCM data should be stored.
 * @return The total number of bytes actually written into the buffer.
 */
size_t WaveStream::getData(size_t len, void *buf) {
    if (eof()) return 0;

    // If the source is already 16-bit, we can perform a direct, efficient read.
    if (m_encoding == WaveEncoding::PCM && m_bits_per_sample == 16) {
        size_t bytes_to_read = std::min(len, static_cast<size_t>(m_data_chunk_size - m_bytes_read_from_data));
        m_file.read(static_cast<char*>(buf), bytes_to_read);
        size_t bytes_read = m_file.gcount();
        m_bytes_read_from_data += bytes_read;
        m_sposition = m_bytes_read_from_data / (m_channels * m_bytes_per_sample);
        m_position = (m_sposition * 1000) / m_rate;
        return bytes_read;
    }

    // For other formats, we must convert to 16-bit.
    // Calculate how many source bytes we need to read to fill the output buffer.
    size_t max_output_samples = len / 2; // Output buffer expects 16-bit (2-byte) samples.
    size_t source_bytes_to_read = max_output_samples * m_bytes_per_sample;
    source_bytes_to_read = std::min(source_bytes_to_read, static_cast<size_t>(m_data_chunk_size - m_bytes_read_from_data));
    if (source_bytes_to_read == 0) return 0;

    // Read the raw source data into a temporary buffer.
    std::vector<char> source_buffer(source_bytes_to_read);
    m_file.read(source_buffer.data(), source_bytes_to_read);
    size_t actual_source_bytes_read = m_file.gcount();
    if (actual_source_bytes_read == 0) return 0;

    // Pointers for conversion.
    auto* out_ptr = static_cast<int16_t*>(buf);
    const char* in_ptr = source_buffer.data();
    size_t samples_to_convert = actual_source_bytes_read / m_bytes_per_sample;

    switch (m_encoding) {
        case WaveEncoding::PCM:
            if (m_bits_per_sample == 8) {
                for (size_t i = 0; i < samples_to_convert; ++i) *out_ptr++ = (static_cast<int16_t>(static_cast<uint8_t>(*in_ptr++)) - 128) << 8;
            } else if (m_bits_per_sample == 24) {
                for (size_t i = 0; i < samples_to_convert; ++i) {
                    int32_t s24_sample = (static_cast<int8_t>(in_ptr[2]) << 16) | (static_cast<uint8_t>(in_ptr[1]) << 8) | static_cast<uint8_t>(in_ptr[0]);
                    in_ptr += 3;
                    *out_ptr++ = static_cast<int16_t>(s24_sample >> 8);
                }
            }
            break;
        case WaveEncoding::IEEE_FLOAT:
            if (m_bits_per_sample == 32) {
                const float* float_in_ptr = reinterpret_cast<const float*>(source_buffer.data());
                for (size_t i = 0; i < samples_to_convert; ++i) {
                    float sample = std::clamp(*float_in_ptr++, -1.0f, 1.0f);
                    *out_ptr++ = static_cast<int16_t>(sample * 32767.0f);
                }
            }
            break;
        case WaveEncoding::ALAW:
            for (size_t i = 0; i < samples_to_convert; ++i) *out_ptr++ = alaw2linear(static_cast<uint8_t>(*in_ptr++));
            break;
        case WaveEncoding::MULAW:
            for (size_t i = 0; i < samples_to_convert; ++i) *out_ptr++ = ulaw2linear(static_cast<uint8_t>(*in_ptr++));
            break;
        case WaveEncoding::Unsupported:
            // Should not happen if parseHeaders is correct.
            break;
    }

    size_t bytes_written = samples_to_convert * 2; // We wrote 16-bit (2-byte) samples.
    m_bytes_read_from_data += actual_source_bytes_read;
    m_sposition = m_bytes_read_from_data / (m_channels * m_bytes_per_sample);
    m_position = (m_sposition * 1000) / m_rate;

    return bytes_written;
}

/**
 * @brief Seeks to a specific time within the WAVE stream.
 *
 * Calculates the byte offset corresponding to the given time in milliseconds and
 * repositions the file stream's read pointer.
 * @param pos The target position to seek to, in milliseconds.
 */
void WaveStream::seekTo(unsigned long pos) {
    unsigned long long sample_pos = (static_cast<unsigned long long>(pos) * m_rate) / 1000;
    unsigned long long byte_pos = sample_pos * m_channels * m_bytes_per_sample;

    if (byte_pos > m_data_chunk_size) {
        byte_pos = m_data_chunk_size;
    }

    m_file.clear(); // Clear any EOF flags
    m_file.seekg(m_data_chunk_offset + byte_pos);
    m_bytes_read_from_data = byte_pos;
    m_sposition = sample_pos;
    m_position = pos;
}

/**
 * @brief Checks if the end of the audio data chunk has been reached.
 * @return `true` if all data from the 'data' chunk has been read, `false` otherwise.
 */
bool WaveStream::eof() {
    return m_bytes_read_from_data >= m_data_chunk_size;
}