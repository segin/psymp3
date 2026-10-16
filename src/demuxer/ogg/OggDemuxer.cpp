/*
 * OggDemuxer.cpp - RFC-compliant Ogg Demuxer
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

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

namespace {
void populateCodecData(const CodecHeaderParser& parser,
                       StreamInfo& info)
{
    for (const auto& header : parser.getHeaderPackets()) {
        info.codec_data.insert(info.codec_data.end(), header.begin(), header.end());
    }
}

uint64_t computeTimestampSamples(const CodecInfo& codec_info,
                                 uint64_t granule_position)
{
    if (granule_position == static_cast<uint64_t>(-1)) {
        return 0;
    }

    std::string codec = codec_info.codec_name;
    std::transform(codec.begin(), codec.end(), codec.begin(), ::tolower);

    if (codec == "opus") {
        if (granule_position <= codec_info.pre_skip) {
            return 0;
        }
        return granule_position - codec_info.pre_skip;
    }

    return granule_position;
}

bool codecKeepsHeadersInBand(const CodecHeaderParser& parser)
{
    std::string codec = parser.getCodecInfo().codec_name;
    std::transform(codec.begin(), codec.end(), codec.begin(), ::tolower);

    // Vorbis decoding still expects the three setup packets to arrive through
    // the normal chunk stream before audio packets.
    return codec == "vorbis";
}

} // namespace

bool OggDemuxer::resetForPlayback_unlocked() {
  if (!m_sync->seek(0)) {
    Debug::log("ogg", "OggDemuxer::resetForPlayback_unlocked() failed to seek to start");
    return false;
  }

  for (auto& pair : m_streams) {
    pair.second->reset();
  }

  std::map<int, size_t> headers_remaining;
  for (const auto& pair : m_parsers) {
    headers_remaining[pair.first] =
        codecKeepsHeadersInBand(*pair.second) ? 0 : pair.second->getHeaderPackets().size();
  }

  ogg_page page;
  while (true) {
    bool all_headers_consumed = true;
    for (const auto& pair : headers_remaining) {
      if (pair.second != 0) {
        all_headers_consumed = false;
        break;
      }
    }

    if (all_headers_consumed) {
      auto primary = m_streams.find(m_primary_serial);
      if (primary == m_streams.end()) {
        break;
      }

      ogg_packet packet{};
      if (primary->second->peekPacket(&packet) == 1) {
        break;
      }
    }

    int result = m_sync->getNextPage(&page);
    if (result != 1) {
      break;
    }

    int serial = ogg_page_serialno(&page);
    auto stream_it = m_streams.find(serial);
    if (stream_it == m_streams.end()) {
      continue;
    }

    stream_it->second->submitPage(&page);

    auto remaining_it = headers_remaining.find(serial);
    if (remaining_it == headers_remaining.end()) {
      continue;
    }

    while (remaining_it->second > 0) {
      ogg_packet packet{};
      int packet_result = stream_it->second->getPacket(&packet);
      if (packet_result != 1) {
        break;
      }
      remaining_it->second--;
    }
  }

  m_eof = false;
  return true;
}

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
    : Demuxer(std::move(handler)), m_primary_serial(0), m_has_primary_serial(false), m_eof(false),
      m_duration_calculated(false), m_cached_duration(0), m_calculating_duration(false) {

  // Demuxer base class owns the IOHandler.
  // OggSyncManager takes a raw pointer to use it.
  m_sync = std::make_unique<OggSyncManager>(m_handler.get());
  /*
   * A station that sends something other than Ogg must not be searched for
   * ever: 256 KiB without a page ends it.
   */
  if (isLiveSource()) m_sync->setSearchLimit(kLivePageSearchBytes);
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
      if (!m_streams.empty()) {
          createTagFromMetadata_unlocked();
          calculateInitialDuration_unlocked();
      } else {
          reportError("Format", "Could not find a valid Ogg bitstream",
                      result, DemuxerErrorRecovery::NONE);
      }
      // Mirror into the base class flag so the generic isParsed() API
      // agrees; m_ogg_mutex is distinct from the base state mutex, so
      // taking it inside setParsed() cannot self-deadlock.
      if (!m_streams.empty()) setParsed(true);
      return !m_streams.empty(); // OK if we found at least one stream
    }
    page_count++;
    /*
     * Headers that never complete -- pages of streams PsyMP3 does not play,
     * or a parser waiting for a packet that never comes -- would hold a
     * live load for ever; a megabyte of a station is more than any header
     * set needs.
     */
    if (isLiveSource() && m_sync->getLogicalPosition() > static_cast<int64_t>(kLiveHeaderBytes)) {
      Debug::log("ogg", "OggDemuxer::parseContainer() no complete headers in ", kLiveHeaderBytes,
                 " bytes of a live stream; giving up");
      reportError("Format", "Live Ogg stream's headers never completed", 0, DemuxerErrorRecovery::NONE);
      return false;
    }

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

      // Extract the first packet for codec identification.
      // getPacket() returns 1 on success, 0 for need-more-data, and -1 for a
      // data gap (packet left untouched). Only 1 means *packet is valid; a
      // bool coercion would treat -1 as success and parse an uninitialized
      // packet.
      ogg_packet packet;
      bool got_packet = (stream->getPacket(&packet) == 1);
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
          } else {
            // If current primary isn't clearly audio (e.g. unknown codec), 
            // and this one is, prefer this one.
            // (Currently all our supported Ogg codecs are audio, so this is mostly defensive)
            auto current_info = m_parsers[m_primary_serial]->getCodecInfo();
            auto new_info = m_parsers[serial]->getCodecInfo();
            if (current_info.codec_name == "Unknown" && new_info.codec_name != "Unknown") {
               m_primary_serial = serial;
               Debug::log("ogg", "OggDemuxer::parseContainer() switched primary_serial to ", serial, " (better codec)");
            }
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
          // Only 1 means *packet is valid; -1 (data gap) must not be parsed.
          while (it->second->getPacket(&packet) == 1) {
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
    calculateInitialDuration_unlocked();
    resetForPlayback_unlocked();
  }

  if (!m_streams.empty()) setParsed(true);
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

void OggDemuxer::calculateInitialDuration_unlocked() {
    // A live stream (internet radio) has no duration, and finding the last
    // granule would read it until the station stopped. Its positions count
    // from where the listener joined, not from the station's first granule.
    if (m_handler && m_handler->isLiveStream()) {
        m_cached_duration = 0;
        m_start_granule = 0;
        m_duration_calculated = true;
        Debug::log("ogg", "OggDemuxer::calculateInitialDuration_unlocked: live stream, no duration");
        return;
    }
    // Pre-calculate duration to avoid blocking UI thread later
    if (m_has_primary_serial && !m_streams.empty()) {
        auto it = m_streams.find(m_primary_serial);
        if (it != m_streams.end()) {
            Debug::log("ogg", "OggDemuxer::calculateInitialDuration_unlocked: Calculating duration...");
            OggSeekingEngine engine(*m_sync, *it->second, getSampleRate());
            m_cached_duration = static_cast<uint64_t>(engine.calculateDuration() * 1000.0);
            m_start_granule = engine.getStartGranule();
            // A chained file lasts as long as all its playable links.
            scanChainedLinks_unlocked();
            m_duration_calculated = true;
            Debug::log("ogg", "OggDemuxer::calculateInitialDuration_unlocked: Calculated duration: ", m_cached_duration, "ms");
        }
    }
}

/**
 * @brief Find a file's chained links and its duration across them; see the
 *        declaration.
 */
void OggDemuxer::scanChainedLinks_unlocked() {
  m_links.clear();
  m_current_link = 0;
  if (!m_handler || isLiveSource() || !m_has_primary_serial) return;

  const auto saved = m_handler->tell();
  m_handler->seek(0, SEEK_END);
  const int64_t size = static_cast<int64_t>(m_handler->tell());
  if (size <= 0) {
    m_handler->seek(saved, SEEK_SET);
    return;
  }

  /*
   * The common case first, and cheaply: a file whose last page belongs to
   * its first stream is one link. Only one that ends in another stream's
   * page is read through.
   */
  {
    const int64_t tail = std::min<int64_t>(size, 65536);
    std::vector<char> buf(static_cast<size_t>(tail));
    m_handler->seek(size - tail, SEEK_SET);
    const size_t got = m_handler->read(buf.data(), 1, buf.size());
    ogg_sync_state oy;
    ogg_sync_init(&oy);
    char* dst = ogg_sync_buffer(&oy, static_cast<long>(got));
    std::memcpy(dst, buf.data(), got);
    ogg_sync_wrote(&oy, static_cast<long>(got));
    ogg_page og;
    int last_serial = m_primary_serial;
    bool any = false;
    for (;;) {
      const long r = ogg_sync_pageseek(&oy, &og);
      if (r == 0) break;
      if (r > 0) {
        last_serial = ogg_page_serialno(&og);
        any = true;
      }
    }
    ogg_sync_clear(&oy);
    if (!any || last_serial == m_primary_serial) {
      m_handler->seek(saved, SEEK_SET);
      return;
    }
  }

  /** @brief A link as the scan finds it. */
  struct Found {
    int64_t offset = 0;
    bool has_audio = false;    // a stream PsyMP3 plays was found (serials can be negative ints)
    int serial = 0;            // that stream's serial
    std::string codec;
    uint32_t rate = 0;
    uint32_t channels = 0;
    uint64_t pre_skip = 0;
    uint64_t last_granule = 0;
  };
  std::vector<Found> found;
  Found current;
  bool have_current = false;
  bool in_bos_group = false;

  ogg_sync_state oy;
  ogg_sync_init(&oy);
  m_handler->seek(0, SEEK_SET);
  int64_t offset = 0;  // file offset of the next byte the sync layer will look at
  ogg_page og;
  for (;;) {
    const long r = ogg_sync_pageseek(&oy, &og);
    if (r == 0) {
      char* dst = ogg_sync_buffer(&oy, 65536);
      const size_t got = m_handler->read(dst, 1, 65536);
      if (got == 0) break;
      ogg_sync_wrote(&oy, static_cast<long>(got));
      continue;
    }
    if (r < 0) {
      offset += -r;
      continue;
    }
    const int64_t page_offset = offset;
    offset += r;
    const int serial = ogg_page_serialno(&og);
    if (ogg_page_bos(&og)) {
      if (!in_bos_group) {
        // A BOS after data pages: the next link begins here.
        if (have_current) found.push_back(current);
        current = Found();
        current.offset = page_offset;
        have_current = true;
        in_bos_group = true;
      }
      if (!current.has_audio) {
        ogg_stream_state os;
        ogg_stream_init(&os, serial);
        ogg_stream_pagein(&os, &og);
        ogg_packet packet{};
        if (ogg_stream_packetout(&os, &packet) == 1) {
          if (auto parser = CodecHeaderParser::create(&packet)) {
            parser->parseHeader(&packet);
            const auto info = parser->getCodecInfo();
            current.has_audio = true;
            current.serial = serial;
            current.codec = info.codec_name;
            std::transform(current.codec.begin(), current.codec.end(), current.codec.begin(), ::tolower);
            current.rate = info.rate;
            current.channels = info.channels;
            current.pre_skip = current.codec == "opus" ? info.pre_skip : 0;
          }
        }
        ogg_stream_clear(&os);
      }
    } else {
      in_bos_group = false;
      if (have_current && current.has_audio && serial == current.serial && ogg_page_granulepos(&og) >= 0) {
        current.last_granule = static_cast<uint64_t>(ogg_page_granulepos(&og));
      }
    }
  }
  if (have_current) found.push_back(current);
  ogg_sync_clear(&oy);
  m_handler->seek(saved, SEEK_SET);

  /*
   * The playable chain: links sharing the first one's codec, rate and
   * channels, as the codec is rebuilt between links but the audio device is
   * not. One that differs, or has no stream PsyMP3 plays, ends it there.
   */
  if (found.size() < 2 || !found[0].has_audio || found[0].serial != m_primary_serial) return;
  uint64_t start = 0;
  for (const Found& link : found) {
    if (!link.has_audio || link.codec != found[0].codec || link.rate != found[0].rate ||
        link.channels != found[0].channels) {
      Debug::log("ogg", "OggDemuxer: chained file's link at ", link.offset,
                 " cannot follow the first (other codec or format); the file ends before it");
      break;
    }
    ChainLink entry;
    entry.offset = link.offset;
    entry.serial = link.serial;
    entry.pre_skip = link.pre_skip;
    entry.start_samples = start;
    entry.length_samples = link.last_granule > link.pre_skip ? link.last_granule - link.pre_skip : 0;
    start += entry.length_samples;
    m_links.push_back(entry);
  }
  if (m_links.size() < 2) {
    m_links.clear();
    return;
  }
  const uint64_t rate = found[0].codec == "opus" ? 48000 : found[0].rate;
  if (rate != 0) {
    m_cached_duration = start * 1000 / rate;
  }
  Debug::log("ogg", "OggDemuxer: chained file, ", m_links.size(), " links, ", m_cached_duration.load(), " ms in all");
}

/**
 * @brief Make link @p index the one read.
 * @param index Into m_links.
 * @return false if it cannot be set up.
 */
bool OggDemuxer::setUpLink_unlocked(size_t index) {
  if (index >= m_links.size()) return false;
  if (!m_sync->seek(m_links[index].offset)) return false;
  ogg_page page;
  if (m_sync->getNextPage(&page) != 1 || !ogg_page_bos(&page)) return false;
  if (!beginNextLink_unlocked(&page)) {
    m_eof = false; // a seek failing is not the end of the file
    return false;
  }
  m_current_link = index;
  return true;
}

/**
 * @brief Move a later link's chunk onto the file's timeline.
 * @param chunk The chunk.
 */
void OggDemuxer::applyLinkOffset_unlocked(MediaChunk& chunk) const {
  if (m_links.size() < 2 || m_current_link == 0 || m_current_link >= m_links.size()) return;
  if (chunk.granule_position == static_cast<uint64_t>(-1)) return;
  const ChainLink& link = m_links[m_current_link];
  // timestamp_samples is already the link's own, its pre-skip taken off.
  chunk.timestamp_samples += link.start_samples;
  // A granule the first link's codec would give the same sample.
  chunk.granule_position = chunk.timestamp_samples + m_links[0].pre_skip;
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
    // 0 when the codec states none (Opus); DemuxedStream then derives an
    // average from the file size and duration.
    info.bitrate = static_cast<uint32_t>(ci.nominal_bitrate > 0 ? ci.nominal_bitrate : 0);
    info.duration_ms = m_cached_duration.load();
    if (info.duration_ms > 0 && info.sample_rate > 0) {
      info.duration_samples = (info.duration_ms * info.sample_rate) / 1000;
    }
    populateCodecData(*pair.second, info);
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
  // 0 when the codec states none (Opus); DemuxedStream then derives an
  // average from the file size and duration.
  info.bitrate = static_cast<uint32_t>(ci.nominal_bitrate > 0 ? ci.nominal_bitrate : 0);
  info.duration_ms = m_cached_duration.load();
  if (info.duration_ms > 0 && info.sample_rate > 0) {
    info.duration_samples = (info.duration_ms * info.sample_rate) / 1000;
  }
  populateCodecData(*pit->second, info);
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
  // Fast path check without lock (atomics)
  if (m_duration_calculated) {
    return m_cached_duration;
  }

  std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
  // Duration is pre-calculated in parseContainer() to avoid blocking
  return m_cached_duration;
}

uint64_t OggDemuxer::getPosition() const {
  std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
  if (!m_has_primary_serial) {
    return 0;
  }

  return granuleToMs(getGranulePosition(m_primary_serial), m_primary_serial);
}

uint64_t OggDemuxer::getGranulePosition(uint32_t stream_id) const {
  std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);

  auto it = m_streams.find(stream_id);
  if (it == m_streams.end())
    return 0;

  return it->second->getGranulePos();
}

