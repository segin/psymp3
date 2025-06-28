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
#include "wav.h"

// RIFF FourCC codes (in little-endian)
constexpr uint32_t RIFF_ID = 0x46464952; // "RIFF"
constexpr uint32_t WAVE_ID = 0x45564157; // "WAVE"
constexpr uint32_t FMT_ID  = 0x20746d66; // "fmt "
constexpr uint32_t DATA_ID = 0x61746164; // "data"

// WAVE format tags
constexpr uint16_t WAVE_FORMAT_PCM = 0x0001;
constexpr uint16_t WAVE_FORMAT_MPEGLAYER3 = 0x0055;

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

WaveStream::~WaveStream() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

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
            if (format_tag == WAVE_FORMAT_MPEGLAYER3) {
                throw BadFormatException("MP3 in WAVE container is not yet supported.");
            }
            if (format_tag != WAVE_FORMAT_PCM) {
                throw BadFormatException("Unsupported WAVE format: not LPCM.");
            }

            m_channels = read_le<uint16_t>(m_file);
            m_rate = read_le<uint32_t>(m_file);
            m_file.seekg(6, std::ios_base::cur); // Skip ByteRate and BlockAlign
            uint16_t bits_per_sample = read_le<uint16_t>(m_file);

            if (bits_per_sample != 16) {
                throw BadFormatException("Unsupported WAVE format: only 16-bit LPCM is supported.");
            }
            found_fmt = true;
        } else if (chunk_id == DATA_ID) {
            if (!found_fmt) throw BadFormatException("WAVE 'data' chunk found before 'fmt ' chunk.");
            m_data_chunk_offset = chunk_start_pos;
            m_data_chunk_size = chunk_size;
            m_slength = m_data_chunk_size / (m_channels * (16 / 8));
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

size_t WaveStream::getData(size_t len, void *buf) {
    if (eof()) return 0;

    size_t bytes_to_read = std::min(len, static_cast<size_t>(m_data_chunk_size - m_bytes_read_from_data));
    m_file.read(static_cast<char*>(buf), bytes_to_read);
    size_t bytes_read = m_file.gcount();

    m_bytes_read_from_data += bytes_read;
    m_sposition = m_bytes_read_from_data / (m_channels * 2);
    m_position = (m_sposition * 1000) / m_rate;

    return bytes_read;
}

void WaveStream::seekTo(unsigned long pos) {
    unsigned long long sample_pos = (static_cast<unsigned long long>(pos) * m_rate) / 1000;
    unsigned long long byte_pos = sample_pos * m_channels * 2; // 16-bit = 2 bytes

    if (byte_pos > m_data_chunk_size) {
        byte_pos = m_data_chunk_size;
    }

    m_file.clear(); // Clear any EOF flags
    m_file.seekg(m_data_chunk_offset + byte_pos);
    m_bytes_read_from_data = byte_pos;
    m_sposition = sample_pos;
    m_position = pos;
}

bool WaveStream::eof() {
    return m_bytes_read_from_data >= m_data_chunk_size;
}