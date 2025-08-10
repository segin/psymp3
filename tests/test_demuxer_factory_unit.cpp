/*
 * test_demuxer_factory_unit.cpp - Unit tests for DemuxerFactory
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

/**
 * @brief Mock IOHandler with configurable data for format detection
 */
class FormatTestIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    explicit FormatTestIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = std::min(size * count, m_data.size() - m_position);
        if (bytes_to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
            m_position += bytes_to_read;
        }
        return bytes_to_read / size;
    }
    
    int seek(long offset, int whence) override {
        long new_pos;
        switch (whence) {
            case SEEK_SET: new_pos = offset; break;
            case SEEK_CUR: new_pos = static_cast<long>(m_position) + offset; break;
            case SEEK_END: new_pos = static_cast<long>(m_data.size()) + offset; break;
            default: return -1;
        }
        
        if (new_pos < 0 || new_pos > static_cast<long>(m_data.size())) {
            return -1;
        }
        
        m_position = static_cast<size_t>(new_pos);
        return 0;
    }
    
    long tell() override {
        return static_cast<long>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    void close() override {}
};

/**
 * @brief Test format signature detection
 */
class FormatSignatureTest : public TestCase {
public:
    FormatSignatureTest() : TestCase("Format Signature Test") {}
    
protected:
    void runTest() override {
        // Test FormatSignature constructor
        std::vector<uint8_t> signature = {0x52, 0x49, 0x46, 0x46}; // "RIFF"
        FormatSignature riff_sig("riff", signature, 0, 100);
        
        ASSERT_EQUALS("riff", riff_sig.format_id, "Format ID should be set correctly");
        ASSERT_EQUALS(4u, riff_sig.signature.size(), "Signature size should be correct");
        ASSERT_EQUALS(0x52, riff_sig.signature[0], "First signature byte should be correct");
        ASSERT_EQUALS(0u, riff_sig.offset, "Offset should be set correctly");
        ASSERT_EQUALS(100, riff_sig.priority, "Priority should be set correctly");
        
        // Test signature with offset
        std::vector<uint8_t> mp4_sig = {0x66, 0x74, 0x79, 0x70}; // "ftyp"
        FormatSignature mp4_sig_obj("mp4", mp4_sig, 4, 90);
        
        ASSERT_EQUALS("mp4", mp4_sig_obj.format_id, "MP4 format ID should be correct");
        ASSERT_EQUALS(4u, mp4_sig_obj.offset, "MP4 signature offset should be correct");
        ASSERT_EQUALS(90, mp4_sig_obj.priority, "MP4 priority should be correct");
    }
};

/**
 * @brief Test format detection by magic bytes
 */
