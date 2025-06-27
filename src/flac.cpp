/*
 * flac.cpp - Extends the Stream base class to decode FLACs.
 * This file is part of PsyMP3.
 * Copyright © 2011-2024 Kirn Gill <segin2005@gmail.com>
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

Flac::Flac(TagLib::String name) : Stream(name), m_handle(name) // m_handle is FlacDecoder
{
    // sampbuf is no longer needed with the new buffering approach in FlacDecoder
    // delete[] sampbuf; // Remove this line if it was in the constructor
    open(name);
}

Flac::~Flac()
{
    // m_handle (FlacDecoder) is a member, its destructor will be called automatically.
    // No need to delete sampbuf here.
}

void Flac::open(TagLib::String name)
{
    // Initialize the decoder with the file
    if (!m_handle.init(name.to8Bit(true))) {
        throw InvalidMediaException("Failed to initialize FLAC decoder for: " + name);
    }

    // Process metadata to get stream info (rate, channels, length, etc.)
    // This will trigger metadata_callback in FlacDecoder
    if (!m_handle.process_until_end_of_metadata()) {
        throw InvalidMediaException("Failed to read FLAC metadata for: " + name);
    }

    // Populate Stream base class members from FlacDecoder's metadata
    m_rate = m_handle.m_stream_info.sample_rate;
    m_channels = m_handle.m_stream_info.channels;
    m_slength = m_handle.m_stream_info.total_samples;
    m_bitrate = 0; // FLAC is lossless, bitrate is variable. Can estimate if needed.

    // Calculate length in milliseconds
    if (m_rate > 0) {
        m_length = static_cast<unsigned int>((m_slength * 1000) / m_rate);
    } else {
        m_length = 0;
    }
    m_position = 0;
    m_sposition = 0;
    m_eof = false;
}

size_t Flac::getData(size_t len, void *buf)
{
    char *current_buf = static_cast<char *>(buf);
    size_t bytes_to_read = len;
    size_t total_bytes_read = 0;

    while (bytes_to_read > 0) {
        std::unique_lock<std::mutex> lock(m_handle.m_output_buffer_mutex);
        // Wait until there's data in the buffer or the decoder has reached the end of its stream
        m_handle.m_output_buffer_cv.wait(lock, [this]{
            return !m_handle.m_output_buffer.empty() || m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM;
        });

        if (m_handle.m_output_buffer.empty() && m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM) {
            m_eof = true; // No more data and decoder is at EOF
            break;
        }

        // Calculate how many samples we can copy from the buffer
        size_t samples_available = m_handle.m_output_buffer.size();
        size_t samples_needed = bytes_to_read / sizeof(int16_t);
        size_t samples_to_copy = std::min(samples_available, samples_needed);

        if (samples_to_copy == 0) {
            // Buffer is empty, but not yet EOF. Need to process more from the FLAC file.
            // Release lock before processing to allow write_callback to acquire it.
            lock.unlock();
            if (!m_handle.process_single()) { // Decode more data from the FLAC file
                // If process_single fails or returns false, it might be an error or actual EOF.
                // Check state again after processing.
                if (m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM) {
                    m_eof = true;
                } else {
                    // Some other error or no data produced.
                    // For now, break. More robust error handling needed.
                    std::cerr << "FLAC::getData: process_single failed or produced no data." << std::endl;
                }
                break;
            }
            continue; // Try to read from buffer again
        }

        // Copy samples to the output buffer
        std::memcpy(current_buf, m_handle.m_output_buffer.data(), samples_to_copy * sizeof(int16_t));

        // Remove copied samples from the internal buffer
        m_handle.m_output_buffer.erase(m_handle.m_output_buffer.begin(), m_handle.m_output_buffer.begin() + samples_to_copy);

        total_bytes_read += samples_to_copy * sizeof(int16_t);
        current_buf += samples_to_copy * sizeof(int16_t);
        bytes_to_read -= samples_to_copy * sizeof(int16_t);
    }

    // Update position based on samples read
    // Note: m_sposition is total samples across all channels.
    m_sposition += (total_bytes_read / sizeof(int16_t));
    if (m_channels > 0 && m_rate > 0) {
        m_position = static_cast<unsigned int>((m_sposition / m_channels * 1000) / m_rate);
    } else {
        m_position = 0;
    }

    return total_bytes_read;
}

void Flac::seekTo(unsigned long pos)
{
    if (!m_handle.is_valid()) {
        throw InvalidMediaException("FLAC decoder not initialized for seeking.");
    }

    // Convert milliseconds to sample offset based on original FLAC rate
    ogg_int64_t target_sample = (static_cast<ogg_int64_t>(pos) * m_handle.m_stream_info.sample_rate) / 1000;

    if (!m_handle.seek_absolute(target_sample)) {
        throw BadFormatException("Failed to seek in FLAC file to " + std::to_string(pos) + "ms.");
    }

    // Clear internal buffer after seek, as data is no longer valid
    std::unique_lock<std::mutex> lock(m_handle.m_output_buffer_mutex);
    m_handle.m_output_buffer.clear();
    lock.unlock();

    // Update positions
    m_sposition = target_sample;
    m_position = pos;
    m_eof = false; // Reset EOF flag after seek
}

bool Flac::eof()
{
    // Check if the decoder itself has reached the end of the stream
    // and if our internal buffer is also empty.
    return m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM && m_handle.m_output_buffer.empty();
}

void Flac::init()
{
    // FLAC++ library doesn't require global init/fini like mpg123
    // but if there were any global FLAC settings, they'd go here.
}

void Flac::fini()
{
    // No global finalization needed for FLAC++
}

::FLAC__StreamDecoderWriteStatus FlacDecoder::write_callback(const ::FLAC__Frame *frame, const FLAC__int32 *const buffer[]) {
    std::unique_lock<std::mutex> lock(m_output_buffer_mutex);

    // Convert FLAC__int32 samples to int16_t and store in m_output_buffer.
    // This handles bit depth conversion. No sample rate conversion here.
    for (unsigned i = 0; i < frame->header.blocksize; ++i) {
        for (unsigned channel = 0; channel < frame->header.channels; ++channel) {
            int32_t sample = buffer[channel][i];

            // Clamp to int16_t range and convert
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;

            m_output_buffer.push_back(static_cast<int16_t>(sample));
        }
    }

    m_output_buffer_cv.notify_one(); // Notify that data is available
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::metadata_callback(const ::FLAC__StreamMetadata *metadata) {
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        m_stream_info = metadata->data.stream_info;
    }
}

void FlacDecoder::error_callback(::FLAC__StreamDecoderErrorStatus status) {
    std::cerr << "FLAC Decoder Error: " << FLAC__StreamDecoderErrorStatusString[status] << std::endl;
    // Depending on the error, you might want to set a flag or throw an exception
    // to signal the main thread that playback cannot continue.
    // For now, we just log it.
    // If this error indicates end of stream, ensure getData loop terminates.
    if (status == FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC ||
        status == FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER ||
        status == FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM) {
        // Consider setting m_eof = true in the Flac class or similar.
    }
}