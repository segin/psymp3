/*
 * vorbis.cpp - Extends the Stream base class to decode Ogg Vorbis.
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

Vorbis::Vorbis(TagLib::String name) : Stream(name)
{
    m_session = 0; // Initialize session
    open(name); // open() will handle handler creation and callbacks
}

Vorbis::~Vorbis()
{
    // ov_clear will call the close callback, which is handled by our IOHandler.
    // The m_handler unique_ptr will then be destroyed, ensuring resources are freed.
    ov_clear(&m_vorbis_file);
}

void Vorbis::open(TagLib::String name)
{
    // Use the new URI and IOHandler abstraction layer.
    URI uri(name);
    if (uri.scheme() == "file") {
        m_handler = std::make_unique<FileIOHandler>(uri.path());
    } else {
        // In the future, a factory could create other IOHandlers here (e.g., HttpIOHandler).
        throw InvalidMediaException("Unsupported URI scheme for Vorbis: " + uri.scheme());
    }

    // These callbacks now forward to the generic IOHandler interface.
    ov_callbacks callbacks = {
        /* read_func */
        [](void *ptr, size_t size, size_t nmemb, void *datasource) -> size_t {
            return static_cast<IOHandler*>(datasource)->read(ptr, size, nmemb);
        },
        /* seek_func */
        [](void *datasource, ogg_int64_t offset, int whence) -> int {
            return static_cast<IOHandler*>(datasource)->seek(offset, whence);
        },
        /* close_func */
        [](void *datasource) -> int {
            // The IOHandler is managed by the Vorbis class's unique_ptr,
            // so its destructor will handle the actual closing. We can return 0 here.
            // Alternatively, for handlers that need explicit closing:
            return static_cast<IOHandler*>(datasource)->close();
        },
        /* tell_func */
        [](void *datasource) -> long {
            return static_cast<IOHandler*>(datasource)->tell();
        }
    };

    // Pass the raw pointer to our IOHandler as the datasource.
    auto ret = ov_open_callbacks(m_handler.get(), &m_vorbis_file, nullptr, 0, callbacks);

    switch (ret) {
    case OV_ENOTVORBIS:
        throw WrongFormatException("Not a Vorbis file: " + name);
        break;
    case OV_EREAD:
    case OV_EVERSION:
    case OV_EBADHEADER:
    case OV_EFAULT:
        throw BadFormatException("Bad file: " + name);
        //throw;
        break;
    default: // returned 0 for success
        m_vi = ov_info(&m_vorbis_file, -1);
        m_eof = false;
        switch(m_vi->channels) {
        case 1:
        case 2:
            m_rate = m_vi->rate;
            m_channels = m_vi->channels;
            m_bitrate = m_vi->bitrate_nominal;
            m_length = ov_time_total(&m_vorbis_file, -1) * 1000;
            m_slength = ov_pcm_total(&m_vorbis_file, -1);
            break;
        default:
            // throw
            break;
        };
        break;
    };
}

void Vorbis::seekTo(unsigned long pos)
{
    ov_time_seek(&m_vorbis_file, (double) pos / 1000.0);
    m_sposition = ov_pcm_tell(&m_vorbis_file);
    m_position = ov_time_tell(&m_vorbis_file) * 1000;
}

size_t Vorbis::getData(size_t len, void *buf)
{
    char *current_buf = static_cast<char *>(buf);
    size_t bytes_left = len;
    size_t total_bytes_read = 0;

    while (bytes_left > 0) {
        long bytes_read_this_call = ov_read(&m_vorbis_file, current_buf, bytes_left, 0, 2, 1, &m_session);

        if (bytes_read_this_call < 0) { // Error
            // Any negative value from ov_read is a fatal error.
            throw BadFormatException("Failed to read Vorbis file, error code: " + std::to_string(bytes_read_this_call));
        } else if (bytes_read_this_call == 0) { // End of file
            m_eof = true;
            break;
        }

        total_bytes_read += bytes_read_this_call;
        current_buf += bytes_read_this_call;
        bytes_left -= bytes_read_this_call;
    }
    m_sposition = ov_pcm_tell(&m_vorbis_file);
    m_position = ov_time_tell(&m_vorbis_file) * 1000;
    return total_bytes_read;
}

unsigned int Vorbis::getLength()
{
    return m_length;
}

unsigned int Vorbis::getPosition()
{
    return ov_time_tell(&m_vorbis_file) * 1000;
}

unsigned long long Vorbis::getSLength()
{
    return m_slength;
}

unsigned long long Vorbis::getSPosition()
{
    return m_sposition;
}

unsigned int Vorbis::getChannels()
{
    return m_channels;
}

unsigned int Vorbis::getRate()
{
    return m_rate;
}

unsigned int Vorbis::getEncoding()
{
    return 0;
}

unsigned int Vorbis::getBitrate()
{
    return m_bitrate;
}

bool Vorbis::eof()
{
    return m_eof;
}
