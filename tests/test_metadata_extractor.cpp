/*
 * test_metadata_extractor.cpp - Unit tests for ISO MetadataExtractor
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "demuxer/iso/MetadataExtractor.h"
#include "io/MemoryIOHandler.h"
#include "test_framework.h"

using namespace PsyMP3::Demuxer::ISO;
using namespace PsyMP3::IO;
using namespace TestFramework;

// Helper to write Big Endian integers
void WriteUInt32BE(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void WriteString(std::vector<uint8_t>& buffer, const std::string& str) {
    for (char c : str) {
        buffer.push_back(static_cast<uint8_t>(c));
    }
}

// Helper to construct a box
// Appends box size (placeholder), type, data, and updates size
void WriteBox(std::vector<uint8_t>& buffer, uint32_t type, const std::vector<uint8_t>& data) {
    uint32_t size = 8 + data.size();
    WriteUInt32BE(buffer, size);
    WriteUInt32BE(buffer, type);
    buffer.insert(buffer.end(), data.begin(), data.end());
}

// Helper to construct a container box (box that contains other boxes)
// Appends box size (placeholder), type, content, and updates size
// Note: This assumes content is already formatted as boxes or raw data
void WriteContainerBox(std::vector<uint8_t>& buffer, uint32_t type, const std::vector<uint8_t>& content) {
    uint32_t size = 8 + content.size();
    WriteUInt32BE(buffer, size);
    WriteUInt32BE(buffer, type);
    buffer.insert(buffer.end(), content.begin(), content.end());
}

// Helper to create a meta box (full box version/flags + content)
void WriteMetaBox(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& content) {
    std::vector<uint8_t> metaContent;
    // Version/Flags (4 bytes) - usually 0
    WriteUInt32BE(metaContent, 0);
    metaContent.insert(metaContent.end(), content.begin(), content.end());

    WriteContainerBox(buffer, BOX_META, metaContent);
}

// Helper to create a data atom (for metadata value)
void WriteDataAtom(std::vector<uint8_t>& buffer, const std::string& value, uint32_t type = 1) { // Type 1 = UTF-8 text
    std::vector<uint8_t> dataContent;
    // Type indicator (4 bytes)
    WriteUInt32BE(dataContent, type);
    // Locale (4 bytes) - usually 0
    WriteUInt32BE(dataContent, 0);
    // Value
    WriteString(dataContent, value);

    WriteContainerBox(buffer, BOX_DATA, dataContent);
}

class TestMetadataExtractor : public TestCase {
public:
    TestMetadataExtractor() : TestCase("TestMetadataExtractor") {}

    void runTest() override {
        testEmptyInput();
        testSimpleMetadata();
        testRecursionLimit();
        testTextPadding();
    }

    void testEmptyInput() {
        MetadataExtractor extractor;
        std::shared_ptr<IOHandler> io = nullptr;

        auto metadata = extractor.ExtractMetadata(io, 0, 0);
        ASSERT_TRUE(metadata.empty(), "Metadata should be empty for null IO");

        std::vector<uint8_t> emptyData;
        io = std::make_shared<MemoryIOHandler>(emptyData.data(), emptyData.size(), true);
        metadata = extractor.ExtractMetadata(io, 0, 0);
        ASSERT_TRUE(metadata.empty(), "Metadata should be empty for empty data");

        // Data too small (< 8 bytes)
        std::vector<uint8_t> smallData = {0, 0, 0, 4, 't', 'e', 's', 't'}; // 8 bytes but not valid box
        io = std::make_shared<MemoryIOHandler>(smallData.data(), smallData.size(), true);
        metadata = extractor.ExtractMetadata(io, 0, 4); // Only passed 4 bytes size
        ASSERT_TRUE(metadata.empty(), "Metadata should be empty for small size");
    }

    void testSimpleMetadata() {
        // Construct: meta -> ilst -> ©nam (title) -> data -> "Test Title"
        std::vector<uint8_t> dataAtom;
        WriteDataAtom(dataAtom, "Test Title");

        std::vector<uint8_t> titleAtom;
        WriteContainerBox(titleAtom, BOX_TITLE, dataAtom);

        std::vector<uint8_t> ilstBox;
        WriteContainerBox(ilstBox, BOX_ILST, titleAtom);

        std::vector<uint8_t> metaBox;
        WriteMetaBox(metaBox, ilstBox);

        std::shared_ptr<IOHandler> io = std::make_shared<MemoryIOHandler>(metaBox.data(), metaBox.size(), true);
        MetadataExtractor extractor;
        auto metadata = extractor.ExtractMetadata(io, 0, metaBox.size());

        ASSERT_FALSE(metadata.empty(), "Metadata should not be empty");
        ASSERT_TRUE(metadata.find("title") != metadata.end(), "Title should be present");
        ASSERT_EQUALS("Test Title", metadata["title"], "Title should be 'Test Title'");
    }

    void testRecursionLimit() {
        // Verify that recursion is limited to ~32 levels.
        // We construct a nested structure: meta -> meta -> ... -> ilst -> title

        // Case 1: Depth 30 (should be found)
        {
            int depth = 30;
            std::vector<uint8_t> dataAtom;
            WriteDataAtom(dataAtom, "Deep Title");

            std::vector<uint8_t> titleAtom;
            WriteContainerBox(titleAtom, BOX_TITLE, dataAtom);

            std::vector<uint8_t> content;
            WriteContainerBox(content, BOX_ILST, titleAtom);

            // Wrap in 'depth' layers of meta boxes
            for (int i = 0; i < depth; ++i) {
                std::vector<uint8_t> wrapper;
                WriteMetaBox(wrapper, content);
                content = wrapper;
            }

            std::shared_ptr<IOHandler> io = std::make_shared<MemoryIOHandler>(content.data(), content.size(), true);
            MetadataExtractor extractor;
            auto metadata = extractor.ExtractMetadata(io, 0, content.size());

            ASSERT_FALSE(metadata.empty(), "Metadata should be found at depth 30");
            ASSERT_EQUALS("Deep Title", metadata["title"], "Title should match");
        }

        // Case 2: Depth 40 (should NOT be found due to limit)
        {
            int depth = 40;
            std::vector<uint8_t> dataAtom;
            WriteDataAtom(dataAtom, "Too Deep Title");

            std::vector<uint8_t> titleAtom;
            WriteContainerBox(titleAtom, BOX_TITLE, dataAtom);

            std::vector<uint8_t> content;
            WriteContainerBox(content, BOX_ILST, titleAtom);

            // Wrap in 'depth' layers of meta boxes
            for (int i = 0; i < depth; ++i) {
                std::vector<uint8_t> wrapper;
                WriteMetaBox(wrapper, content);
                content = wrapper;
            }

            std::shared_ptr<IOHandler> io = std::make_shared<MemoryIOHandler>(content.data(), content.size(), true);
            MetadataExtractor extractor;
            auto metadata = extractor.ExtractMetadata(io, 0, content.size());

            ASSERT_TRUE(metadata.empty(), "Metadata should NOT be found at depth 40 due to recursion limit");
        }
    }

    void testTextPadding() {
        MetadataExtractor extractor;
        // Construct: meta -> ilst -> ©nam (title) -> data -> "Padded Text\0\0 "
        std::vector<uint8_t> dataAtom;
        std::string padded = "Padded Text";
        padded.push_back('\0');
        padded.push_back('\0');
        padded.push_back(' ');
        WriteDataAtom(dataAtom, padded);

        std::vector<uint8_t> titleAtom;
        WriteContainerBox(titleAtom, BOX_TITLE, dataAtom);

        std::vector<uint8_t> ilstBox;
        WriteContainerBox(ilstBox, BOX_ILST, titleAtom);

        std::vector<uint8_t> metaContent;
        WriteUInt32BE(metaContent, 0);
        metaContent.insert(metaContent.end(), ilstBox.begin(), ilstBox.end());

        std::vector<uint8_t> metaBox;
        WriteBox(metaBox, BOX_META, metaContent);

        std::shared_ptr<MemoryIOHandler> io = std::make_shared<MemoryIOHandler>(metaBox.data(), metaBox.size(), true);
        auto metadata = extractor.ExtractMetadata(io, 0, metaBox.size());

        ASSERT_EQUALS("Padded Text", metadata["title"], "Text padding/nulls should be trimmed");
    }
};

int main(int argc, char* argv[]) {
    try {
        TestSuite suite("MetadataExtractorTests");
        suite.addTest(std::make_unique<TestMetadataExtractor>());

        std::vector<TestCaseInfo> results = suite.runAll();

        // Return 0 if all tests passed, 1 otherwise
        return suite.getFailureCount(results) == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Test suite execution failed: " << e.what() << std::endl;
        return 1;
    }
}
