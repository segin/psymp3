/*
 * flacout.cpp - FLAC to PCM/WAV decoder utility
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This utility decodes FLAC files to either:
 * - Raw PCM (LPCM) data
 * - RIFF WAVE format
 *
 * This serves as a test of the native FLAC decoder implementation.
 * Note: Output is 16-bit signed PCM as that's what the codec produces.
 * High bit-depth sources (24-bit, 32-bit) are converted to 16-bit.
 *
 * Usage:
 *   flacout [options] input.flac [output.wav|output.pcm]
 *
 * Options:
 *   -r, --raw       Output raw PCM instead of WAVE (default: WAVE)
 *   -q, --quiet     Suppress progress output
 *   -v, --verbose   Show detailed decoding information
 *   -h, --help      Show this help message
 *
 * If no output file is specified, output goes to stdout.
 */

#include "psymp3.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <vector>
#include <memory>
#include <chrono>
#include <iomanip>

using namespace PsyMP3::Demuxer;

// RIFF WAVE header structure
#pragma pack(push, 1)
struct WAVHeader {
    // RIFF chunk
    char riff_id[4];           // "RIFF"
    uint32_t riff_size;        // File size - 8
    char wave_id[4];           // "WAVE"
    
    // fmt chunk
    char fmt_id[4];            // "fmt "
    uint32_t fmt_size;         // 16 for PCM
    uint16_t audio_format;     // 1 for PCM
    uint16_t num_channels;     // Number of channels
    uint32_t sample_rate;      // Sample rate in Hz
    uint32_t byte_rate;        // sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align;      // num_channels * bits_per_sample/8
    uint16_t bits_per_sample;  // Bits per sample
    
    // data chunk
    char data_id[4];           // "data"
    uint32_t data_size;        // Size of audio data
};
#pragma pack(pop)

/**
 * @brief Configuration options for flacout
 */
struct FlacOutConfig {
    std::string input_file;
    std::string output_file;
    bool raw_output = false;      // Output raw PCM instead of WAVE
    bool quiet = false;           // Suppress progress output
    bool verbose = false;         // Show detailed info
    bool use_stdout = false;      // Output to stdout
};

/**
 * @brief Print usage information
 */
static void printUsage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [options] input.flac [output.wav|output.pcm]\n"
              << "\n"
              << "Decode FLAC files to PCM or WAVE format.\n"
              << "\n"
              << "Options:\n"
              << "  -r, --raw       Output raw PCM instead of WAVE (default: WAVE)\n"
              << "  -q, --quiet     Suppress progress output\n"
              << "  -v, --verbose   Show detailed decoding information\n"
              << "  -h, --help      Show this help message\n"
              << "\n"
              << "Output format:\n"
              << "  16-bit signed little-endian PCM (S16_LE)\n"
              << "  High bit-depth sources are converted to 16-bit.\n"
              << "\n"
              << "If no output file is specified, output goes to stdout.\n"
              << "When outputting to stdout, raw PCM is used by default.\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " input.flac output.wav\n"
              << "  " << program_name << " -r input.flac output.pcm\n"
              << "  " << program_name << " input.flac > output.pcm\n"
              << "  " << program_name << " input.flac | aplay -f S16_LE -r 44100 -c 2\n";
}

/**
 * @brief Parse command line arguments
 */
static bool parseArgs(int argc, char* argv[], FlacOutConfig& config) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        } else if (arg == "-r" || arg == "--raw") {
            config.raw_output = true;
        } else if (arg == "-q" || arg == "--quiet") {
            config.quiet = true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return false;
        } else if (config.input_file.empty()) {
            config.input_file = arg;
        } else if (config.output_file.empty()) {
            config.output_file = arg;
        } else {
            std::cerr << "Too many arguments\n";
            printUsage(argv[0]);
            return false;
        }
    }
    
    if (config.input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        printUsage(argv[0]);
        return false;
    }
    
    // If no output file, use stdout with raw PCM
    if (config.output_file.empty()) {
        config.use_stdout = true;
        config.raw_output = true;  // Default to raw for stdout
        config.quiet = true;       // Suppress progress for stdout
    }
    
    return true;
}

/**
 * @brief Write WAVE header to output stream
 */
static bool writeWAVHeader(std::ostream& out, uint32_t sample_rate, 
                           uint16_t channels, uint16_t bits_per_sample,
                           uint32_t data_size) {
    WAVHeader header;
    
    // RIFF chunk
    std::memcpy(header.riff_id, "RIFF", 4);
    header.riff_size = 36 + data_size;  // File size - 8
    std::memcpy(header.wave_id, "WAVE", 4);
    
    // fmt chunk
    std::memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_size = 16;  // PCM format
    header.audio_format = 1;  // PCM
    header.num_channels = channels;
    header.sample_rate = sample_rate;
    header.bits_per_sample = bits_per_sample;
    header.block_align = channels * (bits_per_sample / 8);
    header.byte_rate = sample_rate * header.block_align;
    
    // data chunk
    std::memcpy(header.data_id, "data", 4);
    header.data_size = data_size;
    
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    return out.good();
}

/**
 * @brief Update WAVE header with final data size
 */
static bool updateWAVHeader(std::ostream& out, uint32_t data_size) {
    // Seek to riff_size position (offset 4)
    out.seekp(4, std::ios::beg);
    uint32_t riff_size = 36 + data_size;
    out.write(reinterpret_cast<const char*>(&riff_size), sizeof(riff_size));
    
    // Seek to data_size position (offset 40)
    out.seekp(40, std::ios::beg);
    out.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
    
    return out.good();
}

/**
 * @brief Format time in MM:SS.mmm format
 */