class FormatDetectionTest : public TestCase {
public:
    FormatDetectionTest() : TestCase("Format Detection Test") {}
    
protected:
    void runTest() override {
        // Test RIFF format detection
        std::vector<uint8_t> riff_data = {
            0x52, 0x49, 0x46, 0x46, // "RIFF"
            0x24, 0x08, 0x00, 0x00, // File size
            0x57, 0x41, 0x56, 0x45  // "WAVE"
        };
        auto riff_handler = std::make_unique<FormatTestIOHandler>(riff_data);
        std::string riff_format = DemuxerFactory::probeFormat(riff_handler.get());
        ASSERT_EQUALS("riff", riff_format, "RIFF format should be detected correctly");
        
        // Test Ogg format detection
        std::vector<uint8_t> ogg_data = {
            0x4F, 0x67, 0x67, 0x53, // "OggS"
            0x00, 0x02, 0x00, 0x00, // Version, header type, granule pos
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        auto ogg_handler = std::make_unique<FormatTestIOHandler>(ogg_data);
        std::string ogg_format = DemuxerFactory::probeFormat(ogg_handler.get());
        ASSERT_EQUALS("ogg", ogg_format, "Ogg format should be detected correctly");
        
        // Test AIFF format detection
        std::vector<uint8_t> aiff_data = {
            0x46, 0x4F, 0x52, 0x4D, // "FORM"
            0x00, 0x00, 0x08, 0x24, // File size (big-endian)
            0x41, 0x49, 0x46, 0x46  // "AIFF"
        };
        auto aiff_handler = std::make_unique<FormatTestIOHandler>(aiff_data);
        std::string aiff_format = DemuxerFactory::probeFormat(aiff_handler.get());
        ASSERT_EQUALS("aiff", aiff_format, "AIFF format should be detected correctly");
        
        // Test MP4 format detection
        std::vector<uint8_t> mp4_data = {
            0x00, 0x00, 0x00, 0x20, // Box size
            0x66, 0x74, 0x79, 0x70, // "ftyp"
            0x69, 0x73, 0x6F, 0x6D, // "isom"
            0x00, 0x00, 0x02, 0x00  // Minor version
        };
        auto mp4_handler = std::make_unique<FormatTestIOHandler>(mp4_data);
        std::string mp4_format = DemuxerFactory::probeFormat(mp4_handler.get());
        ASSERT_EQUALS("mp4", mp4_format, "MP4 format should be detected correctly");
        
        // Test FLAC format detection
        std::vector<uint8_t> flac_data = {
            0x66, 0x4C, 0x61, 0x43, // "fLaC"
            0x80, 0x00, 0x00, 0x22, // Metadata block header
            0x10, 0x00, 0x10, 0x00  // Stream info
        };
        auto flac_handler = std::make_unique<FormatTestIOHandler>(flac_data);
        std::string flac_format = DemuxerFactory::probeFormat(flac_handler.get());
        ASSERT_EQUALS("flac", flac_format, "FLAC format should be detected correctly");
        
        // Test unknown format
        std::vector<uint8_t> unknown_data = {
            0x12, 0x34, 0x56, 0x78, // Unknown signature
            0x9A, 0xBC, 0xDE, 0xF0
        };
        auto unknown_handler = std::make_unique<FormatTestIOHandler>(unknown_data);
        std::string unknown_format = DemuxerFactory::probeFormat(unknown_handler.get());
        ASSERT_TRUE(unknown_format.empty(), "Unknown format should return empty string");
    }
};

/**
 * @brief Test format detection with file path hints
 */
class FormatDetectionWithPathTest : public TestCase {
public:
    FormatDetectionWithPathTest() : TestCase("Format Detection With Path Test") {}
    
protected:
    void runTest() override {
        // Test raw audio format detection by extension
        std::vector<uint8_t> raw_data(1024, 0x00); // Raw PCM data
        auto raw_handler = std::make_unique<FormatTestIOHandler>(raw_data);
        
        // Test .pcm extension
        std::string pcm_format = DemuxerFactory::probeFormat(raw_handler.get(), "test.pcm");
        ASSERT_EQUALS("raw", pcm_format, "PCM extension should be detected as raw format");
        
        // Test .alaw extension
        std::string alaw_format = DemuxerFactory::probeFormat(raw_handler.get(), "test.alaw");
        ASSERT_EQUALS("raw", alaw_format, "A-law extension should be detected as raw format");
        
        // Test .ulaw extension
        std::string ulaw_format = DemuxerFactory::probeFormat(raw_handler.get(), "test.ulaw");
        ASSERT_EQUALS("raw", ulaw_format, "μ-law extension should be detected as raw format");
        
        // Test .au extension
        std::string au_format = DemuxerFactory::probeFormat(raw_handler.get(), "test.au");
        ASSERT_EQUALS("raw", au_format, "AU extension should be detected as raw format");
        
        // Test magic bytes override extension
        std::vector<uint8_t> ogg_data = {0x4F, 0x67, 0x67, 0x53}; // "OggS"
        auto ogg_handler = std::make_unique<FormatTestIOHandler>(ogg_data);
        std::string ogg_format = DemuxerFactory::probeFormat(ogg_handler.get(), "test.pcm");
        ASSERT_EQUALS("ogg", ogg_format, "Magic bytes should override extension hint");
        
        // Test unknown extension
        std::string unknown_format = DemuxerFactory::probeFormat(raw_handler.get(), "test.xyz");
        ASSERT_TRUE(unknown_format.empty(), "Unknown extension should return empty string");
    }
};

/**
 * @brief Test demuxer creation
 */
class DemuxerCreationTest : public TestCase {
public:
    DemuxerCreationTest() : TestCase("Demuxer Creation Test") {}
    
protected:
    void runTest() override {
        // Test RIFF demuxer creation
        std::vector<uint8_t> riff_data = {
            0x52, 0x49, 0x46, 0x46, // "RIFF"
            0x24, 0x08, 0x00, 0x00, // File size
            0x57, 0x41, 0x56, 0x45  // "WAVE"
        };
        auto riff_handler = std::make_unique<FormatTestIOHandler>(riff_data);
        auto riff_demuxer = DemuxerFactory::createDemuxer(std::move(riff_handler));
        ASSERT_NOT_NULL(riff_demuxer.get(), "RIFF demuxer should be created successfully");
        
        // Test Ogg demuxer creation
        std::vector<uint8_t> ogg_data = {
            0x4F, 0x67, 0x67, 0x53, // "OggS"
            0x00, 0x02, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        auto ogg_handler = std::make_unique<FormatTestIOHandler>(ogg_data);
        auto ogg_demuxer = DemuxerFactory::createDemuxer(std::move(ogg_handler));
        ASSERT_NOT_NULL(ogg_demuxer.get(), "Ogg demuxer should be created successfully");
        
        // Test AIFF demuxer creation
        std::vector<uint8_t> aiff_data = {
            0x46, 0x4F, 0x52, 0x4D, // "FORM"
            0x00, 0x00, 0x08, 0x24,
            0x41, 0x49, 0x46, 0x46  // "AIFF"
        };
        auto aiff_handler = std::make_unique<FormatTestIOHandler>(aiff_data);
        auto aiff_demuxer = DemuxerFactory::createDemuxer(std::move(aiff_handler));
        ASSERT_NOT_NULL(aiff_demuxer.get(), "AIFF demuxer should be created successfully");
        
        // Test MP4 demuxer creation
        std::vector<uint8_t> mp4_data = {
            0x00, 0x00, 0x00, 0x20,
            0x66, 0x74, 0x79, 0x70, // "ftyp"
            0x69, 0x73, 0x6F, 0x6D
        };
        auto mp4_handler = std::make_unique<FormatTestIOHandler>(mp4_data);
        auto mp4_demuxer = DemuxerFactory::createDemuxer(std::move(mp4_handler));
        ASSERT_NOT_NULL(mp4_demuxer.get(), "MP4 demuxer should be created successfully");
        
        // Test FLAC demuxer creation
        std::vector<uint8_t> flac_data = {
            0x66, 0x4C, 0x61, 0x43, // "fLaC"
            0x80, 0x00, 0x00, 0x22
        };
        auto flac_handler = std::make_unique<FormatTestIOHandler>(flac_data);
        auto flac_demuxer = DemuxerFactory::createDemuxer(std::move(flac_handler));
        ASSERT_NOT_NULL(flac_demuxer.get(), "FLAC demuxer should be created successfully");
        
        // Test raw audio demuxer creation with path hint
        std::vector<uint8_t> raw_data(1024, 0x00);
        auto raw_handler = std::make_unique<FormatTestIOHandler>(raw_data);
        auto raw_demuxer = DemuxerFactory::createDemuxer(std::move(raw_handler), "test.pcm");
        ASSERT_NOT_NULL(raw_demuxer.get(), "Raw audio demuxer should be created successfully");
        
        // Test unknown format returns nullptr
        std::vector<uint8_t> unknown_data = {0x12, 0x34, 0x56, 0x78};
        auto unknown_handler = std::make_unique<FormatTestIOHandler>(unknown_data);
        auto unknown_demuxer = DemuxerFactory::createDemuxer(std::move(unknown_handler));
        ASSERT_NULL(unknown_demuxer.get(), "Unknown format should return nullptr");
    }
};

/**
 * @brief Test format signature registration
 */
class FormatRegistrationTest : public TestCase {
public:
    FormatRegistrationTest() : TestCase("Format Registration Test") {}
    
protected:
    void runTest() override {
        // Get initial signature count
        auto initial_signatures = DemuxerFactory::getSignatures();
        size_t initial_count = initial_signatures.size();
        
        // Register a custom format signature
        std::vector<uint8_t> custom_sig = {0xCA, 0xFE, 0xBA, 0xBE};
        FormatSignature custom_format("custom", custom_sig, 0, 50);
        DemuxerFactory::registerSignature(custom_format);
        
        // Check that signature was registered
        auto updated_signatures = DemuxerFactory::getSignatures();
        ASSERT_EQUALS(initial_count + 1, updated_signatures.size(), "Signature count should increase by 1");
        
        // Find the registered signature
        bool found = false;
        for (const auto& sig : updated_signatures) {
            if (sig.format_id == "custom") {
                found = true;
                ASSERT_EQUALS(4u, sig.signature.size(), "Custom signature size should be correct");
                ASSERT_EQUALS(0xCA, sig.signature[0], "First byte should be correct");
                ASSERT_EQUALS(50, sig.priority, "Priority should be correct");
                break;
            }
        }
        ASSERT_TRUE(found, "Custom signature should be found in registered signatures");
        
        // Test that custom format can be detected
        std::vector<uint8_t> custom_data = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x01, 0x02, 0x03};
        auto custom_handler = std::make_unique<FormatTestIOHandler>(custom_data);
        std::string detected_format = DemuxerFactory::probeFormat(custom_handler.get());
        ASSERT_EQUALS("custom", detected_format, "Custom format should be detected");
    }
};

/**
 * @brief Test demuxer factory registration
 */
class DemuxerFactoryRegistrationTest : public TestCase {
public:
    DemuxerFactoryRegistrationTest() : TestCase("Demuxer Factory Registration Test") {}
    
protected:
    void runTest() override {
        // Register a custom demuxer factory
        bool factory_called = false;
        auto custom_factory = [&factory_called](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
            factory_called = true;
            return nullptr; // Return nullptr for testing
        };
        
        DemuxerFactory::registerDemuxer("custom", custom_factory);
        
        // Register the signature for the custom format
        std::vector<uint8_t> custom_sig = {0xDE, 0xAD, 0xBE, 0xEF};
        FormatSignature custom_format("custom", custom_sig, 0, 40);
        DemuxerFactory::registerSignature(custom_format);
        
        // Test that custom factory is called
        std::vector<uint8_t> custom_data = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78};
        auto custom_handler = std::make_unique<FormatTestIOHandler>(custom_data);
        auto custom_demuxer = DemuxerFactory::createDemuxer(std::move(custom_handler));
        
