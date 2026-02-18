/*
 * test_issue_ogg_duration.cpp - Reproduction test for Ogg duration calculation issue
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "demuxer/ogg/OggDemuxer.h"
#include "io/MemoryIOHandler.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <ogg/ogg.h>

// Helper to create Ogg pages manually since we need valid checksums
class OggBuilder {
public:
    static std::vector<uint8_t> createOggStream() {
        std::vector<uint8_t> stream_data;
        ogg_stream_state os;
        ogg_page og;
        ogg_packet op;

        int serial = 12345;
        if (ogg_stream_init(&os, serial) != 0) {
            throw std::runtime_error("ogg_stream_init failed");
        }

        // 1. BOS Page (Header) - Opus Identification Header
        // "OpusHead" (8) + Ver(1) + Chan(1) + PreSkip(2) + Rate(4) + Gain(2) + Map(1)
        unsigned char header_data[19];
        memcpy(header_data, "OpusHead", 8);
        header_data[8] = 1; // Version
        header_data[9] = 2; // Channels
        header_data[10] = 0; header_data[11] = 0; // Pre-skip (0)
        // Sample rate 48000 (0xBB80)
        header_data[12] = 0x80; header_data[13] = 0xBB; header_data[14] = 0; header_data[15] = 0;
        header_data[16] = 0; header_data[17] = 0; // Gain
        header_data[18] = 0; // Mapping family

        op.packet = header_data;
        op.bytes = sizeof(header_data);
        op.b_o_s = 1;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 0;

        ogg_stream_packetin(&os, &op);

        while(ogg_stream_flush(&os, &og)) {
            stream_data.insert(stream_data.end(), og.header, og.header + og.header_len);
            stream_data.insert(stream_data.end(), og.body, og.body + og.body_len);
        }

        // 2. Data Page (Middle)
        unsigned char body_data[] = "SomeAudioData";
        op.packet = body_data;
        op.bytes = sizeof(body_data);
        op.b_o_s = 0;
        op.e_o_s = 0;
        op.granulepos = 24000; // 0.5 seconds (at 48kHz)
        op.packetno = 1;

        ogg_stream_packetin(&os, &op);

        while(ogg_stream_pageout(&os, &og)) {
            stream_data.insert(stream_data.end(), og.header, og.header + og.header_len);
            stream_data.insert(stream_data.end(), og.body, og.body + og.body_len);
        }

        // 3. EOS Page (End)
        unsigned char end_data[] = "EndAudioData";
        op.packet = end_data;
        op.bytes = sizeof(end_data);
        op.b_o_s = 0;
        op.e_o_s = 1; // EOS
        op.granulepos = 480000; // 10 seconds
        op.packetno = 2;

        ogg_stream_packetin(&os, &op);

        while(ogg_stream_flush(&os, &og)) {
            stream_data.insert(stream_data.end(), og.header, og.header + og.header_len);
            stream_data.insert(stream_data.end(), og.body, og.body + og.body_len);
        }

        ogg_stream_clear(&os);
        return stream_data;
    }
};

class CountingIOHandler : public PsyMP3::IO::MemoryIOHandler {
public:
    CountingIOHandler(const std::vector<uint8_t>& data)
        : PsyMP3::IO::MemoryIOHandler(data.data(), data.size(), true) {}

    size_t read_count = 0;
    size_t seek_count = 0;

    size_t read(void* buffer, size_t size, size_t count) override {
        size_t res = PsyMP3::IO::MemoryIOHandler::read(buffer, size, count);
        if (res > 0) read_count++;
        return res;
    }

    int seek(off_t offset, int whence) override {
        seek_count++;
        return PsyMP3::IO::MemoryIOHandler::seek(offset, whence);
    }

    void resetCounts() {
        read_count = 0;
        seek_count = 0;
    }
};

int main() {
    std::vector<std::string> channels = {"ogg", "demuxer"};
    Debug::init("", channels);

    std::cout << "Creating Ogg Opus stream..." << std::endl;
    auto ogg_data = OggBuilder::createOggStream();
    std::cout << "Stream size: " << ogg_data.size() << " bytes" << std::endl;

    auto handler = std::make_unique<CountingIOHandler>(ogg_data);
    auto* handler_ptr = handler.get();

    std::cout << "Initializing OggDemuxer..." << std::endl;
    PsyMP3::Demuxer::Ogg::OggDemuxer demuxer(std::move(handler));

    std::cout << "Calling parseContainer()..." << std::endl;
    handler_ptr->resetCounts();
    bool parsed = demuxer.parseContainer();

    std::cout << "Parsed: " << (parsed ? "Yes" : "No") << std::endl;
    std::cout << "parseContainer I/O stats: " << handler_ptr->read_count << " reads, " << handler_ptr->seek_count << " seeks" << std::endl;

    auto streams = demuxer.getStreams();
    std::cout << "Detected streams: " << streams.size() << std::endl;
    if (streams.size() > 0) {
        std::cout << "Stream 0: " << streams[0].codec_name << std::endl;
    }

    std::cout << "Calling getDuration()..." << std::endl;
    handler_ptr->resetCounts();
    uint64_t duration = demuxer.getDuration();

    std::cout << "Duration: " << duration << " ms" << std::endl;
    std::cout << "getDuration I/O stats: " << handler_ptr->read_count << " reads, " << handler_ptr->seek_count << " seeks" << std::endl;

    if (duration == 0) {
        std::cerr << "ERROR: Duration is 0! Ogg file creation might be invalid or calculation failed." << std::endl;
        return 1;
    }

    if (handler_ptr->read_count > 0 || handler_ptr->seek_count > 0) {
        std::cout << "BASELINE CONFIRMED: getDuration() performs I/O." << std::endl;
        return 0; // Success for baseline test
    } else {
        std::cout << "OPTIMIZED BEHAVIOR: getDuration() performed NO I/O." << std::endl;
        return 0;
    }
}
