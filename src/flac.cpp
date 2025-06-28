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
    open(name);
    m_handle.startDecoderThread();
}

Flac::~Flac()
{
    m_handle.stopDecoderThread();
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
    size_t bytes_to_read = len;
    size_t total_bytes_read = 0;

    while (bytes_to_read > 0) {
        // Check for a decoder error before waiting or processing data.
        FLAC__StreamDecoderErrorStatus error = m_handle.m_last_error.load();
        if (error != FLAC__STREAM_DECODER_ERROR_STATUS_OK) {
            throw BadFormatException("FLAC decoder error: " + TagLib::String(FLAC__StreamDecoderErrorStatusString[error]));
        }

        std::unique_lock<std::mutex> lock(m_handle.m_output_buffer_mutex);

        // Wait until there's data, the decoder has finished, or an error has occurred.
        m_handle.m_output_buffer_cv.wait(lock, [this]{
            return !m_handle.m_output_buffer.empty()
                || m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM
                || m_handle.m_last_error.load() != FLAC__STREAM_DECODER_ERROR_STATUS_OK;
        });

        // If the buffer is empty and we're at the end, there's nothing more to do.
        if (m_handle.m_output_buffer.empty() && m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM) {
            m_eof = true;
            break;
        }

        size_t samples_available = m_handle.m_output_buffer.size();
        size_t samples_needed = bytes_to_read / sizeof(int16_t);
        size_t samples_to_copy = std::min(samples_available, samples_needed);

        if (samples_to_copy > 0) {
            std::memcpy(current_buf, m_handle.m_output_buffer.data(), samples_to_copy * sizeof(int16_t));
            m_handle.m_output_buffer.erase(m_handle.m_output_buffer.begin(), m_handle.m_output_buffer.begin() + samples_to_copy);

            size_t bytes_copied = samples_to_copy * sizeof(int16_t);
            total_bytes_read += bytes_copied;
            current_buf += bytes_copied;
            bytes_to_read -= bytes_copied;
        }

        // Unlock and notify the producer thread that there's now space in the buffer.
        lock.unlock();
        m_handle.m_output_buffer_cv.notify_one();
    }

    // Update position based on samples read
    // Note: m_sposition is total samples across all channels.
    m_sposition += (total_bytes_read / (m_channels * sizeof(int16_t)));
    if (m_channels > 0 && m_rate > 0) {
        m_position = static_cast<unsigned int>((m_sposition * 1000) / m_rate);
    } else {
        m_position = 0;
    }

    return total_bytes_read;
}

