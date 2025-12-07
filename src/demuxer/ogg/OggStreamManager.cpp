/*
 * OggStreamManager.cpp - Ogg Stream Layer implementation
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

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
