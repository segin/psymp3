/*
 * OggDemuxer.cpp - RFC-compliant Ogg Demuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/OggDemuxer.h"
#include "demuxer/ogg/CodecHeaderParser.h"
#include "demuxer/ogg/OggSeekingEngine.h"
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
    : Demuxer(std::move(handler)), m_primary_serial(-1), m_eof(false) {
    
    // Demuxer base class owns the IOHandler.
    // OggSyncManager takes a raw pointer to use it.
    m_sync = std::make_unique<OggSyncManager>(m_handler.get());
}

OggDemuxer::~OggDemuxer() {
    // Cleanup
}

bool OggDemuxer::parseContainer() {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    
    // Read pages until we've found all BOS pages and their headers
    ogg_page page;
    bool in_headers = true;
    
    while (in_headers) {
        int result = m_sync->getNextPage(&page);
        if (result != 1) {
            return !m_streams.empty(); // OK if we found at least one stream
        }
        
        int serial = ogg_page_serialno(&page);
        
        // Is this a BOS page?
        if (ogg_page_bos(&page)) {
            // Create a new stream manager for this serial
            auto stream = std::make_unique<OggStreamManager>(serial);
            stream->submitPage(&page);
            
            // Extract the first packet for codec identification
            ogg_packet packet;
            if (stream->getPacket(&packet)) {
                auto parser = CodecHeaderParser::create(&packet);
                if (parser) {
                    parser->parseHeader(&packet);
                    m_parsers[serial] = std::move(parser);
                    
                    // Set primary stream to first audio stream
                    if (m_primary_serial < 0) {
                        m_primary_serial = serial;
                    }
                }
            }
            
            m_streams[serial] = std::move(stream);
        } else {
            // Not a BOS page - submit to appropriate stream
            auto it = m_streams.find(serial);
            if (it != m_streams.end()) {
                it->second->submitPage(&page);
                
                // Check if headers complete
                auto pit = m_parsers.find(serial);
                if (pit != m_parsers.end() && !pit->second->isHeadersComplete()) {
                    ogg_packet packet;
                    while (it->second->getPacket(&packet)) {
                        pit->second->parseHeader(&packet);
                        if (pit->second->isHeadersComplete()) {
                            break;
                        }
                    }
                }
            }
            
            // Check if all streams have complete headers
            in_headers = false;
            for (auto& p : m_parsers) {
                if (!p.second->isHeadersComplete()) {
                    in_headers = true;
                    break;
                }
            }
        }
    }
    
    return !m_streams.empty();
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    std::vector<StreamInfo> result;
    
    for (auto& pair : m_parsers) {
        StreamInfo info;
        info.stream_id = pair.first;
        auto ci = pair.second->getCodecInfo();
        info.codec_name = ci.codec_name;
        info.channels = ci.channels;
        info.sample_rate = ci.rate;
        result.push_back(info);
    }
    
    return result;
}

StreamInfo OggDemuxer::getStreamInfo(uint32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    
    auto pit = m_parsers.find(stream_id);
    if (pit == m_parsers.end()) {
        return StreamInfo();
    }
    
    StreamInfo info;
    info.stream_id = stream_id;
    auto ci = pit->second->getCodecInfo();
    info.codec_name = ci.codec_name;
    info.channels = ci.channels;
    info.sample_rate = ci.rate;
    return info;
}

MediaChunk OggDemuxer::readChunk() {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    return readChunk_unlocked(m_primary_serial);
}

MediaChunk OggDemuxer::readChunk(uint32_t stream_id) {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    return readChunk_unlocked(stream_id);
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    return seekTo_unlocked(timestamp_ms);
}

bool OggDemuxer::isEOF() const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    return m_eof;
}

uint64_t OggDemuxer::getDuration() const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    
    if (m_primary_serial < 0 || m_streams.empty()) return 0;
    
    // Use seeking engine to calculate
    auto it = m_streams.find(m_primary_serial);
    if (it == m_streams.end()) return 0;
    
    OggSeekingEngine engine(*m_sync, *it->second, getSampleRate());
    return static_cast<uint64_t>(engine.calculateDuration() * 1000.0);
}

uint64_t OggDemuxer::getPosition() const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    // Return position of primary stream
    return getGranulePosition(m_primary_serial);
}

uint64_t OggDemuxer::getGranulePosition(uint32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end()) return 0;
    
    return it->second->getGranulePos();
}

MediaChunk OggDemuxer::readChunk_unlocked(uint32_t stream_id) {
    MediaChunk chunk;
    
    auto sit = m_streams.find(stream_id);
    if (sit == m_streams.end()) {
        m_eof = true;
        return chunk;
    }
    
    // Try to get a packet from this stream
    ogg_packet packet;
    
    while (!sit->second->getPacket(&packet)) {
        // Need more data - read next page
        ogg_page page;
        int result = m_sync->getNextPage(&page);
        if (result != 1) {
            m_eof = true;
            return chunk;
        }
        
        int serial = ogg_page_serialno(&page);
        auto it = m_streams.find(serial);
        if (it != m_streams.end()) {
            it->second->submitPage(&page);
        }
        
        // Check for EOS
        if (ogg_page_eos(&page) && serial == static_cast<int>(stream_id)) {
            m_eof = true;
        }
    }
    
    // Got a packet - copy data to chunk
    chunk.data.assign(packet.packet, packet.packet + packet.bytes);
    chunk.stream_id = stream_id;
    chunk.granule_position = packet.granulepos;
    
    return chunk;
}

bool OggDemuxer::seekTo_unlocked(uint64_t timestamp_ms) {
    if (m_primary_serial < 0) return false;
    
    auto sit = m_streams.find(m_primary_serial);
    if (sit == m_streams.end()) return false;
    
    OggSeekingEngine engine(*m_sync, *sit->second, getSampleRate());
    
    double time_seconds = static_cast<double>(timestamp_ms) / 1000.0;
    bool success = engine.seekToTime(time_seconds);
    
    if (success) {
        m_eof = false;
        // Reset all streams
        for (auto& pair : m_streams) {
            pair.second->reset();
        }
    }
    
    return success;
}

long OggDemuxer::getSampleRate() const {
    auto pit = m_parsers.find(m_primary_serial);
    if (pit == m_parsers.end()) return 48000;
    return pit->second->getCodecInfo().rate;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