/**
 * @brief Map a page's serial to the stream it feeds: a later chained link's
 *        pages go to the primary stream's key.
 * @param serial The page's serial number.
 * @return The key in m_streams.
 */
int OggDemuxer::streamKeyForSerial_unlocked(int serial) const {
  auto it = m_serial_alias.find(serial);
  return it == m_serial_alias.end() ? serial : it->second;
}

/**
 * @brief Set the next chained link up in place of the primary stream. Reads
 *        pages (skipping other serials) until the link's headers are
 *        complete, replaces the primary key's stream and parser, records the
 *        link's artist and title, and arranges for the consumer to rebuild its
 *        codec: in-band headers (Vorbis) are queued as chunks, the first one
 *        flagged new_link; otherwise the first audio chunk is flagged.
 * @param bos_page The link's BOS page.
 * @return false if the link is unusable; m_eof is set then.
 */
bool OggDemuxer::beginNextLink_unlocked(ogg_page* bos_page) {
  /*
   * A link opens with the BOS pages of all its streams. Its first need not
   * be one PsyMP3 plays (a Skeleton stream comes first by convention), so
   * each BOS page of the group is tried in turn. And a BOS arriving while
   * the chosen stream's headers are still being read -- a reconnection
   * starting the stream over -- restarts the link from that page.
   */
  ogg_page candidate = *bos_page;
  int serial = 0;
  std::unique_ptr<OggStreamManager> stream;
  std::unique_ptr<CodecHeaderParser> parser;
  std::vector<std::vector<uint8_t>> headers;
  ogg_packet packet{};
  bool restart = true;
  while (restart) {
    restart = false;
    serial = ogg_page_serialno(&candidate);
    stream = std::make_unique<OggStreamManager>(serial);
    stream->submitPage(&candidate);
    parser.reset();
    headers.clear();
    if (stream->getPacket(&packet) == 1) {
      parser = CodecHeaderParser::create(&packet);
    }
    if (!parser) {
      ogg_page next;
      if (m_sync->getNextPage(&next) == 1 && ogg_page_bos(&next)) {
        Debug::log("ogg", "OggDemuxer: next chained link's stream ", static_cast<unsigned>(serial),
                   " is not one PsyMP3 plays; trying the link's next");
        candidate = next;
        restart = true;
        continue;
      }
      Debug::log("ogg", "OggDemuxer: next chained link has no codec PsyMP3 knows (serial ",
                 static_cast<unsigned>(serial), "); ending the stream");
      m_eof = true;
      return false;
    }
    headers.emplace_back(packet.packet, packet.packet + packet.bytes);
    parser->parseHeader(&packet);

    while (!parser->isHeadersComplete()) {
      const int result = stream->getPacket(&packet);
      if (result == 1) {
        headers.emplace_back(packet.packet, packet.packet + packet.bytes);
        parser->parseHeader(&packet);
        continue;
      }
      if (result < 0) {
        continue; // a gap; the next packet follows
      }
      ogg_page page;
      if (m_sync->getNextPage(&page) != 1) {
        Debug::log("ogg", "OggDemuxer: stream ended inside the next chained link's headers (serial ",
                   static_cast<unsigned>(serial), ")");
        m_eof = true;
        return false;
      }
      if (ogg_page_serialno(&page) == serial && !ogg_page_bos(&page)) {
        stream->submitPage(&page);
      } else if (ogg_page_bos(&page) && isLiveSource()) {
        Debug::log("ogg", "OggDemuxer: a new BOS while reading link headers; starting the link over");
        candidate = page;
        restart = true;
        break;
      }
    }
  }

  const bool in_band = codecKeepsHeadersInBand(*parser);
  const int key = m_primary_serial;
  m_streams[key] = std::move(stream);
  m_parsers[key] = std::move(parser);
  m_serial_alias[serial] = key;

  // The tag describes the stream as opened and is handed out by reference,
  // so it is not replaced here; the link's own artist and title go to
  // getStreamTitle().
  const OggVorbisComment comment = m_parsers[key]->getVorbisComment();
  auto field = [&comment](const char* name) {
    auto it = comment.fields.find(name);
    return (it == comment.fields.end() || it->second.empty()) ? std::string() : it->second.front();
  };
  const std::string artist = field("ARTIST");
  const std::string title = field("TITLE");
  m_link_title = (!artist.empty() && !title.empty()) ? artist + " - " + title : artist + title;

  // A chained file has moved on to its next link (a seek sets the link
  // itself, after this).
  if (!isLiveSource() && m_current_link + 1 < m_links.size()) {
    ++m_current_link;
  }
  const uint64_t link_start = m_current_link < m_links.size() ? m_links[m_current_link].start_samples : 0;

  m_link_chunks.clear();
  if (in_band) {
    for (auto& header : headers) {
      MediaChunk chunk;
      chunk.stream_id = static_cast<uint32_t>(key);
      chunk.data = std::move(header);
      chunk.new_link = m_link_chunks.empty();
      // Where on the file's timeline the link begins (a seek moves it on).
      chunk.timestamp_samples = link_start;
      m_link_chunks.push_back(std::move(chunk));
    }
    m_link_flag_pending = false;
  } else {
    m_link_flag_pending = true;
  }
  Debug::log("ogg", "OggDemuxer: next chained link, ", m_parsers[key]->getCodecInfo().codec_name,
             ", serial ", static_cast<unsigned>(serial), ": '", m_link_title, "'");
  return true;
}

