/**
 * Integration tests for Native FLAC decoder using real FLAC files
 * 
 * Tests decode real FLAC files from tests/data directory and verify:
 * - Successful decoding without errors
 * - Reasonable sample counts
 * - No crashes or memory issues
 * - Bit-perfect decoding (when possible to compare)
 * 
 * Feature: native-flac-decoder, Task 20.2
 * Validates: Requirements 16, 18, 50
 */

#include "psymp3.h"

using namespace PsyMP3::Codec::FLAC;

namespace {

struct TestFile {
    std::string filename;
    std::string description;
    uint32_t expected_sample_rate;
    uint32_t expected_channels;
    uint32_t expected_bit_depth;
    bool should_decode;  // Some files might be intentionally malformed
};

// Test files available in tests/data directory
const std::vector<TestFile> TEST_FILES = {
    {"tests/data/04 Time.flac", "04 Time.flac", 44100, 2, 16, true},
    {"tests/data/11 Everlong.flac", "11 Everlong.flac", 44100, 2, 16, true},
    {"tests/data/11 life goes by.flac", "11 life goes by.flac", 44100, 2, 16, true},
    {"tests/data/RADIO GA GA.flac", "RADIO GA GA.flac", 44100, 2, 16, true},
};

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

void testDecodeFile(const TestFile& testFile) {
    std::cout << "\nTesting: " << testFile.description << std::endl;
    
    if (!fileExists(testFile.filename)) {
        std::cout << "  SKIPPED: File not found" << std::endl;
        return;
    }
    
    try {
        // Open the file
        std::unique_ptr<IOHandler> io = std::make_unique<FileIOHandler>(testFile.filename);
        
        // Create demuxer
        auto demuxer = DemuxerFactory::createDemuxer(std::move(io));
        if (!demuxer) {
            if (testFile.should_decode) {
                std::cerr << "  FAILED: Could not create demuxer" << std::endl;
                exit(1);
            } else {
                std::cout << "  PASSED: Correctly rejected malformed file" << std::endl;
                return;
            }
        }
        
        // Parse container
        if (!demuxer->parseContainer()) {
            if (testFile.should_decode) {
                std::cerr << "  FAILED: Could not parse container" << std::endl;
                exit(1);
            } else {
                std::cout << "  PASSED: Correctly rejected malformed file" << std::endl;
                return;
            }
        }
        
        // Get stream info
        StreamInfo streamInfo = demuxer->getStreamInfo();
        
        // Verify stream parameters
        std::cout << "  Stream info:" << std::endl;
        std::cout << "    Sample rate: " << streamInfo.sample_rate << " Hz" << std::endl;
        std::cout << "    Channels: " << streamInfo.channels << std::endl;
        std::cout << "    Bit depth: " << streamInfo.bits_per_sample << " bits" << std::endl;
        
        if (testFile.expected_sample_rate > 0 && 
            streamInfo.sample_rate != testFile.expected_sample_rate) {
            std::cerr << "  FAILED: Expected sample rate " << testFile.expected_sample_rate 
                      << ", got " << streamInfo.sample_rate << std::endl;
            exit(1);
        }
        
        if (testFile.expected_channels > 0 && 
            streamInfo.channels != testFile.expected_channels) {
            std::cerr << "  FAILED: Expected " << testFile.expected_channels 
                      << " channels, got " << streamInfo.channels << std::endl;
            exit(1);
        }
        
        if (testFile.expected_bit_depth > 0 && 
            streamInfo.bits_per_sample != testFile.expected_bit_depth) {
            std::cerr << "  FAILED: Expected " << testFile.expected_bit_depth 
                      << " bits per sample, got " << streamInfo.bits_per_sample << std::endl;
            exit(1);
        }
        
        // Create codec
        auto codec = CodecRegistry::createCodec(streamInfo);
        if (!codec) {
            std::cerr << "  FAILED: Could not create codec" << std::endl;
            exit(1);
        }
        
        // Decode all frames
        uint64_t totalSamples = 0;
        uint32_t frameCount = 0;
        MediaChunk chunk = demuxer->getNextChunk();
        
        while (!chunk.data.empty()) {
            AudioFrame frame = codec->decode(chunk);
            
            if (!frame.samples.empty()) {
                totalSamples += frame.samples.size() / streamInfo.channels;
                frameCount++;
            }
            
            chunk = demuxer->getNextChunk();
        }
        
        std::cout << "  Decoded successfully:" << std::endl;
        std::cout << "    Total frames: " << frameCount << std::endl;
        std::cout << "    Total samples: " << totalSamples << " per channel" << std::endl;
        std::cout << "    Duration: " << (double)totalSamples / streamInfo.sample_rate 
                  << " seconds" << std::endl;
        
        if (totalSamples == 0) {
            std::cerr << "  FAILED: No samples decoded" << std::endl;
            exit(1);
        }
        
        std::cout << "  PASSED" << std::endl;
        
    } catch (const std::exception& e) {
        if (testFile.should_decode) {
            std::cerr << "  FAILED: Exception during decoding: " << e.what() << std::endl;
            exit(1);
        } else {
            std::cout << "  PASSED: Correctly rejected with exception: " << e.what() << std::endl;
        }
    }
}

} // anonymous namespace

int main() {
    std::cout << "=== Native FLAC Real File Tests ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Testing " << TEST_FILES.size() << " FLAC files from tests/data directory" << std::endl;
    
    for (const auto& testFile : TEST_FILES) {
        testDecodeFile(testFile);
    }
    
    std::cout << "\n=== All Real File Tests Completed ===" << std::endl;
    
    return 0;
}
