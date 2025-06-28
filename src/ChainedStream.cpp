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