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
    if (m_handle.init() != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
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
    size_t bytes_left = len;
    size_t total_bytes_read = 0;

    while (bytes_left > 0) {
        // First, check our internal buffer for any available data.
        std::unique_lock<std::mutex> lock(m_handle.m_output_buffer_mutex);
        size_t samples_available = m_handle.m_output_buffer.size();

        if (samples_available > 0) {
            size_t samples_needed = bytes_left / sizeof(int16_t);
            size_t samples_to_copy = std::min(samples_available, samples_needed);

            // Copy the data and update our state.
            std::memcpy(current_buf, m_handle.m_output_buffer.data(), samples_to_copy * sizeof(int16_t));
            m_handle.m_output_buffer.erase(m_handle.m_output_buffer.begin(), m_handle.m_output_buffer.begin() + samples_to_copy);

            size_t bytes_copied = samples_to_copy * sizeof(int16_t);
            total_bytes_read += bytes_copied;
            current_buf += bytes_copied;
            bytes_left -= bytes_copied;
        }
        lock.unlock(); // Release the lock before potentially decoding more.

        // If we still need more data, drive the decoder to produce some.
        if (bytes_left > 0) {
            // Check if the decoder has already finished.
            if (m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM) {
                m_eof = true;
                break; // No more data can be decoded.
            }

            // process_single() will synchronously call the write_callback, filling our buffer.
            if (!m_handle.process_single()) {
                if (m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM) {
                    m_eof = true;
                } else {
                    std::cerr << "FLAC::getData: process_single() failed." << std::endl;
                }
                break; // Stop trying to get data on failure or EOF.
            }
        }
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

FlacDecoder::FlacDecoder(TagLib::String path)
    : FLAC::Decoder::Stream(), m_path(path), m_file_handle(nullptr)
{
}

FlacDecoder::~FlacDecoder()
{
    if (m_file_handle) {
        fclose(m_file_handle);
        m_file_handle = nullptr;
    }
}

::FLAC__StreamDecoderInitStatus FlacDecoder::init()
{
    // Open the file using fopen for better control over paths
#ifdef _WIN32
    m_file_handle = _wfopen(m_path.toCWString(), L"rb");
#else
    m_file_handle = fopen(m_path.toCString(true), "rb");
#endif

    if (!m_file_handle) {
        return FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE;
    }

    // Now initialize the base stream decoder
    return FLAC::Decoder::Stream::init();
}

::FLAC__StreamDecoderReadStatus FlacDecoder::read_callback(FLAC__byte buffer[], size_t *bytes) {
    if (*bytes > 0) {
        *bytes = fread(buffer, sizeof(FLAC__byte), *bytes, m_file_handle);
        if (ferror(m_file_handle))
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        else
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

::FLAC__StreamDecoderSeekStatus FlacDecoder::seek_callback(FLAC__uint64 absolute_byte_offset) {
    if (fseeko(m_file_handle, static_cast<off_t>(absolute_byte_offset), SEEK_SET) < 0)
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

::FLAC__StreamDecoderTellStatus FlacDecoder::tell_callback(FLAC__uint64 *absolute_byte_offset) {
    off_t pos = ftello(m_file_handle);
    if (pos < 0)
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = static_cast<FLAC__uint64>(pos);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

::FLAC__StreamDecoderLengthStatus FlacDecoder::length_callback(FLAC__uint64 *stream_length) {
    struct stat st;
    if (fstat(fileno(m_file_handle), &st) != 0)
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    *stream_length = st.st_size;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

bool FlacDecoder::eof_callback() {
    return feof(m_file_handle) ? true : false;
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