/**
 * @brief The current chained link's "Artist - Title".
 * @return It, or "" before the first link change.
 */
std::string OggDemuxer::getStreamTitle() const {
  std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
  return m_link_title;
}

MediaChunk OggDemuxer::readChunk_unlocked(uint32_t stream_id) {
  MediaChunk chunk;

  // A new chained link's in-band headers go out before anything else.
  if (!m_link_chunks.empty() && m_has_primary_serial &&
      stream_id == static_cast<uint32_t>(m_primary_serial)) {
    chunk = std::move(m_link_chunks.front());
    m_link_chunks.pop_front();
    return chunk;
  }

  auto sit = m_streams.find(stream_id);
  if (sit == m_streams.end()) {
    m_eof = true;
    return chunk;
  }

  // Initialize to zero to avoid maybe-uninitialized warning in unity builds
  ogg_packet packet = {};

  while (true) {
    int packet_result = sit->second->getPacket(&packet);
    if (packet_result == 1) {
      break;
    }

    if (packet_result < 0) {
      chunk.stream_id = stream_id;
      chunk.packet_lost = true;
      chunk.granule_position = getGranulePosition(stream_id);

      auto pit = m_parsers.find(stream_id);
      if (pit != m_parsers.end()) {
        chunk.timestamp_samples =
            computeTimestampSamples(pit->second->getCodecInfo(), chunk.granule_position);
        int64_t base = m_start_granule.load();
        if (base > 0 && stream_id == static_cast<uint32_t>(m_primary_serial)) {
          chunk.timestamp_samples =
              (chunk.timestamp_samples >= static_cast<uint64_t>(base))
                  ? chunk.timestamp_samples - static_cast<uint64_t>(base)
                  : 0;
        }
      }
      if (stream_id == static_cast<uint32_t>(m_primary_serial)) {
        applyLinkOffset_unlocked(chunk);
      }

      return chunk;
    }

    // Need more data - read next page
    ogg_page page;
    int result = m_sync->getNextPage(&page);
    if (result != 1) {
      m_eof = true;
      return chunk;
    }

    int serial = ogg_page_serialno(&page);
    const int key = streamKeyForSerial_unlocked(serial);
    auto it = m_streams.find(key);
    /*
     * A new logical stream beginning where the primary one was: the next
     * link of a chained stream -- or, after a station's connection was
     * dropped and made again, the stream starting over, perhaps under the
     * same serial. Every packet of the old link has gone out already, since
     * pages are read only when the stream has none. A chained file goes on
     * into the links its scan found playable (m_links); past them it ends.
     */
    const bool chain_continues = isLiveSource() || m_current_link + 1 < m_links.size();
    const bool next_link = ogg_page_bos(&page) && m_has_primary_serial && chain_continues &&
                           stream_id == static_cast<uint32_t>(m_primary_serial) &&
                           (it == m_streams.end() || key == m_primary_serial);
    if (!next_link && it != m_streams.end()) {
      it->second->submitPage(&page);
    } else if (next_link) {
      if (!beginNextLink_unlocked(&page)) {
        return chunk;
      }
      if (!m_link_chunks.empty()) {
        chunk = std::move(m_link_chunks.front());
        m_link_chunks.pop_front();
        return chunk;
      }
      sit = m_streams.find(stream_id);
    }

  }

  // Got a packet - copy data to chunk
  chunk.data.assign(packet.packet, packet.packet + packet.bytes);
  chunk.stream_id = stream_id;
  chunk.granule_position =
      (packet.granulepos >= 0) ? static_cast<uint64_t>(packet.granulepos)
                               : static_cast<uint64_t>(-1);
  chunk.end_of_stream = packet.e_o_s != 0;
  if (m_link_flag_pending && m_has_primary_serial &&
      stream_id == static_cast<uint32_t>(m_primary_serial)) {
    chunk.new_link = true;
    m_link_flag_pending = false;
  }

  auto pit = m_parsers.find(stream_id);
  if (pit != m_parsers.end()) {
    chunk.timestamp_samples =
        computeTimestampSamples(pit->second->getCodecInfo(), chunk.granule_position);
  }
  if (stream_id == static_cast<uint32_t>(m_primary_serial)) {
    applyLinkOffset_unlocked(chunk);
  }

  return chunk;
}

