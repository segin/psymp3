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

Flac::Flac(TagLib::String name) : Stream(name), m_handle(name)
{
    sampbuf = new char[16384];
    open(name);
}

Flac::~Flac()
{
    delete[] sampbuf;
}

void Flac::open(TagLib::String name)
{
    try {
        m_handle.init(name.to8Bit(true));
    } catch (...) {
        throw InvalidMediaException("Couldn't open as FLAC!");
    }
}

size_t Flac::getData(size_t len, void *buf)
{
    memset(buf, 0, len);
    return 0;
}

void Flac::seekTo(unsigned long pos)
{
    return;
}

bool Flac::eof()
{
    return m_eof;
}

void Flac::init()
{

}

void Flac::fini()
{

}

::FLAC__StreamDecoderWriteStatus FlacDecoder::write_callback(const ::FLAC__Frame *frame, const FLAC__int32 *const buffer[]) {
    return ::FLAC__StreamDecoderWriteStatus();
}

void FlacDecoder::metadata_callback(const ::FLAC__StreamMetadata *metadata) {
}

void FlacDecoder::error_callback(::FLAC__StreamDecoderErrorStatus status) {
}