/*
 * OggStreamManager.cpp - Ogg Stream Layer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

#include "demuxer/ogg/OggStreamManager.h"

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

OggStreamManager::OggStreamManager(int serial_number)
    : m_serial_no(serial_number)
    , m_headers_complete(false)
    , m_initialized(false)
    , m_last_granule(0) {
    if (ogg_stream_init(&m_stream_state, m_serial_no) == 0) {
        m_initialized = true;
    }
}

OggStreamManager::~OggStreamManager() {
    if (m_initialized) {
        ogg_stream_clear(&m_stream_state);
    }
}

int OggStreamManager::submitPage(ogg_page* page) {
    if (!m_initialized) return -1;
    return ogg_stream_pagein(&m_stream_state, page);
}

int OggStreamManager::getPacket(ogg_packet* packet) {
    if (!m_initialized) return -1;
    int result = ogg_stream_packetout(&m_stream_state, packet);
    if (result == 1 && packet->granulepos >= 0) {
        m_last_granule = packet->granulepos;
    } else if (result == -1) {
        Debug::log("ogg", "OggStreamManager::getPacket: data gap detected in stream ", m_serial_no);
    }
    return result;
}

int OggStreamManager::peekPacket(ogg_packet* packet) {
    if (!m_initialized) return -1;
    return ogg_stream_packetpeek(&m_stream_state, packet);
}

void OggStreamManager::reset() {
    if (m_initialized) {
        ogg_stream_reset(&m_stream_state);
    }
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