bool OggDemuxer::seekTo_unlocked(uint64_t timestamp_ms) {
  if (!m_has_primary_serial)
    return false;
  // Internet radio plays from where it is; there is nothing to seek in.
  if (m_handler && m_handler->isLiveStream())
    return false;

  /*
   * A chained file: the target lies in one of its links. When that is not
   * the link being read, its stream and codec headers are set up first (and
   * the consumer rebuilds its codec, as at a link change); then the seek
   * runs inside that link, at the target's time within it.
   */
  uint64_t link_start_ms = 0;
  size_t target_link = m_current_link;
  if (m_links.size() > 1) {
    const uint64_t rate = static_cast<uint64_t>(getSampleRate());
    const uint64_t out_rate = rate == 0 ? 48000 : rate;
    auto codec_name = [this]() {
      auto pit = m_parsers.find(m_primary_serial);
      std::string c = pit == m_parsers.end() ? std::string() : pit->second->getCodecInfo().codec_name;
      std::transform(c.begin(), c.end(), c.begin(), ::tolower);
      return c;
    };
    const uint64_t timeline_rate = codec_name() == "opus" ? 48000 : out_rate;
    const uint64_t target_samples = timestamp_ms * timeline_rate / 1000;
    target_link = 0;
    for (size_t i = 1; i < m_links.size(); ++i) {
      if (m_links[i].start_samples <= target_samples) target_link = i;
    }
    if (target_link != m_current_link && !setUpLink_unlocked(target_link)) {
      return false;
    }
    link_start_ms = m_links[target_link].start_samples * 1000 / timeline_rate;
  }

  auto sit = m_streams.find(m_primary_serial);
  if (sit == m_streams.end())
    return false;

  OggSeekingEngine engine(*m_sync, *sit->second, getSampleRate());

  double time_seconds = static_cast<double>(timestamp_ms - std::min(timestamp_ms, link_start_ms)) / 1000.0;
  bool success = engine.seekToTime(time_seconds);

  if (success) {
    m_eof = false;
    // Reset all streams
    for (auto &pair : m_streams) {
      pair.second->reset();
    }

    // reset() clears the granule, and a zero would make the first frame after
    // the seek look like the start of the file. A seek lands on a page
    // boundary, so seed the primary stream with the granule that page begins
    // at: the frames that follow then carry honest positions, and the caller
    // can tell how much of the landing page precedes the sample it asked for.
    sit->second->setGranulePos(engine.getLandingGranule());

    /*
     * A seek into another link of a chained file queued that link's headers
     * to rebuild the codec. They are the first chunks the consumer sees, and
     * it re-anchors on the first chunk's timestamp: give them the landing
     * point on the file's timeline, not the link's start.
     */
    if (!m_link_chunks.empty() && m_links.size() > 1) {
      auto pit = m_parsers.find(m_primary_serial);
      if (pit != m_parsers.end()) {
        const uint64_t landing = computeTimestampSamples(pit->second->getCodecInfo(), engine.getLandingGranule()) +
                                 m_links[target_link].start_samples;
        for (MediaChunk& chunk : m_link_chunks) chunk.timestamp_samples = landing;
      }
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

uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);

    if (granule == static_cast<uint64_t>(-1)) {
        return 0;
    }
    
    // Check legacy test streams first
    auto tit = m_test_streams.find(stream_id);
    if (tit != m_test_streams.end()) {
        uint32_t rate = tit->second.sample_rate;
        if (rate == 0) return 0;
        
        if (tit->second.codec_name == "opus") {
            if (granule < tit->second.pre_skip) return 0;
            uint64_t g = granule - tit->second.pre_skip;
            // Split the division so granule * 1000 cannot overflow uint64.
            return (g / 48000) * 1000 + (g % 48000) * 1000 / 48000;
        }
        return (granule / rate) * 1000 + (granule % rate) * 1000 / rate;
    }

    auto pit = m_parsers.find(stream_id);
    if (pit == m_parsers.end()) return 0;

    auto ci = pit->second->getCodecInfo();
    if (ci.rate == 0) return 0;

    // Handle codec-specific granule interpretation
    std::string codec = ci.codec_name;
    std::transform(codec.begin(), codec.end(), codec.begin(), ::tolower);

    if (codec == "opus") {
        if (granule < ci.pre_skip) return 0;
        uint64_t g = granule - ci.pre_skip;
        // Split the division so granule * 1000 cannot overflow uint64.
        return (g / 48000) * 1000 + (g % 48000) * 1000 / 48000;
    }

    // Generic sample-based (Vorbis, FLAC, Speex). For the primary stream,
    // rebase onto the stream start so a mid-stream capture reports positions
    // from zero (the base is zero for a normal stream, so this is a no-op).
    uint64_t rel = granule;
    int64_t base = m_start_granule.load();
    if (base > 0 && stream_id == static_cast<uint32_t>(m_primary_serial)) {
        rel = (granule >= static_cast<uint64_t>(base)) ? granule - static_cast<uint64_t>(base) : 0;
    }
    return (rel / ci.rate) * 1000 + (rel % ci.rate) * 1000 / ci.rate;
}

uint64_t OggDemuxer::msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    
    // Check legacy test streams first
    auto tit = m_test_streams.find(stream_id);
    if (tit != m_test_streams.end()) {
        uint32_t rate = tit->second.sample_rate;
        if (rate == 0) return 0;
        
        if (tit->second.codec_name == "opus") {
            return (timestamp_ms * 48000 / 1000) + tit->second.pre_skip;
        }
        return timestamp_ms * rate / 1000;
    }

    auto pit = m_parsers.find(stream_id);
    if (pit == m_parsers.end()) return 0;

    auto ci = pit->second->getCodecInfo();
    if (ci.rate == 0) return 0;

    std::string codec = ci.codec_name;
    std::transform(codec.begin(), codec.end(), codec.begin(), ::tolower);

    if (codec == "opus") {
        return (timestamp_ms * 48000 / 1000) + ci.pre_skip;
    }

    // Convert a stream-relative time back to an absolute granule by re-adding
    // the primary stream's base (zero for a normal stream), so seeking targets
    // the correct sample in a mid-stream capture.
    uint64_t absolute = timestamp_ms * ci.rate / 1000;
    int64_t base = m_start_granule.load();
    if (base > 0 && stream_id == static_cast<uint32_t>(m_primary_serial)) {
        absolute += static_cast<uint64_t>(base);
    }
    return absolute;
}

