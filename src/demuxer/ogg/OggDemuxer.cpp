/*
 * OggDemuxer.cpp - RFC-compliant Ogg Demuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/OggDemuxer.h"
#include "demuxer/DemuxerFactory.h"
#include <iostream>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

// Auto-registration
namespace {
    bool registered = OggDemuxer::registerDemuxer();
}

bool OggDemuxer::registerDemuxer() {
    DemuxerFactory::registerDemuxer("ogg", [](std::unique_ptr<PsyMP3::IO::IOHandler> io) {
        return std::make_unique<OggDemuxer>(std::move(io));
    });
    return true;
}

OggDemuxer::OggDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler)
    : Demuxer(std::move(handler)) {
    
    // Demuxer base class owns the IOHandler.
    // OggSyncManager takes a raw pointer to use it.
    m_sync = std::make_unique<OggSyncManager>(m_handler.get());
}

OggDemuxer::~OggDemuxer() {
    // Cleanup
}

bool OggDemuxer::parseContainer() {
    // To be implemented
    return false;
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
    return {};
}

StreamInfo OggDemuxer::getStreamInfo(uint32_t stream_id) const {
    return StreamInfo();
}

MediaChunk OggDemuxer::readChunk() {
    // To be implemented
    return MediaChunk();
}

MediaChunk OggDemuxer::readChunk(uint32_t stream_id) {
    // To be implemented
    return MediaChunk();
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    return false;
}

bool OggDemuxer::isEOF() const {
    return true;
}

uint64_t OggDemuxer::getDuration() const {
    return 0;
}

uint64_t OggDemuxer::getPosition() const {
    return 0;
}

uint64_t OggDemuxer::getGranulePosition(uint32_t stream_id) const {
    return 0;
}

MediaChunk OggDemuxer::readChunk_unlocked(uint32_t stream_id) {
    return MediaChunk();
}

bool OggDemuxer::seekTo_unlocked(uint64_t timestamp_ms) {
    return false;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