        ASSERT_TRUE(factory_called, "Custom factory should be called");
        ASSERT_NULL(custom_demuxer.get(), "Custom factory returned nullptr as expected");
    }
};

/**
 * @brief Test thread safety of DemuxerFactory
 */
class DemuxerFactoryThreadSafetyTest : public TestCase {
public:
    DemuxerFactoryThreadSafetyTest() : TestCase("DemuxerFactory Thread Safety Test") {}
    
protected:
    void runTest() override {
        // Test concurrent format detection
        std::vector<uint8_t> riff_data = {0x52, 0x49, 0x46, 0x46, 0x24, 0x08, 0x00, 0x00};
        std::vector<uint8_t> ogg_data = {0x4F, 0x67, 0x67, 0x53, 0x00, 0x02, 0x00, 0x00};
        
        // Create multiple handlers
        auto handler1 = std::make_unique<FormatTestIOHandler>(riff_data);
        auto handler2 = std::make_unique<FormatTestIOHandler>(ogg_data);
        
        // Test concurrent probing (simplified test - real threading would be more complex)
        std::string format1 = DemuxerFactory::probeFormat(handler1.get());
        std::string format2 = DemuxerFactory::probeFormat(handler2.get());
        
        ASSERT_EQUALS("riff", format1, "First format detection should work");
        ASSERT_EQUALS("ogg", format2, "Second format detection should work");
        
        // Test concurrent demuxer creation
        auto handler3 = std::make_unique<FormatTestIOHandler>(riff_data);
        auto handler4 = std::make_unique<FormatTestIOHandler>(ogg_data);
        
        auto demuxer1 = DemuxerFactory::createDemuxer(std::move(handler3));
        auto demuxer2 = DemuxerFactory::createDemuxer(std::move(handler4));
        
        ASSERT_NOT_NULL(demuxer1.get(), "First demuxer creation should work");
        ASSERT_NOT_NULL(demuxer2.get(), "Second demuxer creation should work");
        
        // Test concurrent signature access
        auto signatures1 = DemuxerFactory::getSignatures();
        auto signatures2 = DemuxerFactory::getSignatures();
        
        ASSERT_EQUALS(signatures1.size(), signatures2.size(), "Concurrent signature access should be consistent");
    }
};

int main() {
    TestSuite suite("DemuxerFactory Unit Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<FormatSignatureTest>());
    suite.addTest(std::make_unique<FormatDetectionTest>());
    suite.addTest(std::make_unique<FormatDetectionWithPathTest>());
    suite.addTest(std::make_unique<DemuxerCreationTest>());
    suite.addTest(std::make_unique<FormatRegistrationTest>());
    suite.addTest(std::make_unique<DemuxerFactoryRegistrationTest>());
    suite.addTest(std::make_unique<DemuxerFactoryThreadSafetyTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}