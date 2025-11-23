/*
 * ChainedStream.h - A stream that seamlessly plays multiple files in sequence.
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

#ifndef CHAINEDSTREAM_H
#define CHAINEDSTREAM_H

namespace PsyMP3 {
namespace Demuxer {

class ChainedStream : public Stream
{
public:
    explicit ChainedStream(std::vector<TagLib::String> paths);
    virtual ~ChainedStream() = default;

    // Overridden methods from Stream
    virtual void open(TagLib::String name) override; // This will be a no-op
    virtual size_t getData(size_t len, void *buf) override;
    virtual void seekTo(unsigned long pos) override;
    virtual bool eof() override;

    // We also need to override these to provide aggregated values
    virtual unsigned int getLength() override;
    virtual unsigned long long getSLength() override;
    virtual unsigned int getPosition() override;
    virtual unsigned long long getSPosition() override;

private:
    bool openNextTrack();

    std::vector<TagLib::String> m_paths;
    std::vector<unsigned int> m_track_lengths_ms;
    std::vector<unsigned long long> m_track_lengths_samples;
    size_t m_current_track_index;
    std::unique_ptr<Stream> m_current_stream;

    unsigned int m_total_length_ms;
    unsigned long long m_total_samples;
    unsigned long long m_samples_played_in_previous_tracks;
};


} // namespace Demuxer
} // namespace PsyMP3
#endif // CHAINEDSTREAM_H
