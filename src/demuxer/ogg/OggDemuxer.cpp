/*
 * OggDemuxer.cpp - RFC-compliant Ogg Demuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/OggDemuxer.h"
#include "demuxer/DemuxerFactory.h"
#include "demuxer/ogg/CodecHeaderParser.h"
#include "demuxer/ogg/OggSeekingEngine.h"
#include "tag/VorbisCommentTag.h"
#include <algorithm>
#include <iostream>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

// Note: Registration is handled by registerAllDemuxers() called from main().
// Do NOT use static auto-registration here as it causes initialization order
// issues.

bool OggDemuxer::registerDemuxer() {
  DemuxerFactory::registerDemuxer(
      "ogg", [](std::unique_ptr<PsyMP3::IO::IOHandler> io) {
        return std::make_unique<OggDemuxer>(std::move(io));
      });
  return true;
}

OggDemuxer::OggDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler)
    : Demuxer(std::move(handler)), m_primary_serial(0), m_has_primary_serial(false), m_eof(false) {

  // Demuxer base class owns the IOHandler.
  // OggSyncManager takes a raw pointer to use it.
  m_sync = std::make_unique<OggSyncManager>(m_handler.get());
}

OggDemuxer::~OggDemuxer() {
  // Cleanup
}

bool OggDemuxer::parseContainer() {
  std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);

  Debug::log("ogg", "OggDemuxer::parseContainer() starting");

  // Read pages until we've found all BOS pages and their headers
  ogg_page page;
  bool in_headers = true;
  int page_count = 0;

  while (in_headers) {
    int result = m_sync->getNextPage(&page);
    Debug::log("ogg",
               "OggDemuxer::parseContainer() getNextPage returned: ", result,
               " (1=success, 0=EOF/error)");
    if (result != 1) {
      Debug::log("ogg", "OggDemuxer::parseContainer() failed after ",
                 page_count, " pages, streams found: ", m_streams.size());
      return !m_streams.empty(); // OK if we found at least one stream
    }
    page_count++;

    int serial = ogg_page_serialno(&page);
    Debug::log("ogg", "OggDemuxer::parseContainer() page ", page_count,
               " serial=", serial, " bos=", ogg_page_bos(&page));

    // Is this a BOS page?
    if (ogg_page_bos(&page)) {
      Debug::log("ogg", "OggDemuxer::parseContainer() BOS page detected, "
                        "creating stream manager");
      // Create a new stream manager for this serial
      auto stream = std::make_unique<OggStreamManager>(serial);
      stream->submitPage(&page);

      // Extract the first packet for codec identification
      ogg_packet packet;
      bool got_packet = stream->getPacket(&packet);
      Debug::log("ogg", "OggDemuxer::parseContainer() getPacket returned ",
                 got_packet, " bytes=", got_packet ? packet.bytes : 0);
      if (got_packet) {
        auto parser = CodecHeaderParser::create(&packet);
        Debug::log(
            "ogg",
            "OggDemuxer::parseContainer() CodecHeaderParser::create returned ",
            (parser != nullptr ? "valid parser" : "nullptr"));
        if (parser) {
          parser->parseHeader(&packet);
          m_parsers[serial] = std::move(parser);

          // Set primary stream to first audio stream
          if (!m_has_primary_serial) {
            m_primary_serial = serial;
            m_has_primary_serial = true;
            Debug::log(
                "ogg",
                "OggDemuxer::parseContainer() set primary_serial=", serial);
          }
        }
      }

      m_streams[serial] = std::move(stream);
      Debug::log("ogg", "OggDemuxer::parseContainer() streams count=",
                 m_streams.size(), " parsers count=", m_parsers.size());
    } else {
      // Not a BOS page - submit to appropriate stream
      Debug::log("ogg", "OggDemuxer::parseContainer() non-BOS page for serial=",
                 serial);
      auto it = m_streams.find(serial);
      if (it != m_streams.end()) {
        it->second->submitPage(&page);
        Debug::log(
            "ogg",
            "OggDemuxer::parseContainer() submitted page to stream manager");

        // Check if headers complete
        auto pit = m_parsers.find(serial);
        bool headers_complete_before =
            pit != m_parsers.end() ? pit->second->isHeadersComplete() : true;
        Debug::log("ogg",
                   "OggDemuxer::parseContainer() headers_complete_before=",
                   headers_complete_before);

        if (pit != m_parsers.end() && !pit->second->isHeadersComplete()) {
          ogg_packet packet;
          int packet_count = 0;
          while (it->second->getPacket(&packet)) {
            Debug::log("ogg", "OggDemuxer::parseContainer() got packet ",
                       packet_count, " bytes=", packet.bytes);
            pit->second->parseHeader(&packet);
            packet_count++;
            if (pit->second->isHeadersComplete()) {
              Debug::log("ogg",
                         "OggDemuxer::parseContainer() headers now complete "
                         "after packet ",
                         packet_count);
              break;
            }
          }
          if (packet_count == 0) {
            Debug::log("ogg", "OggDemuxer::parseContainer() no packets "
                              "available from stream manager");
          }
        }
      } else {
        Debug::log(
            "ogg",
            "OggDemuxer::parseContainer() no stream found for serial=", serial);
      }

      // Check if all streams have complete headers
      in_headers = false;
      for (auto &p : m_parsers) {
        bool complete = p.second->isHeadersComplete();
        Debug::log("ogg",
                   "OggDemuxer::parseContainer() parser for serial=", p.first,
                   " isHeadersComplete=", complete);
        if (!complete) {
          in_headers = true;
          break;
        }
      }
      Debug::log("ogg", "OggDemuxer::parseContainer() in_headers=", in_headers,
                 " (if false, will exit loop)");
    }
  }

  // Create tag from parsed VorbisComment data
  if (!m_streams.empty()) {
    createTagFromMetadata_unlocked();
  }

  return !m_streams.empty();
}

void OggDemuxer::createTagFromMetadata_unlocked() {
    // Get VorbisComment from primary stream's parser
    if (!m_has_primary_serial) {
        Debug::log("ogg", "OggDemuxer::createTagFromMetadata_unlocked: no primary stream");
        return;
    }
    
    auto pit = m_parsers.find(m_primary_serial);
    if (pit == m_parsers.end()) {
        Debug::log("ogg", "OggDemuxer::createTagFromMetadata_unlocked: no parser for primary stream");
        return;
    }
    
    OggVorbisComment ogg_comment = pit->second->getVorbisComment();
    if (ogg_comment.isEmpty()) {
        Debug::log("ogg", "OggDemuxer::createTagFromMetadata_unlocked: no VorbisComment data");
        return;
    }
    
    Debug::log("ogg", "OggDemuxer::createTagFromMetadata_unlocked: creating VorbisCommentTag from ",
               ogg_comment.fields.size(), " fields");
    
    // Convert OggVorbisComment to VorbisCommentTag format
    // VorbisCommentTag expects multi-valued map
    std::map<std::string, std::vector<std::string>> multi_fields = ogg_comment.fields;
    
    // Create VorbisCommentTag (no pictures in Ogg VorbisComment typically, 
    // METADATA_BLOCK_PICTURE would need base64 decoding which VorbisCommentTag handles)
    std::vector<PsyMP3::Tag::Picture> pictures;
    
    m_tag = std::make_unique<PsyMP3::Tag::VorbisCommentTag>(
        ogg_comment.vendor,
        multi_fields,
        pictures
    );
    
    Debug::log("ogg", "OggDemuxer::createTagFromMetadata_unlocked: VorbisCommentTag created successfully");
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
  std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
  std::vector<StreamInfo> result;

  for (auto &pair : m_parsers) {
    StreamInfo info;
    info.stream_id = pair.first;
    auto ci = pair.second->getCodecInfo();

    // Normalize codec name to lowercase for factory compatibility
    std::string codec_name = ci.codec_name;
    std::transform(codec_name.begin(), codec_name.end(), codec_name.begin(),
                   ::tolower);
    info.codec_name = codec_name;
    info.codec_type = "audio"; // All currently supported Ogg codecs are audio
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

  // Normalize codec name to lowercase for factory compatibility
  std::string codec_name = ci.codec_name;
  std::transform(codec_name.begin(), codec_name.end(), codec_name.begin(),
                 ::tolower);
  info.codec_name = codec_name;
  info.codec_type = "audio"; // All currently supported Ogg codecs are audio
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

  if (!m_has_primary_serial || m_streams.empty())
    return 0;

  // Use seeking engine to calculate
  auto it = m_streams.find(m_primary_serial);
  if (it == m_streams.end())
    return 0;

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
  if (it == m_streams.end())
    return 0;

  return it->second->getGranulePos();
}

MediaChunk OggDemuxer::readChunk_unlocked(uint32_t stream_id) {
  MediaChunk chunk;

  auto sit = m_streams.find(stream_id);
  if (sit == m_streams.end()) {
    m_eof = true;
    return chunk;
  }

  // Initialize to zero to avoid maybe-uninitialized warning in unity builds
  ogg_packet packet = {};

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
  if (!m_has_primary_serial)
    return false;

  auto sit = m_streams.find(m_primary_serial);
  if (sit == m_streams.end())
    return false;

  OggSeekingEngine engine(*m_sync, *sit->second, getSampleRate());

  double time_seconds = static_cast<double>(timestamp_ms) / 1000.0;
  bool success = engine.seekToTime(time_seconds);

  if (success) {
    m_eof = false;
    // Reset all streams
    for (auto &pair : m_streams) {
      pair.second->reset();
    }
  }

  return success;
}

long OggDemuxer::getSampleRate() const {
  auto pit = m_parsers.find(m_primary_serial);
  if (pit == m_parsers.end())
    return 48000;
  return pit->second->getCodecInfo().rate;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
