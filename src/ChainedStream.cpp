/*
 * ChainedStream.cpp - Implementation for a stream that plays multiple files.
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
#include "ChainedStream.h"

ChainedStream::ChainedStream(std::vector<TagLib::String> paths)
    : Stream(), // Call base constructor
      m_paths(std::move(paths)),
      m_current_track_index(0),
      m_total_length_ms(0),
      m_total_samples(0),
      m_samples_played_in_previous_tracks(0)
{
    if (m_paths.empty()) {
        throw std::invalid_argument("ChainedStream cannot be created with an empty path list.");
    }

    // Pre-calculate total length and samples, and validate format consistency.
    long first_rate = -1;
    int first_channels = -1;

    for (const auto& path : m_paths) {
        try {
            std::unique_ptr<Stream> temp_stream(MediaFile::open(path));
            if (!temp_stream) {
                throw InvalidMediaException("ChainedStream: Failed to open track for metadata scan: " + path);
            }

            if (first_rate == -1) { // First track
                first_rate = temp_stream->getRate();
                first_channels = temp_stream->getChannels();
            } else {
                // Ensure all subsequent tracks have the same format.
                if (temp_stream->getRate() != first_rate || temp_stream->getChannels() != first_channels) {
                    throw InvalidMediaException("ChainedStream tracks must have the same sample rate and channel count.");
                }
            }
            m_total_length_ms += temp_stream->getLength();
            m_total_samples += temp_stream->getSLength();
            m_track_lengths_ms.push_back(temp_stream->getLength());
            m_track_lengths_samples.push_back(temp_stream->getSLength());
        } catch (const std::exception& e) {
            // Re-throw with more context
            throw InvalidMediaException("ChainedStream: Error processing track " + path + ": " + e.what());
        }
    }

    // Set the properties for the *entire* chained stream
    m_rate = first_rate;
    m_channels = first_channels;
    m_slength = m_total_samples;
    m_length = m_total_length_ms;

    // The base class path should be the first track's path for identification
    m_path = m_paths.front();

    // Open the first track in the chain.
    if (!openNextTrack()) {
        throw InvalidMediaException("ChainedStream could not open its first track.");
    }
}

bool ChainedStream::openNextTrack()
{
    if (m_current_track_index >= m_paths.size()) {
        m_current_stream.reset(); // No more tracks
        return false;
    }

    // Before opening the next, add the completed samples of the previous one.
    if (m_current_stream) {
        m_samples_played_in_previous_tracks += m_current_stream->getSLength();
    }

    try {
        m_current_stream.reset(MediaFile::open(m_paths[m_current_track_index]));
        m_current_track_index++;
        return m_current_stream != nullptr;
    } catch (const std::exception& e) {
        std::cerr << "ChainedStream: Failed to open track " << m_paths[m_current_track_index] << ": " << e.what() << std::endl;
        m_current_stream.reset();
        return false;
    }
}

void ChainedStream::open(TagLib::String name)
{
    // This is a no-op for ChainedStream as it's initialized with a list of paths.
    // The name parameter is ignored.
}

size_t ChainedStream::getData(size_t len, void *buf)
{
    char *current_buf = static_cast<char *>(buf);
    size_t bytes_remaining = len;
    size_t total_bytes_read = 0;

    while (bytes_remaining > 0) {
        if (!m_current_stream) {
            // No current stream, we've reached the end of the chain.
            break;
        }

        size_t bytes_read_this_call = m_current_stream->getData(bytes_remaining, current_buf);
        total_bytes_read += bytes_read_this_call;
        current_buf += bytes_read_this_call;
        bytes_remaining -= bytes_read_this_call;

        if (m_current_stream->eof()) {
            // Current track finished, try to open the next one.
            if (!openNextTrack()) {
                // No more tracks in the chain, we are done.
                break;
            }
        }

        // If getData returned less than requested but we are not at EOF (e.g. buffer underrun),
        // we should break to avoid a tight loop. The caller will call again.
        if (bytes_read_this_call == 0 && !m_current_stream->eof()) {
            break;
        }
    }

    // Update the base class position members for compatibility, using the aggregated position.
    m_sposition = getSPosition();
    if (m_rate > 0) {
        m_position = (m_sposition * 1000) / m_rate;
    }

    return total_bytes_read;
}

bool ChainedStream::eof()
{
    // The chain is at its end if there is no current stream to play from.
    // This happens when the last track finishes and openNextTrack() fails.
    return !m_current_stream;
}

// The following methods provide aggregated values for the entire chain.
unsigned int ChainedStream::getLength() { return m_total_length_ms; }
unsigned long long ChainedStream::getSLength() { return m_total_samples; }

unsigned int ChainedStream::getPosition()
{
    if (m_rate > 0) return (getSPosition() * 1000) / m_rate;
    return 0;
}

unsigned long long ChainedStream::getSPosition()
{
    if (!m_current_stream) return m_total_samples;
    return m_samples_played_in_previous_tracks + m_current_stream->getSPosition();
}

void ChainedStream::seekTo(unsigned long pos)
{
    // 1. Convert target time in ms to an absolute sample position for the whole chain.
    unsigned long long target_sample_pos = (static_cast<unsigned long long>(pos) * m_rate) / 1000;
    if (target_sample_pos > m_total_samples) {
        target_sample_pos = m_total_samples;
    }

    // 2. Find which track this absolute sample position falls into.
    unsigned long long cumulative_samples = 0;
    size_t target_track_index = 0;
    for (size_t i = 0; i < m_track_lengths_samples.size(); ++i) {
        if (target_sample_pos < cumulative_samples + m_track_lengths_samples[i]) {
            target_track_index = i;
            break;
        }
        cumulative_samples += m_track_lengths_samples[i];
        // Handle case where seek is to the very end of the last track.
        if (i == m_track_lengths_samples.size() - 1) {
            target_track_index = i;
        }
    }

    // 3. Open the target track. This replaces the current stream.
    try {
        m_current_stream.reset(MediaFile::open(m_paths[target_track_index]));
    } catch (const std::exception& e) {
        std::cerr << "ChainedStream::seekTo: Failed to open track " << m_paths[target_track_index] << ": " << e.what() << std::endl;
        m_current_stream.reset();
        m_eof = true; // Can't play anymore.
        return;
    }

    // 4. Update state variables to reflect the new track.
    m_current_track_index = target_track_index + 1; // Next call to openNextTrack will open track after this one.
    m_samples_played_in_previous_tracks = cumulative_samples; // This is the sum of samples before the target track.

    // 5. Seek within the newly opened track.
    unsigned long long seek_pos_in_track_samples = target_sample_pos - m_samples_played_in_previous_tracks;
    unsigned long seek_pos_in_track_ms = (m_rate > 0) ? (seek_pos_in_track_samples * 1000) / m_rate : 0;
    m_current_stream->seekTo(seek_pos_in_track_ms);

    // 6. Reset EOF flag and update base class position members for consistency.
    m_eof = false;
    m_sposition = getSPosition();
    m_position = getPosition();
}