static std::string formatTime(uint64_t ms) {
    uint64_t seconds = ms / 1000;
    uint64_t minutes = seconds / 60;
    seconds %= 60;
    uint64_t millis = ms % 1000;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << "." << std::setw(3) << millis;
    return oss.str();
}

/**
 * @brief Main decoding function using DemuxedStream
 */
static int decode(const FlacOutConfig& config) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Open input file using DemuxedStream
    if (!config.quiet) {
        std::cerr << "Opening: " << config.input_file << "\n";
    }
    
    TagLib::String path(config.input_file.c_str(), TagLib::String::UTF8);
    
    std::unique_ptr<DemuxedStream> stream;
    try {
        stream = std::make_unique<DemuxedStream>(path);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to open file: " << e.what() << "\n";
        std::cerr << "Check flacout_debug.log for detailed error information\n";
        return 1;
    }
    
    if (!stream) {
        std::cerr << "Error: Failed to create stream\n";
        std::cerr << "Check flacout_debug.log for detailed error information\n";
        return 1;
    }
    
    // Get stream info
    StreamInfo info = stream->getCurrentStreamInfo();
    uint64_t duration_ms = stream->getLength();
    
    if (!config.quiet) {
        std::cerr << "Stream info:\n"
                  << "  Demuxer: " << stream->getDemuxerType() << "\n"
                  << "  Codec: " << stream->getCodecType() << "\n"
                  << "  Sample rate: " << info.sample_rate << " Hz\n"
                  << "  Channels: " << info.channels << "\n"
                  << "  Source bits per sample: " << info.bits_per_sample << "\n"
                  << "  Output bits per sample: 16 (S16_LE)\n"
                  << "  Duration: " << formatTime(duration_ms) << "\n";
        
        if (!info.title.empty()) {
            std::cerr << "  Title: " << info.title << "\n";
        }
        if (!info.artist.empty()) {
            std::cerr << "  Artist: " << info.artist << "\n";
        }
    }
    
    // Open output
    std::ostream* out = nullptr;
    std::ofstream file_out;
    
    if (config.use_stdout) {
        out = &std::cout;
    } else {
        file_out.open(config.output_file, std::ios::binary);
        if (!file_out) {
            std::cerr << "Error: Failed to open output file: " << config.output_file << "\n";
            return 1;
        }
        out = &file_out;
    }
    
    // Output is always 16-bit PCM (codec converts to 16-bit)
    const uint16_t output_bits = 16;
    
    // Write WAVE header placeholder if not raw
    if (!config.raw_output) {
        if (!writeWAVHeader(*out, info.sample_rate, info.channels, output_bits, 0)) {
            std::cerr << "Error: Failed to write WAVE header\n";
            return 1;
        }
    }
    
    // Decode loop - read PCM data from stream
    uint64_t total_bytes = 0;
    uint64_t total_samples = 0;
    
    // Buffer for reading decoded PCM data
    const size_t buffer_size = 16384;  // 16KB buffer
    std::vector<uint8_t> buffer(buffer_size);
    
    if (!config.quiet) {
        std::cerr << "Decoding...\n";
    }
    
    uint64_t last_progress_update = 0;
    
    while (!stream->eof()) {
        size_t bytes_read = stream->getData(buffer_size, buffer.data());
        
        if (bytes_read == 0) {
            break;
        }
        
        // Write decoded samples
        out->write(reinterpret_cast<const char*>(buffer.data()), bytes_read);
        
        if (!out->good()) {
            std::cerr << "Error: Write failed\n";
            return 1;
        }
        
        total_bytes += bytes_read;
        // Each sample is 16-bit (2 bytes) per channel
        total_samples += bytes_read / (2 * info.channels);
        
        // Progress output (every ~500ms worth of audio)
        if (!config.quiet) {
            uint64_t pos_ms = stream->getPosition();
            if (pos_ms - last_progress_update >= 500 || pos_ms < last_progress_update) {
                double progress = (duration_ms > 0) ? 
                    (static_cast<double>(pos_ms) / duration_ms * 100.0) : 0.0;
                std::cerr << "\r  " << formatTime(pos_ms) << " / " << formatTime(duration_ms)
                          << " (" << std::fixed << std::setprecision(1) << progress << "%)"
                          << std::flush;
                last_progress_update = pos_ms;
            }
        }
    }
    
    // Update WAVE header with final size
    if (!config.raw_output && !config.use_stdout) {
        updateWAVHeader(*out, static_cast<uint32_t>(total_bytes));
    }
    
    // Close output file
    if (file_out.is_open()) {
        file_out.close();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (!config.quiet) {
        std::cerr << "\n\nDecoding complete:\n"
                  << "  Total samples: " << total_samples << "\n"
                  << "  Output size: " << total_bytes << " bytes\n"
                  << "  Time elapsed: " << elapsed.count() << " ms\n";
        
        if (duration_ms > 0 && elapsed.count() > 0) {
            double speed = static_cast<double>(duration_ms) / elapsed.count();
            std::cerr << "  Decode speed: " << std::fixed << std::setprecision(1) 
                      << speed << "x realtime\n";
        }
        
        if (!config.output_file.empty()) {
            std::cerr << "  Output file: " << config.output_file << "\n";
        }
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    // Initialize debug system with all channels enabled
    std::vector<std::string> debug_channels = {"all"};
    Debug::init("flacout_debug.log", debug_channels);
    
    // Register all codecs and demuxers
    registerAllCodecs();
    registerAllDemuxers();
    
    FlacOutConfig config;
    
    if (!parseArgs(argc, argv, config)) {
        return 1;
    }
    
    int result = decode(config);
    
    Debug::shutdown();
    return result;
}
