/*
 * test_ebml_reader.cpp - EBML variable-length integers and element payloads
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"

using namespace TestFramework;
using PsyMP3::Demuxer::Matroska::EBMLElement;
using PsyMP3::Demuxer::Matroska::EBMLReader;
using PsyMP3::IO::MemoryIOHandler;

namespace {

/// An EBMLReader over a literal run of bytes. Every case here is small enough
/// to write out by hand, which is the point: the reader can be pinned down
/// exactly without a Matroska file anywhere in sight.
class Bytes {
public:
    explicit Bytes(std::vector<uint8_t> data)
        : m_data(std::move(data))
        , m_handler(std::make_unique<MemoryIOHandler>(m_data.data(), m_data.size(), false))
        , m_reader(m_handler.get())
    {
    }
    EBMLReader& reader() { return m_reader; }

private:
    std::vector<uint8_t> m_data;
    std::unique_ptr<MemoryIOHandler> m_handler;
    EBMLReader m_reader;
};

/// The width of a VINT is carried in the leading zeros of its first byte.
class VIntWidthTest : public TestCase {
public:
    VIntWidthTest() : TestCase("A VINT's width comes from the leading zeros of its first byte") {}

protected:
    void runTest() override
    {
        ASSERT_EQUALS(1, EBMLReader::vintLength(0x80), "0x80 starts a one-byte VINT");
        ASSERT_EQUALS(1, EBMLReader::vintLength(0xFF), "0xFF starts a one-byte VINT");
        ASSERT_EQUALS(2, EBMLReader::vintLength(0x40), "0x40 starts a two-byte VINT");
        ASSERT_EQUALS(3, EBMLReader::vintLength(0x20), "0x20 starts a three-byte VINT");
        ASSERT_EQUALS(4, EBMLReader::vintLength(0x10), "0x10 starts a four-byte VINT");
        ASSERT_EQUALS(8, EBMLReader::vintLength(0x01), "0x01 starts an eight-byte VINT");
        // A first byte with no marker bit would mean a ninth byte or beyond,
        // which EBML does not define.
        ASSERT_EQUALS(0, EBMLReader::vintLength(0x00), "0x00 cannot start a VINT");
    }
};

/// Marker handling is the one thing that differs between decoding an ID and
/// decoding a size, and getting it backwards is silent: IDs stop matching and
/// sizes come out enormous.
class VIntMarkerTest : public TestCase {
public:
    VIntMarkerTest() : TestCase("IDs keep the VINT marker bit and sizes strip it") {}

protected:
    void runTest() override
    {
        const uint8_t ebml_header[] = {0x1A, 0x45, 0xDF, 0xA3};
        uint64_t value = 0;
        ASSERT_EQUALS(size_t{4},
                      EBMLReader::decodeVInt(ebml_header, 4, value, true),
                      "The EBML header ID is four bytes");
        ASSERT_TRUE(value == 0x1A45DFA3ULL,
                    "An ID decodes to the bytes as written, marker included -- the "
                    "EBML magic is 0x1A45DFA3, which is how the specification "
                    "spells it and how a lookup table has to spell it too");

        // The same bytes read as a size have the marker taken off.
        ASSERT_EQUALS(size_t{4},
                      EBMLReader::decodeVInt(ebml_header, 4, value, false),
                      "Four bytes consumed either way");
        ASSERT_TRUE(value == 0x0A45DFA3ULL,
                    "A size drops the marker bit");

        // One byte, the common case for small sizes.
        const uint8_t one[] = {0x85};
        ASSERT_EQUALS(size_t{1}, EBMLReader::decodeVInt(one, 1, value, false), "one byte");
        ASSERT_TRUE(value == 5, "0x85 is a size of 5");

        // Two encodings of the same value: 0x85 and 0x40 0x05 both mean 5.
        // Matroska writers pad sizes to a fixed width all the time.
        const uint8_t padded[] = {0x40, 0x05};
        ASSERT_EQUALS(size_t{2}, EBMLReader::decodeVInt(padded, 2, value, false), "two bytes");
        ASSERT_TRUE(value == 5, "A padded size decodes to the same value as a short one");
    }
};

/// An all-ones size means "runs until something else ends it", which is how a
/// muxer writing to a socket emits a Segment whose length it cannot know.
class VIntUnknownSizeTest : public TestCase {
public:
    VIntUnknownSizeTest() : TestCase("A size with every value bit set is an unknown size") {}

protected:
    void runTest() override
    {
        uint64_t value = 0;
        bool unknown = false;

        const uint8_t one_byte[] = {0xFF};
        EBMLReader::decodeVInt(one_byte, 1, value, false, &unknown);
        ASSERT_TRUE(unknown, "0xFF is the one-byte unknown size");

        const uint8_t two_byte[] = {0x7F, 0xFF};
        EBMLReader::decodeVInt(two_byte, 2, value, false, &unknown);
        ASSERT_TRUE(unknown, "0x7FFF is the two-byte unknown size");

        const uint8_t eight_byte[] = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        EBMLReader::decodeVInt(eight_byte, 8, value, false, &unknown);
        ASSERT_TRUE(unknown, "The eight-byte unknown size");

        // One less than all-ones is an ordinary size, not unknown. This is the
        // boundary the check has to land on exactly.
        const uint8_t not_unknown[] = {0xFE};
        EBMLReader::decodeVInt(not_unknown, 1, value, false, &unknown);
        ASSERT_FALSE(unknown, "0xFE is a size of 126, not an unknown size");
        ASSERT_TRUE(value == 126, "0xFE is 126");

        const uint8_t two_byte_ordinary[] = {0x7F, 0xFE};
        EBMLReader::decodeVInt(two_byte_ordinary, 2, value, false, &unknown);
        ASSERT_FALSE(unknown, "0x7FFE is an ordinary two-byte size");
    }
};

/// A truncated VINT must be refused rather than read past its buffer.
class VIntTruncationTest : public TestCase {
public:
    VIntTruncationTest() : TestCase("A VINT longer than the bytes available is refused") {}

protected:
    void runTest() override
    {
        uint64_t value = 0;
        const uint8_t four_byte_id[] = {0x1A, 0x45, 0xDF, 0xA3};
        // The first byte promises four bytes; offer it three.
        ASSERT_EQUALS(size_t{0}, EBMLReader::decodeVInt(four_byte_id, 3, value, true),
                      "A VINT wider than the buffer decodes to nothing");
        ASSERT_EQUALS(size_t{0}, EBMLReader::decodeVInt(four_byte_id, 0, value, true),
                      "An empty buffer decodes to nothing");
        const uint8_t invalid[] = {0x00, 0x11};
        ASSERT_EQUALS(size_t{0}, EBMLReader::decodeVInt(invalid, 2, value, true),
                      "A first byte of zero decodes to nothing");
    }
};

class ElementHeaderTest : public TestCase {
public:
    ElementHeaderTest() : TestCase("Element headers report their ID, size and payload offset") {}

protected:
    void runTest() override
    {
        // EBML header ID, size 5, then five bytes of payload.
        Bytes bytes({0x1A, 0x45, 0xDF, 0xA3, 0x85, 'h', 'e', 'l', 'l', 'o'});
        EBMLElement element;
        ASSERT_TRUE(bytes.reader().readElementHeader(element), "The header reads");
        ASSERT_TRUE(element.id == 0x1A45DFA3ULL, "ID is the EBML magic");
        ASSERT_TRUE(element.size == 5, "Size is 5");
        ASSERT_FALSE(element.unknown_size, "The size is known");
        ASSERT_TRUE(element.header_offset == 0, "The header starts at 0");
        ASSERT_TRUE(element.data_offset == 5, "The payload starts after ID and size");
        ASSERT_TRUE(element.end() == 10, "The element ends at 10");
        ASSERT_TRUE(bytes.reader().readString(element) == "hello", "The payload reads back");

        // Reading the payload leaves the position at the element's end, so a
        // sibling can be read straight afterwards without an explicit seek.
        ASSERT_TRUE(bytes.reader().tell() == 10, "Position ends up past the payload");
        EBMLElement past_end;
        ASSERT_FALSE(bytes.reader().readElementHeader(past_end),
                     "Running out of input is a clean stop, not an error");
    }
};

class IntegerPayloadTest : public TestCase {
public:
    IntegerPayloadTest() : TestCase("Integers decode big-endian, zero-width and sign-extended") {}

protected:
    void runTest() override
    {
        {   // Unsigned, three bytes: 0x010203.
            Bytes bytes({0x81, 0x83, 0x01, 0x02, 0x03});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(bytes.reader().readUInt(element) == 0x010203ULL,
                        "A three-byte unsigned integer is big-endian");
        }
        {   // A zero-length integer is legal EBML and means zero. Matroska
            // leans on it to leave a defaulted field present but empty.
            Bytes bytes({0x81, 0x80});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(element.size == 0, "The payload is empty");
            ASSERT_TRUE(bytes.reader().readUInt(element) == 0,
                        "A zero-length integer is zero, not an error");
        }
        {   // Signed, one byte: 0xFF is -1, and must not come back as 255.
            Bytes bytes({0x81, 0x81, 0xFF});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(bytes.reader().readInt(element) == -1,
                        "A one-byte 0xFF sign-extends to -1, not 255");
        }
        {   // Signed, two bytes: 0xFF00 is -256.
            Bytes bytes({0x81, 0x82, 0xFF, 0x00});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(bytes.reader().readInt(element) == -256,
                        "A two-byte signed integer sign-extends from 16 bits");
        }
        {   // A positive value with the high bit clear stays positive.
            Bytes bytes({0x81, 0x82, 0x7F, 0xFF});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(bytes.reader().readInt(element) == 32767,
                        "0x7FFF is 32767");
        }
    }
};

class FloatPayloadTest : public TestCase {
public:
    FloatPayloadTest() : TestCase("Floats decode as big-endian IEEE 754 at four and eight bytes") {}

protected:
    void runTest() override
    {
        {   // 48000.0f is 0x474EA000. Matroska writes SamplingFrequency as a
            // float, so this is the shape of a real track header field.
            Bytes bytes({0x81, 0x84, 0x47, 0x3B, 0x80, 0x00});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(bytes.reader().readFloat(element) == 48000.0,
                        "A four-byte float decodes big-endian");
        }
        {   // 44100.0 as a double is 0x40E5888000000000.
            Bytes bytes({0x81, 0x88, 0x40, 0xE5, 0x88, 0x80, 0x00, 0x00, 0x00, 0x00});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(bytes.reader().readFloat(element) == 44100.0,
                        "An eight-byte float decodes big-endian");
        }
        {   Bytes bytes({0x81, 0x80});
            EBMLElement element;
            bytes.reader().readElementHeader(element);
            ASSERT_TRUE(bytes.reader().readFloat(element) == 0.0,
                        "A zero-length float is 0.0");
        }
    }
};

class StringPaddingTest : public TestCase {
public:
    StringPaddingTest() : TestCase("Strings drop the NUL padding EBML allows") {}

protected:
    void runTest() override
    {
        // EBML permits padding a string out with NULs. Left in place they make
        // every CodecID comparison fail, and the failure is invisible in a log.
        Bytes bytes({0x81, 0x88, 'A', '_', 'O', 'P', 'U', 'S', 0x00, 0x00});
        EBMLElement element;
        bytes.reader().readElementHeader(element);
        const std::string codec = bytes.reader().readString(element);
        ASSERT_TRUE(codec == "A_OPUS",
                    "A NUL-padded string compares equal to the unpadded form");
        ASSERT_TRUE(codec.size() == 6, "The padding is gone, not merely invisible");
    }
};

class UnknownSizeElementTest : public TestCase {
public:
    UnknownSizeElementTest() : TestCase("An element of unknown size is reported, not guessed at") {}

protected:
    void runTest() override
    {
        // Segment ID (0x18538067) with a one-byte unknown size, as a live
        // WebM muxer writes it.
        Bytes bytes({0x18, 0x53, 0x80, 0x67, 0xFF, 0x00});
        EBMLElement element;
        ASSERT_TRUE(bytes.reader().readElementHeader(element), "The header reads");
        ASSERT_TRUE(element.id == 0x18538067ULL, "The Segment ID");
        ASSERT_TRUE(element.unknown_size, "The size is flagged unknown");
        ASSERT_TRUE(element.data_offset == 5, "The payload still starts after the header");

        // Skipping is impossible without a size, and silently skipping zero
        // bytes would spin the caller's loop forever.
        bool threw = false;
        try {
            bytes.reader().skip(element);
        } catch (const std::exception&) {
            threw = true;
        }
        ASSERT_TRUE(threw, "Skipping an unknown-size element is refused rather than "
                           "quietly treated as a skip of nothing");
    }
};

class HostileSizeTest : public TestCase {
public:
    HostileSizeTest() : TestCase("An implausible payload size is refused, not allocated") {}

protected:
    void runTest() override
    {
        // A size field claiming ~2^48 bytes, which a truncated file produces by
        // accident often enough. The file is six bytes long.
        Bytes bytes({0x81, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00});
        EBMLElement element;
        ASSERT_TRUE(bytes.reader().readElementHeader(element), "The header reads");
        ASSERT_TRUE(element.size > EBMLReader::kMaxBinarySize, "The claimed size is huge");

        bool threw = false;
        try {
            bytes.reader().readBinary(element);
        } catch (const std::exception&) {
            threw = true;
        }
        ASSERT_TRUE(threw, "A payload larger than the cap is refused before any "
                           "allocation is attempted");
    }
};

class NestedElementTest : public TestCase {
public:
    NestedElementTest() : TestCase("Master elements can be walked child by child") {}

protected:
    void runTest() override
    {
        // A master element (ID 0x1A45DFA3, size 9) holding two children:
        //   0x4286 (EBMLVersion) size 1, value 1
        //   0x42F7 (EBMLReadVersion) size 1, value 1
        Bytes bytes({0x1A, 0x45, 0xDF, 0xA3, 0x88,
                     0x42, 0x86, 0x81, 0x01,
                     0x42, 0xF7, 0x81, 0x01});
        EBMLElement parent;
        ASSERT_TRUE(bytes.reader().readElementHeader(parent), "The parent reads");
        ASSERT_TRUE(parent.size == 8, "The parent spans both children");

        // Walking children is bounded by the parent's end, which is the whole
        // structural contract the Matroska layer will rely on.
        int children = 0;
        uint64_t ids[2] = {0, 0};
        while (bytes.reader().tell() < parent.end()) {
            EBMLElement child;
            ASSERT_TRUE(bytes.reader().readElementHeader(child), "A child reads");
            ASSERT_TRUE(children < 2, "No more than the two children written");
            ids[children] = child.id;
            ASSERT_TRUE(bytes.reader().readUInt(child) == 1, "Each child holds 1");
            ++children;
        }
        ASSERT_EQUALS(2, children, "Both children were walked");
        ASSERT_TRUE(ids[0] == 0x4286ULL, "EBMLVersion");
        ASSERT_TRUE(ids[1] == 0x42F7ULL, "EBMLReadVersion");
        ASSERT_TRUE(bytes.reader().tell() == parent.end(),
                    "The walk lands exactly on the parent's end");
    }
};

} // namespace

int main()
{
    TestSuite suite("EBML Reader Tests");
    suite.addTest(std::make_unique<VIntWidthTest>());
    suite.addTest(std::make_unique<VIntMarkerTest>());
    suite.addTest(std::make_unique<VIntUnknownSizeTest>());
    suite.addTest(std::make_unique<VIntTruncationTest>());
    suite.addTest(std::make_unique<ElementHeaderTest>());
    suite.addTest(std::make_unique<IntegerPayloadTest>());
    suite.addTest(std::make_unique<FloatPayloadTest>());
    suite.addTest(std::make_unique<StringPaddingTest>());
    suite.addTest(std::make_unique<UnknownSizeElementTest>());
    suite.addTest(std::make_unique<HostileSizeTest>());
    suite.addTest(std::make_unique<NestedElementTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
