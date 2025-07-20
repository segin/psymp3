/*
 * Demuxer.cpp - Generic container demuxer base class implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

Demuxer::Demuxer(std::unique_ptr<IOHandler> handler) 
    : m_handler(std::move(handler)) {
}

// DemuxerFactory implementation moved to DemuxerFactory.cpp