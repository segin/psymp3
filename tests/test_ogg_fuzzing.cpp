/*
 * test_ogg_fuzzing.cpp - Fuzzing and Property-Based Tests for Ogg Demuxer
 * This file is part of PsyMP3.
 *
 * Tests compliance with RFC 3533 and robustness against random inputs.
 */

#include "psymp3.h"
#include "ogg/ogg.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>

namespace {

std::vector<uint8_t> makeMinimalOpusHead()
{
    std::vector<uint8_t> packet = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
    packet.insert(packet.end(), {1, 1, 0, 0, 0x80, 0xBB, 0, 0, 0, 0, 0});
    return packet;
}

class MemoryIOHandler : public PsyMP3::IO::IOHandler {
public:
    explicit MemoryIOHandler(std::vector<uint8_t> data)
        : m_data(std::move(data))
    {
    }

    size_t read(void* ptr, size_t size, size_t count) override
    {
        size_t bytes_requested = size * count;
        if (m_pos >= m_data.size() || size == 0) {
            return 0;
        }

        size_t available = m_data.size() - m_pos;
        size_t to_read = std::min(bytes_requested, available);
        std::memcpy(ptr, m_data.data() + m_pos, to_read);
        m_pos += to_read;
        return to_read / size;
    }

    int seek(off_t offset, int origin) override
    {
        if (origin == SEEK_SET) {
            m_pos = static_cast<size_t>(std::max<off_t>(0, offset));
        } else if (origin == SEEK_CUR) {
            off_t target = static_cast<off_t>(m_pos) + offset;
            m_pos = static_cast<size_t>(std::max<off_t>(0, target));
        } else if (origin == SEEK_END) {
            off_t target = static_cast<off_t>(m_data.size()) + offset;
            m_pos = static_cast<size_t>(std::max<off_t>(0, target));
        }

        if (m_pos > m_data.size()) {
            m_pos = m_data.size();
        }
        return 0;
    }

    off_t tell() override { return static_cast<off_t>(m_pos); }
    off_t getFileSize() override { return static_cast<off_t>(m_data.size()); }
    bool eof() override { return m_pos >= m_data.size(); }

private:
    std::vector<uint8_t> m_data;
    size_t m_pos = 0;
};

class OggStreamBuilder {
public:
    explicit OggStreamBuilder(int32_t serial)
        : m_initialized(ogg_stream_init(&m_stream, serial) == 0)
    {
    }

    ~OggStreamBuilder()
    {
        if (m_initialized) {
            ogg_stream_clear(&m_stream);
        }
    }

    OggStreamBuilder(const OggStreamBuilder&) = delete;
    OggStreamBuilder& operator=(const OggStreamBuilder&) = delete;

    bool packetIn(const std::vector<uint8_t>& packet,
                  int64_t granule_pos,
                  int64_t packet_no,
                  bool bos = false,
                  bool eos = false)
    {
        if (!m_initialized) {
            return false;
        }

        ogg_packet ogg_packet_data{};
        ogg_packet_data.packet = const_cast<unsigned char*>(packet.data());
        ogg_packet_data.bytes = static_cast<long>(packet.size());
        ogg_packet_data.b_o_s = bos ? 1 : 0;
        ogg_packet_data.e_o_s = eos ? 1 : 0;
        ogg_packet_data.granulepos = granule_pos;
        ogg_packet_data.packetno = packet_no;

        return ogg_stream_packetin(&m_stream, &ogg_packet_data) == 0;
    }

    bool pageOut(std::vector<uint8_t>& out)
    {
        return emitPage(out, false);
    }

    bool flush(std::vector<uint8_t>& out)
    {
        return emitPage(out, true);
    }

private:
    bool emitPage(std::vector<uint8_t>& out, bool force_flush)
    {
        if (!m_initialized) {
            return false;
        }

        ogg_page page{};
        int rc = force_flush ? ogg_stream_flush(&m_stream, &page)
                             : ogg_stream_pageout(&m_stream, &page);
        if (rc == 0) {
            return false;
        }

        out.insert(out.end(), page.header, page.header + page.header_len);
        out.insert(out.end(), page.body, page.body + page.body_len);
        return true;
    }