void Flac::seekTo(unsigned long pos)
{
    // Convert milliseconds to sample offset based on original FLAC rate
    ogg_int64_t target_sample = (static_cast<ogg_int64_t>(pos) * m_handle.m_stream_info.sample_rate) / 1000;

    m_handle.requestSeek(target_sample);

    // Update positions
    m_sposition = target_sample; // This is an approximation until the seek completes
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
    : FLAC::Decoder::Stream(), m_path(path), m_file_handle(nullptr),
      m_decoding_active(false), m_seek_request(false), m_seek_position_samples(0)
{
    // High-water mark for the decoded buffer to prevent it from growing too large.
    // Pre-allocate to avoid reallocations in the audio/decoder threads.
    m_output_buffer.reserve(48000 * 2 * 4); // ~4 seconds of 48kHz stereo audio
}

FlacDecoder::~FlacDecoder()
{
    if (m_file_handle) {
        fclose(m_file_handle);
        m_file_handle = nullptr;
    }
    stopDecoderThread();
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
#ifdef _WIN32
    if (_fseeki64(m_file_handle, static_cast<__int64>(absolute_byte_offset), SEEK_SET) < 0)
#else
    if (fseeko(m_file_handle, static_cast<off_t>(absolute_byte_offset), SEEK_SET) < 0)
#endif
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

::FLAC__StreamDecoderTellStatus FlacDecoder::tell_callback(FLAC__uint64 *absolute_byte_offset) {
#ifdef _WIN32
    __int64 pos = _ftelli64(m_file_handle);
#else
    off_t pos = ftello(m_file_handle);
#endif
    if (pos < 0)
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = static_cast<FLAC__uint64>(pos);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

::FLAC__StreamDecoderLengthStatus FlacDecoder::length_callback(FLAC__uint64 *stream_length) {
#ifdef _WIN32
    // Use 64-bit file status functions for large file support on Windows.
    struct __stat64 st;
    if (_fstat64(_fileno(m_file_handle), &st) != 0)
#else
    struct stat st;
    if (fstat(fileno(m_file_handle), &st) != 0)
#endif
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    *stream_length = st.st_size;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

bool FlacDecoder::eof_callback() {
    return feof(m_file_handle) ? true : false;
}

void FlacDecoder::startDecoderThread() {
    if (!m_decoding_active) {
        m_decoding_active = true;
        m_decoder_thread = std::thread(&FlacDecoder::decoderThreadLoop, this);
    }
}

void FlacDecoder::stopDecoderThread() {
    if (m_decoding_active) {
        m_decoding_active = false;
        m_output_buffer_cv.notify_all(); // Wake up all waiting threads
        if (m_decoder_thread.joinable()) {
            m_decoder_thread.join();
        }
    }
}

void FlacDecoder::requestSeek(FLAC__uint64 sample_offset) {
    m_seek_position_samples = sample_offset;
    m_seek_request = true;
    m_output_buffer_cv.notify_one(); // Wake up decoder thread if it's waiting
}

void FlacDecoder::decoderThreadLoop() {
    System::setThisThreadName("flac-decoder");

    // High-water mark for the decoded buffer to prevent it from growing too large.
    // 4 seconds of 48kHz stereo audio: 48000 samples/sec * 2 channels * 4 seconds
    constexpr size_t BUFFER_HIGH_WATER_MARK = 48000 * 2 * 4;

    while (m_decoding_active) {
        if (m_seek_request) {
            if (seek_absolute(m_seek_position_samples.load())) {
                std::unique_lock<std::mutex> lock(m_output_buffer_mutex);
                m_output_buffer.clear(); // Clear buffer after a successful seek
                lock.unlock();
                m_output_buffer_cv.notify_one(); // Notify consumer that seek is done
            }
            m_seek_request = false;
        }

        // Wait if the buffer is full to avoid using too much memory
        std::unique_lock<std::mutex> lock(m_output_buffer_mutex);
        m_output_buffer_cv.wait(lock, [this]{
            return m_output_buffer.size() < BUFFER_HIGH_WATER_MARK || !m_decoding_active || m_seek_request;
        });
        lock.unlock();

        if (!m_decoding_active) break;
        if (m_seek_request) continue; // Loop back to handle seek immediately

        // If we are not at the end of the stream, process more data
        if (get_state() != FLAC__STREAM_DECODER_END_OF_STREAM) {
            if (!process_single()) {
                // process_single() returned false, might be an error or EOF.
                // The state will be updated, and the loop will terminate naturally.
            }
        }
    }
}

::FLAC__StreamDecoderWriteStatus FlacDecoder::write_callback(const ::FLAC__Frame *frame, const FLAC__int32 *const buffer[]) {
    std::unique_lock<std::mutex> lock(m_output_buffer_mutex);

    // Convert FLAC__int32 samples to int16_t and store in m_output_buffer.
    // This handles bit depth conversion. No sample rate conversion here.
    for (unsigned i = 0; i < frame->header.blocksize; ++i) {
        for (unsigned channel = 0; channel < frame->header.channels; ++channel) {
            int32_t sample = buffer[channel][i];
            
            // Perform bit depth conversion if necessary
            if (m_stream_info.bits_per_sample > 16) {
                // Downscale higher bit depth to 16-bit
                sample >>= (m_stream_info.bits_per_sample - 16);
            } else if (m_stream_info.bits_per_sample < 16) {
                // Upscale lower bit depth to 16-bit
                sample <<= (16 - m_stream_info.bits_per_sample);
            }

            sample = std::clamp(sample, -32768, 32767);

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