int OggDemuxer::fetchAndProcessPacket() {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    ogg_page page;
    int result = m_sync->getNextPage(&page);
    if (result != 1) return -1;
    
    int serial = ogg_page_serialno(&page);
    auto it = m_streams.find(serial);
    if (it != m_streams.end()) {
        it->second->submitPage(&page);
        
        // For legacy test satisfaction:
        auto tit = m_test_streams.find(serial);
        if (tit != m_test_streams.end()) {
            ogg_packet packet;
            // Only 1 means *packet is valid; -1 (data gap) must not be queued.
            while (it->second->getPacket(&packet) == 1) {
                // ogg_packet::packet borrows libogg's internal stream buffer,
                // which is overwritten by the next getPacket() in this very loop.
                // Deep-copy the bytes into an owning OggPacket before queueing so
                // the queue never holds a dangling pointer.
                OggPacket queued;
                if (packet.packet && packet.bytes > 0) {
                    queued.data.assign(packet.packet, packet.packet + packet.bytes);
                }
                queued.granule_position = packet.granulepos;
                queued.is_keyframe = false;
                tit->second.m_packet_queue.push_back(std::move(queued));
                
                // If ID header, set codec name for test
                if (ogg_page_bos(&page)) {
                    auto parser = CodecHeaderParser::create(&packet);
                    if (parser) {
                        auto ci = parser->getCodecInfo();
                        tit->second.codec_name = ci.codec_name;
                        tit->second.sample_rate = ci.rate;
                        tit->second.channels = ci.channels;
                        tit->second.headers_complete = parser->isHeadersComplete();
                    }
                }
            }
        }
        
        return 1;
    }
    return 0;
}