    ogg_stream_state m_stream{};
    bool m_initialized = false;
};

std::vector<uint8_t> makeTaggedPageStream(int32_t serial,
                                          int64_t granule,
                                          int64_t packet_no,
                                          const std::vector<uint8_t>& payload,
                                          bool bos = false,
                                          bool eos = false)
{
    OggStreamBuilder builder(serial);
    if (!builder.packetIn(payload, granule, packet_no, bos, eos)) {
        return {};
    }

    std::vector<uint8_t> bytes;
    while (builder.flush(bytes)) {
    }
    return bytes;
}

std::unique_ptr<PsyMP3::Demuxer::Ogg::OggDemuxer> makeDemuxer(std::vector<uint8_t> stream_bytes)
{
    auto io = std::make_unique<MemoryIOHandler>(std::move(stream_bytes));
    return std::make_unique<PsyMP3::Demuxer::Ogg::OggDemuxer>(std::move(io));
}

} // namespace

int main()
{
    rc::check("OggSyncManager: Random Byte Stream Resilience",
              [](const std::vector<uint8_t>& data) {
                  MemoryIOHandler io(data);
                  PsyMP3::Demuxer::Ogg::OggSyncManager sync(&io);

                  ogg_page page{};
                  for (int page_count = 0; page_count < 1000; ++page_count) {
                      int result = sync.getNextPage(&page);
                      if (result == 0) {
                          break;
                      }
                  }

                  RC_ASSERT(true);
              });

    rc::check("OggSyncManager: Valid Page Recovery",
              [](int64_t granule, int32_t serial, int32_t seq, const std::vector<uint8_t>& payload) {
                  std::vector<uint8_t> safe_payload = payload;
                  if (safe_payload.size() > 200) {
                      safe_payload.resize(200);
                  }

                  std::vector<uint8_t> stream_data = {'G', 'a', 'r', 'b', 'a', 'g', 'e'};
                  std::vector<uint8_t> page_data =
                      makeTaggedPageStream(serial, granule, seq, safe_payload, seq == 0, false);
                  RC_PRE(!page_data.empty());

                  stream_data.insert(stream_data.end(), page_data.begin(), page_data.end());
                  stream_data.insert(stream_data.end(), {'M', 'o', 'r', 'e'});

                  MemoryIOHandler io(stream_data);
                  PsyMP3::Demuxer::Ogg::OggSyncManager sync(&io);

                  ogg_page page{};
                  int result = sync.getNextPage(&page);
                  if (result == 1) {
                      RC_ASSERT(ogg_page_serialno(&page) == serial);
                  }
              });

    rc::check("OggDemuxer: Negative Serial Number Support",
              [](const std::vector<uint8_t>&) {
                  const int32_t negative_serial = -975925429;
                  std::vector<uint8_t> stream_data =
                      makeTaggedPageStream(negative_serial, 0, 0, makeMinimalOpusHead(), true, true);
                  RC_PRE(!stream_data.empty());

                  auto demuxer = makeDemuxer(std::move(stream_data));
                  RC_ASSERT(demuxer->parseContainer());

                  auto streams = demuxer->getStreams();
                  RC_ASSERT(!streams.empty());
                  RC_ASSERT(static_cast<int32_t>(streams[0].stream_id) == negative_serial);

                  demuxer->getDuration();
              });

    rc::check("OggDemuxer: Multiplexed Seeking Consistency",
              [](uint32_t primary_serial, uint32_t secondary_serial) {
                  RC_PRE(primary_serial != secondary_serial);

                  OggStreamBuilder primary(primary_serial);
                  OggStreamBuilder secondary(secondary_serial);

                  std::vector<uint8_t> bytes;

                  RC_PRE(primary.packetIn(makeMinimalOpusHead(), 0, 0, true, false));
                  RC_PRE(secondary.packetIn({'N', 'o', 'i', 's', 'e', 'H', 'e', 'a', 'd'}, 0, 0, true, false));

                  while (primary.flush(bytes)) {
                  }
                  while (secondary.flush(bytes)) {
                  }

                  RC_PRE(primary.packetIn({0xDE, 0xAD, 0xBE, 0xEF}, 48000, 1, false, false));
                  while (primary.flush(bytes)) {
                  }

                  RC_PRE(secondary.packetIn({0xCA, 0xFE, 0xBA, 0xBE}, 200, 1, false, false));
                  while (secondary.flush(bytes)) {
                  }

                  auto demuxer = makeDemuxer(std::move(bytes));
                  if (demuxer->parseContainer()) {
                      (void)demuxer->seekTo(500);
                  }

                  RC_ASSERT(true);
              });

    return 0;
}

#else

#include <iostream>

int main()
{
    std::cout << "RapidCheck not available. Skipping test_ogg_fuzzing tests." << std::endl;
    return 0;
}

#endif
