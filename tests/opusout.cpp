/*
 * opusout.cpp - Opus decoding command line utility
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "io/file/FileIOHandler.h"
#include "demuxer/ogg/OggDemuxer.h"
#include "codecs/opus/OpusCodec.h"

// Use namespaces for convenience
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Codec;
using namespace PsyMP3::IO::File;

// Simple WAV header writer
void writeWavHeader(std::ofstream& file, uint32_t sample_rate, uint16_t channels, uint32_t data_size) {
    uint32_t file_size = data_size + 36;
    uint32_t byte_rate = sample_rate * channels * 2;
    uint16_t block_align = channels * 2;
    
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&file_size), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    
    uint32_t fmt_chunk_size = 16;
    uint16_t audio_format = 1; // PCM
    
    file.write(reinterpret_cast<const char*>(&fmt_chunk_size), 4);
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    file.write(reinterpret_cast<const char*>(&block_align), 2);
    
    uint16_t bits_per_sample = 16;
    file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);
}

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [options] <input.opus> <output.wav>" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --help     Show this help message" << std::endl;
    std::cerr << "  --verbose  Enable verbose logging" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_path;
    std::string output_path;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--verbose") {
            verbose = true;
            Debug::init("", {"opus", "demuxer", "codec"}); 
        } else if (input_path.empty()) {
            input_path = arg;
        } else if (output_path.empty()) {
            output_path = arg;
        } else {
            std::cerr << "Error: Too many arguments." << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (input_path.empty() || output_path.empty()) {
        std::cerr << "Error: Missing input or output file." << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    std::cout << "Decoding " << input_path << " to " << output_path << "..." << std::endl;
    
    // Register only OggDemuxer and OpusCodec (minimal registration for opus decoding)
    DemuxerRegistry::getInstance().registerDemuxer("ogg", [](std::unique_ptr<PsyMP3::IO::IOHandler> handler) {
        return std::make_unique<PsyMP3::Demuxer::Ogg::OggDemuxer>(std::move(handler));
    }, "Ogg", {"ogg", "oga", "opus"});
    
    // Register OpusCodec with both cases to handle stream metadata variations
    AudioCodecFactory::registerCodec("opus", [](const StreamInfo& stream_info) {
        return std::make_unique<PsyMP3::Codec::Opus::OpusCodec>(stream_info);
    });
    AudioCodecFactory::registerCodec("Opus", [](const StreamInfo& stream_info) {
        return std::make_unique<PsyMP3::Codec::Opus::OpusCodec>(stream_info);
    });
    
    // Create IOHandler directly
    std::unique_ptr<FileIOHandler> io_handler;
    try {
        io_handler = std::make_unique<FileIOHandler>(input_path);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to open input file: " << e.what() << std::endl;
        return 1;
    }
    
    // Create Demuxer
    auto demuxer = DemuxerFactory::createDemuxer(std::move(io_handler));
    if (!demuxer) {
        std::cerr << "Error: Failed to create demuxer for file." << std::endl;
        return 1;
    }
    
    if (!demuxer->parseContainer()) {
        std::cerr << "Error: Failed to parse container." << std::endl;
        if (demuxer->hasError()) {
            std::cerr << "Details: " << demuxer->getLastError().message << std::endl;
        }
        return 1;
    }
    
    // detailed stream info
    auto streams = demuxer->getStreams();
    const StreamInfo* audio_stream = nullptr;
    
    std::cout << "Container parsed. Found " << streams.size() << " streams." << std::endl;
    for (const auto& stream : streams) {
        std::cout << "  Stream " << stream.stream_id << ": " << stream.codec_type << "/" << stream.codec_name << std::endl;
        // Match Opus codec case-insensitively (OggDemuxer may use "Opus" or "opus")
        std::string codec_lower = stream.codec_name;
        std::transform(codec_lower.begin(), codec_lower.end(), codec_lower.begin(), ::tolower);
        if (codec_lower == "opus" && !audio_stream) {
            audio_stream = &stream;
        }
    }
    
    if (!audio_stream) {
        std::cerr << "Error: No Opus audio stream found." << std::endl;
        return 1;
    }
    
    std::cout << "Selected Opus stream " << audio_stream->stream_id << std::endl;
    
    // Create Codec
    auto codec = AudioCodecFactory::createCodec(*audio_stream);
    if (!codec) {
        std::cerr << "Error: Failed to create Opus codec." << std::endl;
        return 1;
    }
    
    if (!codec->initialize()) {
        std::cerr << "Error: Failed to initialize Opus codec." << std::endl;
        return 1;
    }
    
    // Open output file
    std::ofstream out_file(output_path, std::ios::binary);
    if (!out_file) {
        std::cerr << "Error: Failed to open output file: " << output_path << std::endl;
        return 1;
    }
    
    // Placeholder for WAV header
    writeWavHeader(out_file, 48000, 2, 0); // Default Opus is 48k stereo usually, but we'll update later
    
    uint32_t total_samples = 0;
    uint32_t final_channels = 0;
    uint32_t final_rate = 48000;
    
    // Decoding loop
    int packet_count = 0;
    while (!demuxer->isEOF()) {
        MediaChunk chunk = demuxer->readChunk(audio_stream->stream_id);
        
        if (!chunk.isValid()) {
            if (demuxer->isEOF()) break;
            continue; // Skip invalid chunks
        }
        
        AudioFrame frame = codec->decode(chunk);
        
        if (!frame.samples.empty()) {
            // Write samples
            out_file.write(reinterpret_cast<const char*>(frame.samples.data()), frame.samples.size() * sizeof(int16_t));
            total_samples += frame.getSampleFrameCount();
            
            if (final_channels == 0) final_channels = frame.channels;
            if (final_rate == 0) final_rate = frame.sample_rate;
            
            // Validate consistency
            if (frame.channels != final_channels) {
                std::cerr << "Warning: Channel count changed during decoding!" << std::endl;
            }
        }
        
        packet_count++;
        if (packet_count % 100 == 0) {
            std::cout << "Decoded " << packet_count << " packets (" << total_samples << " samples)..." << "\r" << std::flush;
        }
    }
    
    // Flush codec
    AudioFrame flush_frame = codec->flush();
    if (!flush_frame.samples.empty()) {
         out_file.write(reinterpret_cast<const char*>(flush_frame.samples.data()), flush_frame.samples.size() * sizeof(int16_t));
         total_samples += flush_frame.getSampleFrameCount();
    }
    
    std::cout << "\nDecoding complete." << std::endl;
    std::cout << "Total packets: " << packet_count << std::endl;
    std::cout << "Total samples: " << total_samples << std::endl;
    std::cout << "Duration: " << (double)total_samples / final_rate << " seconds" << std::endl;
    
    // Update WAV header
    out_file.seekp(0);
    writeWavHeader(out_file, final_rate, final_channels, total_samples * final_channels * 2);
    
    return 0;
}
