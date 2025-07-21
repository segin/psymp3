/*
 * flac.cpp - Extends the Stream base class to decode FLACs.
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

#ifdef HAVE_FLAC

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
        std::unique_lock<std::mutex> lock(m_handle.m_output_buffer_mutex);

        // Wait until there's data in the buffer, or the decoder has finished (or errored).
        // All error states are > FLAC__STREAM_DECODER_END_OF_STREAM.
        m_handle.m_output_buffer_cv.wait(lock, [this]{
            return !m_handle.m_output_buffer.empty() || m_handle.get_state() >= FLAC__STREAM_DECODER_END_OF_STREAM;
        });

        // After waking up, check the decoder's state.
        FLAC__StreamDecoderState state = m_handle.get_state();

        // If the state is an error state, throw an exception.
        if (state > FLAC__STREAM_DECODER_END_OF_STREAM) {
            throw BadFormatException("FLAC decoder error: " + std::string(FLAC__StreamDecoderStateString[state]));
        }

        // If the buffer is empty and we're at the end, there's nothing more to do.
        if (m_handle.m_output_buffer.empty() && state == FLAC__STREAM_DECODER_END_OF_STREAM) {
            m_eof = true;
            break;
        }

        // Copy available data from the buffer.
        if (!m_handle.m_output_buffer.empty()) {
            size_t samples_available = m_handle.m_output_buffer.size();
            size_t samples_to_copy = std::min(samples_available, bytes_to_read / sizeof(int16_t));

            if (samples_to_copy > 0) {
                size_t bytes_to_copy = samples_to_copy * sizeof(int16_t);
                std::memcpy(current_buf, m_handle.m_output_buffer.data(), bytes_to_copy);
                m_handle.m_output_buffer.erase(m_handle.m_output_buffer.begin(), m_handle.m_output_buffer.begin() + samples_to_copy);

                total_bytes_read += bytes_to_copy;
                current_buf += bytes_to_copy;
                bytes_to_read -= bytes_to_copy;
            }
        }

        // Unlock and notify the producer thread that there's now space in the buffer.
        lock.unlock();
        m_handle.m_output_buffer_cv.notify_one();
    }

    return total_bytes_read;
}

unsigned int Flac::getPosition()
{
    // Get the real-time sample position from the decoder thread
    // and convert it to milliseconds.
    if (m_rate > 0) {
        // The decoder reports the position of the last *decoded* sample.
        // To get the position of the currently *playing* sample, we must
        // subtract the amount of audio that is still in the buffer.
        uint64_t decoded_pos_samples = m_handle.get_current_sample_position();
        size_t buffered_samples = 0;
        {
            // Lock to safely access the buffer size.
            std::unique_lock<std::mutex> lock(m_handle.m_output_buffer_mutex);
            // The buffer stores interleaved int16_t samples.
            // The number of PCM frames (one sample per channel) is size / channels.
            if (m_channels > 0) {
                buffered_samples = m_handle.m_output_buffer.size() / m_channels;
            }
        }

        // Ensure we don't underflow if the buffer has more samples than the reported position
        // (which can happen briefly during state transitions).
        uint64_t playing_pos_samples = (decoded_pos_samples > buffered_samples) ? (decoded_pos_samples - buffered_samples) : 0;

        return (playing_pos_samples * 1000) / m_rate;
    }
    return 0;
}

unsigned long long Flac::getSPosition()
{
    return m_handle.get_current_sample_position();
}

void Flac::seekTo(unsigned long pos)
{
    // Convert milliseconds to sample offset based on original FLAC rate
    ogg_int64_t target_sample = (static_cast<ogg_int64_t>(pos) * m_handle.m_stream_info.sample_rate) / 1000;

    m_handle.requestSeek(target_sample);
    m_eof = false; // Reset EOF flag after seek. Position is now updated by getPosition().
}

bool Flac::eof()
{
    // Check if the decoder itself has reached the end of the stream
    // and if our internal buffer is also empty.
    return m_handle.get_state() == FLAC__STREAM_DECODER_END_OF_STREAM && m_handle.m_output_buffer.empty();
}

FlacDecoder::FlacDecoder(TagLib::String path)
    : FLAC::Decoder::Stream(), m_path(path),
      m_decoding_active(false), m_seek_request(false)
{
    // Initialize stream_info to zero to prevent garbage data
    memset(&m_stream_info, 0, sizeof(m_stream_info));
    // High-water mark for the decoded buffer to prevent it from growing too large.
    // Pre-allocate to avoid reallocations in the audio/decoder threads.
    m_output_buffer.reserve(48000 * 2 * 4); // ~4 seconds of 48kHz stereo audio

    URI uri(path);
    if (uri.scheme() == "file") {
        m_handler = std::make_unique<FileIOHandler>(uri.path());
    } else {
        throw InvalidMediaException("Unsupported URI scheme for FLAC: " + uri.scheme());
    }
}

FlacDecoder::~FlacDecoder()
{
    stopDecoderThread();
}

::FLAC__StreamDecoderInitStatus FlacDecoder::init()
{
    // Now initialize the base stream decoder
    return FLAC::Decoder::Stream::init();
}

::FLAC__StreamDecoderReadStatus FlacDecoder::read_callback(FLAC__byte buffer[], size_t *bytes) {
    if (*bytes > 0) {
        *bytes = m_handler->read(buffer, 1, *bytes);
        // As per libFLAC examples, return END_OF_STREAM when 0 bytes are read.
        return *bytes == 0 ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT; // Should not be called with *bytes == 0
}

::FLAC__StreamDecoderSeekStatus FlacDecoder::seek_callback(FLAC__uint64 absolute_byte_offset) {
    if (m_handler->seek(static_cast<long>(absolute_byte_offset), SEEK_SET) != 0)
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

::FLAC__StreamDecoderTellStatus FlacDecoder::tell_callback(FLAC__uint64 *absolute_byte_offset) {
    long pos = m_handler->tell();
    if (pos < 0)
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = static_cast<FLAC__uint64>(pos);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

::FLAC__StreamDecoderLengthStatus FlacDecoder::length_callback(FLAC__uint64 *stream_length) {
    // Get stream length by seeking to the end, telling, and seeking back.
    long current_pos = m_handler->tell();
    if (current_pos < 0) return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

    if (m_handler->seek(0, SEEK_END) != 0) return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

    long size = m_handler->tell();
    if (size < 0) return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

    // Restore original position
    if (m_handler->seek(current_pos, SEEK_SET) != 0) return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

    *stream_length = static_cast<FLAC__uint64>(size);
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

bool FlacDecoder::eof_callback() {
    return m_handler->eof();
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
                m_current_sample_position = m_seek_position_samples.load(); // Update position after seek
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

    // Update the real-time sample position.
    m_current_sample_position += frame->header.blocksize;

    m_output_buffer_cv.notify_one(); // Notify that data is available
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::metadata_callback(const ::FLAC__StreamMetadata *metadata) {
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        m_stream_info = metadata->data.stream_info;
    }
}

void FlacDecoder::error_callback(::FLAC__StreamDecoderErrorStatus status) {
    Debug::log("flac", "FLAC Decoder Error: ", FLAC__StreamDecoderErrorStatusString[status]);
    // An error occurred, which changes the decoder's internal state.
    // We just need to wake up the consumer thread in case it's waiting for data.
    // It will then check the new state and handle the error.
    m_output_buffer_cv.notify_one();
}

#endif // HAVE_FLAC