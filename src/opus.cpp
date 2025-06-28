/*
 * opus.cpp - Extends the Stream base class to decode Ogg Opus.
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

OpusFile::OpusFile(TagLib::String name) : Stream(name), m_session(nullptr)
{
    m_eof = false;
    open(name);
}

OpusFile::~OpusFile()
{
    if (m_session) {
        op_free(m_session);
        m_session = nullptr;
    }
}

void OpusFile::open(TagLib::String name)
{
    int error;
    // Clean up previous session if any
    if (m_session) {
        op_free(m_session);
        m_session = nullptr;
    }
    m_session = op_open_file(name.toCString(true), &error);
    if(!m_session) {
        // Provide a more informative exception, including the library's error code.
        throw InvalidMediaException("Failed to open Opus file '" + name + "': " + op_strerror(error));
    }

    // As per the opusfile manual, output is always 48kHz.
    // Since op_read_stereo() is used, output is always 2 channels.
    m_channels = 2;
    m_rate = 48000;
    m_bitrate = 0;
    m_slength = op_pcm_total(m_session, -1);
    // Explicitly calculate length in milliseconds from samples and rate.
    if (m_rate > 0) {
        m_length = (m_slength * 1000) / m_rate;
    } else {
        m_length = 0;
    }
}

void OpusFile::seekTo(unsigned long pos)
{
    if (!m_session) return;
    // Use 64-bit arithmetic to prevent potential overflow when calculating sample position from milliseconds.
    ogg_int64_t pcm_offset = (static_cast<ogg_int64_t>(pos) * m_rate) / 1000;
    op_pcm_seek(m_session, pcm_offset);
    m_sposition = op_pcm_tell(m_session);
    if (m_rate > 0) {
        m_position = (m_sposition * 1000) / m_rate;
    }
}

size_t OpusFile::getData(size_t len, void *buf)
{
    if (!m_session) return 0;

    char *current_ptr = static_cast<char *>(buf);
    size_t bytes_left = len;
    size_t total_bytes_read = 0;

    while (bytes_left > 0) {
        // op_read_stereo wants the total number of opus_int16 values the buffer can hold.
        int buffer_capacity_in_samples = bytes_left / sizeof(opus_int16);
        if (buffer_capacity_in_samples == 0) break;

        int samples_read_per_channel = op_read_stereo(m_session, reinterpret_cast<opus_int16 *>(current_ptr), buffer_capacity_in_samples);

        if (samples_read_per_channel < 0) { // Error
            throw BadFormatException("Failed to read Opus file, error: " + std::to_string(samples_read_per_channel));
        } else if (samples_read_per_channel == 0) { // End of file
            m_eof = true;
            break;
        }

        size_t bytes_read_this_call = samples_read_per_channel * m_channels * sizeof(opus_int16);
        total_bytes_read += bytes_read_this_call;
        current_ptr += bytes_read_this_call;
        bytes_left -= bytes_read_this_call;
    }

    m_sposition = op_pcm_tell(m_session);
    if (m_rate > 0) {
        m_position = (m_sposition * 1000) / m_rate;
    }
    return total_bytes_read;
}

unsigned int OpusFile::getLength()
{
    return m_length;
}

unsigned long long OpusFile::getSLength()
{
    return m_slength;
}

unsigned long long OpusFile::getSPosition()
{
    if (!m_session) return 0;
    return op_pcm_tell(m_session);
}

unsigned int OpusFile::getChannels()
{
    return m_channels;
}

unsigned int OpusFile::getRate()
{
    return m_rate;
}

unsigned int OpusFile::getEncoding()
{
    return 0;
}

unsigned int OpusFile::getBitrate()
{
    return m_bitrate;
}

bool OpusFile::eof()
{
    return m_eof;
}
