/*
 * ISODemuxer.cpp - ISO Base Media File Format demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

ISODemuxer::ISODemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
}

bool ISODemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        // Get file size
        m_handler->seek(0, SEEK_END);
        m_file_size = m_handler->tell();
        m_handler->seek(0, SEEK_SET);
        
        // Parse top-level boxes
        while (m_handler->tell() < static_cast<long>(m_file_size)) {
            ISOBox box;
            if (!readBoxHeader(box)) {
                break;
            }
            
            switch (box.type) {
                case FTYP_BOX:
                    if (!parseFtypBox(box)) {
                        return false;
                    }
                    break;
                    
                case MOOV_BOX:
                    if (!parseMoovBox(box)) {
                        return false;
                    }
                    break;
                    
                case MDAT_BOX:
                    // Skip media data for now - we'll read it during playback
                    skipBox(box);
                    break;
                    
                default:
                    // Skip unknown boxes
                    skipBox(box);
                    break;
            }
        }
        
        // Calculate duration from tracks
        for (const auto& [track_id, track] : m_tracks) {
            if (track.handler_type == "soun") { // Audio track
                uint64_t track_duration_ms = track.getDurationMs();
                m_duration_ms = std::max(m_duration_ms, track_duration_ms);
            }
        }
        
        m_parsed = true;
        return !m_tracks.empty();
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<StreamInfo> ISODemuxer::getStreams() const {
    std::vector<StreamInfo> streams;
    
    for (const auto& [track_id, track] : m_tracks) {
        if (track.handler_type != "soun") {
            continue; // Skip non-audio tracks for now
        }
        
        StreamInfo info;
        info.stream_id = track_id;
        info.codec_type = "audio";
        info.codec_name = track.codec_name;
        info.duration_ms = track.getDurationMs();
        info.bitrate = track.bitrate;
        
        // Get audio properties from sample description
        if (!track.sample_descriptions.empty()) {
            const auto& desc = track.sample_descriptions[0];
            info.channels = desc.channels;
            info.sample_rate = desc.sample_rate >> 16; // Fixed-point 16.16
            info.bits_per_sample = desc.sample_size;
            info.codec_data = desc.specific_data;
        }
        
        // Calculate duration in samples
        if (info.sample_rate > 0) {
            info.duration_samples = (info.duration_ms * info.sample_rate) / 1000ULL;
        }
        
        streams.push_back(info);
    }
    
    return streams;
}

StreamInfo ISODemuxer::getStreamInfo(uint32_t stream_id) const {
    auto streams = getStreams();
    auto it = std::find_if(streams.begin(), streams.end(),
                          [stream_id](const StreamInfo& info) {
                              return info.stream_id == stream_id;
                          });
    
    if (it != streams.end()) {
        return *it;
    }
    
    return StreamInfo{};
}

MediaChunk ISODemuxer::readChunk() {
    uint32_t best_track = findBestAudioTrack();
    if (best_track != 0) {
        return readChunk(best_track);
    }
    
    return MediaChunk{};
}

MediaChunk ISODemuxer::readChunk(uint32_t stream_id) {
    auto it = m_tracks.find(stream_id);
    if (it == m_tracks.end()) {
        return MediaChunk{};
    }
    
    ISOTrack& track = it->second;
    
    // Check if we've reached the end of samples
    if (track.current_sample >= track.sample_sizes.size()) {
        m_eof = true;
        return MediaChunk{};
    }
    
    MediaChunk chunk;
    if (!getSampleData(stream_id, track.current_sample, chunk)) {
        m_eof = true;
        return MediaChunk{};
    }
    
    // Update position
    if (track.current_sample < track.sample_times.size()) {
        uint64_t sample_time = track.sample_times[track.current_sample];
        uint64_t timestamp_ms = track.trackTimeToMs(sample_time);
        chunk.timestamp_samples = (timestamp_ms * track.timescale) / 1000;
        m_position_ms = timestamp_ms;
    }
    
    track.current_sample++;
    
    return chunk;
}

bool ISODemuxer::seekTo(uint64_t timestamp_ms) {
    uint32_t target_track = findBestAudioTrack();
    if (target_track == 0) {
        return false;
    }
    
    auto it = m_tracks.find(target_track);
    if (it == m_tracks.end()) {
        return false;
    }
    
    ISOTrack& track = it->second;
    uint64_t target_time = track.msToTrackTime(timestamp_ms);
    
    bool found = seekToSample(target_track, target_time);
    
    if (found) {
        m_position_ms = timestamp_ms;
        m_eof = false;
    }
    
    return found;
}

bool ISODemuxer::isEOF() const {
    return m_eof;
}

uint64_t ISODemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t ISODemuxer::getPosition() const {
    return m_position_ms;
}

bool ISODemuxer::readBoxHeader(ISOBox& box) {
    long current_pos = m_handler->tell();
    if (current_pos + 8 > static_cast<long>(m_file_size)) {
        return false;
    }
    
    box.size = readBE<uint32_t>();
    box.type = readBE<uint32_t>();
    box.data_offset = m_handler->tell();
    
    if (box.size == 1) {
        // Extended size
        if (current_pos + 16 > static_cast<long>(m_file_size)) {
            return false;
        }
        box.extended_size = readBE<uint64_t>();
        box.data_offset = m_handler->tell();
    } else if (box.size == 0) {
        // Box extends to end of file
        box.extended_size = m_file_size - current_pos;
    }
    
    return true;
}

void ISODemuxer::skipBox(const ISOBox& box) {
    uint64_t content_size = box.getContentSize();
    m_handler->seek(box.data_offset + content_size, SEEK_SET);
}

bool ISODemuxer::parseFtypBox(const ISOBox& box) {
    // Read major brand
    uint32_t major_brand = readBE<uint32_t>();
    uint32_t minor_version = readBE<uint32_t>();
    (void)major_brand; (void)minor_version;
    
    // Skip compatible brands for now
    skipBox(box);
    
    // Verify this is a supported format
    // Common brands: "mp41", "mp42", "M4A ", "isom", "qt  "
    return true; // Accept any for now
}

bool ISODemuxer::parseMoovBox(const ISOBox& box) {
    uint64_t box_end = box.data_offset + box.getContentSize();
    
    while (m_handler->tell() < static_cast<long>(box_end)) {
        ISOBox child_box;
        if (!readBoxHeader(child_box)) {
            break;
        }
        
        if (child_box.type == TRAK_BOX) {
            ISOTrack track;
            if (parseTrakBox(child_box, track)) {
                m_tracks[track.track_id] = track;
            }
        } else {
            skipBox(child_box);
        }
    }
    
    return true;
}

bool ISODemuxer::parseTrakBox(const ISOBox& box, ISOTrack& track) {
    uint64_t box_end = box.data_offset + box.getContentSize();
    
    while (m_handler->tell() < static_cast<long>(box_end)) {
        ISOBox child_box;
        if (!readBoxHeader(child_box)) {
            break;
        }
        
        switch (child_box.type) {
            case TKHD_BOX:
                parseTkhdBox(child_box, track);
                break;
            case MDIA_BOX:
                parseMdiaBox(child_box, track);
                break;
            default:
                skipBox(child_box);
                break;
        }
    }
    
    return track.track_id != 0;
}

bool ISODemuxer::parseTkhdBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8; // Skip version, read 24-bit flags
    (void)flags;
    
    if (version == 1) {
        // 64-bit version
        uint64_t creation_time = readBE<uint64_t>();
        uint64_t modification_time = readBE<uint64_t>();
        track.track_id = readBE<uint32_t>();
        uint32_t reserved = readBE<uint32_t>();
        track.duration = readBE<uint64_t>();
        (void)creation_time; (void)modification_time; (void)reserved;
    } else {
        // 32-bit version
        uint32_t creation_time = readBE<uint32_t>();
        uint32_t modification_time = readBE<uint32_t>();
        track.track_id = readBE<uint32_t>();
        uint32_t reserved = readBE<uint32_t>();
        track.duration = readBE<uint32_t>();
        (void)creation_time; (void)modification_time; (void)reserved;
    }
    
    skipBox(box);
    return true;
}

bool ISODemuxer::parseMdiaBox(const ISOBox& box, ISOTrack& track) {
    uint64_t box_end = box.data_offset + box.getContentSize();
    
    while (m_handler->tell() < static_cast<long>(box_end)) {
        ISOBox child_box;
        if (!readBoxHeader(child_box)) {
            break;
        }
        
        switch (child_box.type) {
            case MDHD_BOX:
                parseMdhdBox(child_box, track);
                break;
            case HDLR_BOX:
                parseHdlrBox(child_box, track);
                break;
            case MINF_BOX:
                // Parse media information
                {
                    uint64_t minf_end = child_box.data_offset + child_box.getContentSize();
                    while (m_handler->tell() < static_cast<long>(minf_end)) {
                        ISOBox minf_child;
                        if (!readBoxHeader(minf_child)) {
                            break;
                        }
                        
                        if (minf_child.type == STBL_BOX) {
                            parseStblBox(minf_child, track);
                        } else {
                            skipBox(minf_child);
                        }
                    }
                }
                break;
            default:
                skipBox(child_box);
                break;
        }
    }
    
    return true;
}

bool ISODemuxer::parseMdhdBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8;
    (void)flags;
    
    if (version == 1) {
        uint64_t creation_time = readBE<uint64_t>();
        uint64_t modification_time = readBE<uint64_t>();
        track.timescale = readBE<uint32_t>();
        track.duration = readBE<uint64_t>();
        (void)creation_time; (void)modification_time;
    } else {
        uint32_t creation_time = readBE<uint32_t>();
        uint32_t modification_time = readBE<uint32_t>();
        track.timescale = readBE<uint32_t>();
        track.duration = readBE<uint32_t>();
        (void)creation_time; (void)modification_time;
    }
    
    skipBox(box);
    return true;
}

bool ISODemuxer::parseHdlrBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8;
    uint32_t pre_defined = readBE<uint32_t>();
    (void)version; (void)flags; (void)pre_defined;
    
    uint32_t handler_type = readBE<uint32_t>();
    track.handler_type = fourCCToString(handler_type);
    
    skipBox(box);
    return true;
}

bool ISODemuxer::parseStblBox(const ISOBox& box, ISOTrack& track) {
    uint64_t box_end = box.data_offset + box.getContentSize();
    
    while (m_handler->tell() < static_cast<long>(box_end)) {
        ISOBox child_box;
        if (!readBoxHeader(child_box)) {
            break;
        }
        
        switch (child_box.type) {
            case STSD_BOX:
                parseStsdBox(child_box, track);
                break;
            case STTS_BOX:
                parseSttsBox(child_box, track);
                break;
            case STSC_BOX:
                parseStscBox(child_box, track);
                break;
            case STSZ_BOX:
                parseStszBox(child_box, track);
                break;
            case STCO_BOX:
            case CO64_BOX:
                parseStcoBox(child_box, track);
                break;
            default:
                skipBox(child_box);
                break;
        }
    }
    
    return true;
}

bool ISODemuxer::parseStsdBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8;
    uint32_t entry_count = readBE<uint32_t>();
    (void)version; (void)flags;
    
    for (uint32_t i = 0; i < entry_count; ++i) {
        SampleDescription desc;
        
        uint32_t sample_desc_size = readBE<uint32_t>();
        desc.format = readBE<uint32_t>();
        uint8_t reserved[6];
        m_handler->read(reserved, 1, 6);
        desc.data_ref_index = readBE<uint16_t>();
        
        if (track.handler_type == "soun") {
            // Audio sample description
            uint16_t version = readBE<uint16_t>();
            uint16_t revision_level = readBE<uint16_t>();
            uint32_t vendor = readBE<uint32_t>();
            desc.channels = readBE<uint16_t>();
            desc.sample_size = readBE<uint16_t>();
            uint16_t compression_id = readBE<uint16_t>();
            uint16_t packet_size = readBE<uint16_t>();
            desc.sample_rate = readBE<uint32_t>(); // 16.16 fixed point
            (void)version; (void)revision_level; (void)vendor; (void)compression_id; (void)packet_size;
            
            // Read any additional data
            size_t bytes_read = 8 + 20; // Box header + audio description
            if (sample_desc_size > bytes_read) {
                size_t extra_size = sample_desc_size - bytes_read;
                desc.specific_data.resize(extra_size);
                m_handler->read(desc.specific_data.data(), 1, extra_size);
            }
        } else {
            // Skip non-audio descriptions for now
            size_t skip_size = sample_desc_size - 8; // Already read 8 bytes
            m_handler->seek(skip_size, SEEK_CUR);
        }
        
        track.sample_descriptions.push_back(desc);
        track.codec_name = formatToCodecName(desc.format);
    }
    
    return true;
}

bool ISODemuxer::parseSttsBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8;
    uint32_t entry_count = readBE<uint32_t>();
    (void)version; (void)flags;
    
    uint64_t current_time = 0;
    
    for (uint32_t i = 0; i < entry_count; ++i) {
        uint32_t sample_count = readBE<uint32_t>();
        uint32_t sample_delta = readBE<uint32_t>();
        
        for (uint32_t j = 0; j < sample_count; ++j) {
            track.sample_times.push_back(current_time);
            current_time += sample_delta;
        }
    }
    
    return true;
}

bool ISODemuxer::parseStscBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8;
    uint32_t entry_count = readBE<uint32_t>();
    (void)version; (void)flags;
    
    for (uint32_t i = 0; i < entry_count; ++i) {
        uint32_t first_chunk = readBE<uint32_t>();
        uint32_t samples_per_chunk = readBE<uint32_t>();
        uint32_t sample_description_index = readBE<uint32_t>();
        (void)first_chunk; (void)sample_description_index;
        
        // Store samples per chunk (will need to expand this later)
        track.samples_per_chunk.push_back(samples_per_chunk);
    }
    
    return true;
}

bool ISODemuxer::parseStszBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8;
    uint32_t sample_size = readBE<uint32_t>();
    uint32_t sample_count = readBE<uint32_t>();
    (void)version; (void)flags;
    
    if (sample_size != 0) {
        // All samples have the same size
        track.sample_sizes.resize(sample_count, sample_size);
    } else {
        // Read individual sample sizes
        track.sample_sizes.resize(sample_count);
        for (uint32_t i = 0; i < sample_count; ++i) {
            track.sample_sizes[i] = readBE<uint32_t>();
        }
    }
    
    return true;
}

bool ISODemuxer::parseStcoBox(const ISOBox& box, ISOTrack& track) {
    uint8_t version = readLE<uint8_t>();
    uint32_t flags = readBE<uint32_t>() >> 8;
    uint32_t entry_count = readBE<uint32_t>();
    (void)version; (void)flags;
    
    track.chunk_offsets.resize(entry_count);
    
    if (box.type == CO64_BOX) {
        // 64-bit chunk offsets
        for (uint32_t i = 0; i < entry_count; ++i) {
            track.chunk_offsets[i] = readBE<uint64_t>();
        }
    } else {
        // 32-bit chunk offsets
        for (uint32_t i = 0; i < entry_count; ++i) {
            track.chunk_offsets[i] = readBE<uint32_t>();
        }
    }
    
    return true;
}

std::string ISODemuxer::formatToCodecName(uint32_t format) const {
    switch (format) {
        case 0x6D703461: // "mp4a"
            return "aac";
        case 0x616C6163: // "alac"
            return "alac";
        case 0x6D703473: // "mp4s"
            return "mp3";
        case 0x61632D33: // "ac-3"
            return "ac3";
        default:
            return "unknown";
    }
}

uint32_t ISODemuxer::findBestAudioTrack() const {
    for (const auto& [track_id, track] : m_tracks) {
        if (track.handler_type == "soun") {
            return track_id;
        }
    }
    return 0;
}

bool ISODemuxer::getSampleData(uint32_t track_id, uint32_t sample_index, MediaChunk& chunk) {
    auto it = m_tracks.find(track_id);
    if (it == m_tracks.end()) {
        return false;
    }
    
    const ISOTrack& track = it->second;
    
    if (sample_index >= track.sample_sizes.size()) {
        return false;
    }
    
    // Calculate which chunk this sample is in
    uint32_t chunk_index, sample_in_chunk;
    calculateSamplePosition(const_cast<ISOTrack&>(track), sample_index, chunk_index, sample_in_chunk);
    
    if (chunk_index >= track.chunk_offsets.size()) {
        return false;
    }
    
    // Calculate file offset for this sample
    uint64_t chunk_offset = track.chunk_offsets[chunk_index];
    uint64_t sample_offset = chunk_offset;
    
    // Add offsets of previous samples in this chunk
    for (uint32_t i = 0; i < sample_in_chunk; ++i) {
        uint32_t prev_sample_index = sample_index - sample_in_chunk + i;
        if (prev_sample_index < track.sample_sizes.size()) {
            sample_offset += track.sample_sizes[prev_sample_index];
        }
    }
    
    // Read sample data
    uint32_t sample_size = track.sample_sizes[sample_index];
    chunk.stream_id = track_id;
    chunk.data.resize(sample_size);
    chunk.file_offset = sample_offset;
    
    m_handler->seek(sample_offset, SEEK_SET);
    size_t bytes_read = m_handler->read(chunk.data.data(), 1, sample_size);
    
    if (bytes_read != sample_size) {
        chunk.data.resize(bytes_read);
    }
    
    return bytes_read > 0;
}

void ISODemuxer::calculateSamplePosition(ISOTrack& track, uint32_t sample_index, 
                                        uint32_t& chunk_index, uint32_t& sample_in_chunk) {
    // This is a simplified calculation - real implementation would need
    // to properly interpret the sample-to-chunk table
    
    if (!track.samples_per_chunk.empty()) {
        uint32_t samples_per_chunk = track.samples_per_chunk[0];
        chunk_index = sample_index / samples_per_chunk;
        sample_in_chunk = sample_index % samples_per_chunk;
    } else {
        chunk_index = sample_index;
        sample_in_chunk = 0;
    }
}

bool ISODemuxer::seekToSample(uint32_t track_id, uint64_t target_time) {
    auto it = m_tracks.find(track_id);
    if (it == m_tracks.end()) {
        return false;
    }
    
    ISOTrack& track = it->second;
    
    // Find the sample with time >= target_time
    for (size_t i = 0; i < track.sample_times.size(); ++i) {
        if (track.sample_times[i] >= target_time) {
            track.current_sample = static_cast<uint32_t>(i);
            return true;
        }
    }
    
    // Seek to end
    track.current_sample = static_cast<uint32_t>(track.sample_times.size());
    return false;
}

std::string ISODemuxer::fourCCToString(uint32_t fourcc) {
    char str[5];
    str[0] = (fourcc >> 24) & 0xFF;
    str[1] = (fourcc >> 16) & 0xFF;
    str[2] = (fourcc >> 8) & 0xFF;
    str[3] = fourcc & 0xFF;
    str[4] = '\0';
    return std::string(str);
}