void OggDemuxer::fillPacketQueue([[maybe_unused]] uint32_t stream_id) {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    // Read some pages until we get at least some packets for this stream
    for (int i = 0; i < 50; ++i) {
        if (fetchAndProcessPacket() < 0) break;
    }
}

bool OggDemuxer::performMemoryAudit() {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    return performMemoryAudit_unlocked();
}

void OggDemuxer::enforceMemoryLimits() {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    enforceMemoryLimits_unlocked();
}

bool OggDemuxer::validateLiboggStructures() {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    return validateLiboggStructures_unlocked();
}

void OggDemuxer::performPeriodicMaintenance() {
    std::lock_guard<std::recursive_mutex> lock(m_ogg_mutex);
    performPeriodicMaintenance_unlocked();
}

bool OggDemuxer::performMemoryAudit_unlocked() {
    // Audit implementation - check if any stream has excessive packets
    for (const auto& pair : m_test_streams) {
        if (pair.second.m_packet_queue.size() > MAX_PACKET_QUEUE_SIZE * 2) {
            return false;
        }
    }
    return true;
}

void OggDemuxer::enforceMemoryLimits_unlocked() {
    // Limit m_test_streams packet queues
    for (auto& pair : m_test_streams) {
        while (pair.second.m_packet_queue.size() > MAX_PACKET_QUEUE_SIZE) {
            pair.second.m_packet_queue.pop_front();
        }
    }
}

bool OggDemuxer::validateLiboggStructures_unlocked() {
    if (!m_sync) return false;
    // Basic validation
    return true;
}

void OggDemuxer::performPeriodicMaintenance_unlocked() {
    enforceMemoryLimits_unlocked();
    validateLiboggStructures_unlocked();
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
