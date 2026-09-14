/*
 * test_cover_art.cpp - Cover art selection and data: URIs
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using PsyMP3::Tag::Picture;
using PsyMP3::Tag::PictureType;
namespace ImageUtils = PsyMP3::Tag::ImageUtils;

namespace {

/// @p head followed by zero bytes out to @p size.
std::vector<uint8_t> image(std::initializer_list<int> head, size_t size = 64)
{
    std::vector<uint8_t> data;
    for (int byte : head) {
        data.push_back(static_cast<uint8_t>(byte));
    }
    data.resize(std::max(data.size(), size), 0);
    return data;
}

const std::initializer_list<int> kJpeg = {0xFF, 0xD8, 0xFF, 0xE0};
const std::initializer_list<int> kPng = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

Picture picture(PictureType type, std::vector<uint8_t> data, const std::string& mime = "image/jpeg")
{
    Picture p;
    p.type = type;
    p.mime_type = mime;
    p.data = std::move(data);
    return p;
}

/// A stream with no audio and no file, only the tag it is given.
class TagOnlyStream : public Stream {
public:
    explicit TagOnlyStream(std::unique_ptr<PsyMP3::Tag::Tag> tag)
        : Stream(TagLib::String("/nonexistent/psymp3-cover-art-test.flac"))
    {
        m_tag = std::move(tag);
    }
    size_t getData(size_t, void*) { return 0; }
    void seekTo(unsigned long) {}
    bool eof() { return true; }
};

std::unique_ptr<PsyMP3::Tag::Tag> tagWith(const std::vector<Picture>& pictures)
{
    return std::make_unique<PsyMP3::Tag::VorbisCommentTag>(
        "test", std::map<std::string, std::vector<std::string>>{}, pictures);
}

class SniffTest : public TestCase {
public:
    SniffTest() : TestCase("Image formats are recognised from their own headers") {}

protected:
    void runTest() override
    {
        ASSERT_EQUALS(std::string("image/jpeg"), ImageUtils::sniffMimeType(image(kJpeg)), "JPEG");
        ASSERT_EQUALS(std::string("image/png"), ImageUtils::sniffMimeType(image(kPng)), "PNG");
        ASSERT_EQUALS(std::string("image/gif"), ImageUtils::sniffMimeType(image({'G', 'I', 'F', '8', '9', 'a'})), "GIF89a");
        ASSERT_EQUALS(std::string("image/webp"),
                      ImageUtils::sniffMimeType(image({'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'})), "WebP");
        ASSERT_EQUALS(std::string("image/bmp"),
                      ImageUtils::sniffMimeType(image({'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0})), "BMP");
        ASSERT_EQUALS(std::string(), ImageUtils::sniffMimeType(image({'B', 'M'})),
                      "BM without a pixel offset inside the data is not trusted");
        ASSERT_EQUALS(std::string(), ImageUtils::sniffMimeType(image({'h', 'e', 'l', 'l', 'o'})), "text");
        ASSERT_EQUALS(std::string(), ImageUtils::sniffMimeType({}), "nothing");
    }
};

class DataUriTest : public TestCase {
public:
    DataUriTest() : TestCase("A data: URI carries the bytes under the type they really are") {}

protected:
    void runTest() override
    {
        // Taggers mislabel: a JPEG declared as PNG is published as JPEG.
        const Picture mislabelled = picture(PictureType::FrontCover, image(kJpeg), "image/png");
        const std::string uri = ImageUtils::dataUri(mislabelled);
        const std::string prefix = "data:image/jpeg;base64,";
        ASSERT_TRUE(uri.compare(0, prefix.size(), prefix) == 0, "sniffed type, base64 marker");
        ASSERT_TRUE(PsyMP3::Core::Utility::Base64::decode(uri.substr(prefix.size())) == mislabelled.data,
                    "the payload decodes back to the picture");

        ASSERT_TRUE(ImageUtils::dataUri(picture(PictureType::FrontCover, image({'x', 'y', 'z'}))).empty(),
                    "data that is no image gives no URI");

        ASSERT_FALSE(ImageUtils::dataUri(picture(PictureType::FrontCover,
                                                 image(kJpeg, ImageUtils::kMaxDataUriImageBytes))).empty(),
                     "an image at the limit is encoded");
        ASSERT_TRUE(ImageUtils::dataUri(picture(PictureType::FrontCover,
                                                image(kJpeg, ImageUtils::kMaxDataUriImageBytes + 1))).empty(),
                    "one byte over the limit is not");
    }
};

class CoverSelectionTest : public TestCase {
public:
    CoverSelectionTest() : TestCase("The front cover is chosen, else the first picture with data") {}

protected:
    void runTest() override
    {
        TagOnlyStream front(tagWith({picture(PictureType::BackCover, image(kJpeg)),
                                     picture(PictureType::FrontCover, image(kPng), "image/png")}));
        auto cover = front.getCoverArt();
        ASSERT_TRUE(cover && cover->type == PictureType::FrontCover, "front cover preferred over an earlier back cover");
        ASSERT_EQUALS(std::string("image/png"), ImageUtils::sniffMimeType(cover->data), "and it is the PNG");

        TagOnlyStream other(tagWith({picture(PictureType::Artist, image(kPng), "image/png")}));
        cover = other.getCoverArt();
        ASSERT_TRUE(cover && cover->type == PictureType::Artist, "without a front cover, the first picture");

        TagOnlyStream empty_front(tagWith({picture(PictureType::FrontCover, {}),
                                           picture(PictureType::Other, image(kJpeg))}));
        cover = empty_front.getCoverArt();
        ASSERT_TRUE(cover && cover->type == PictureType::Other, "a front cover with no data is passed over");

        TagOnlyStream none(nullptr);
        ASSERT_FALSE(none.getCoverArt().has_value(), "no tag, no file: no cover");
    }
};

} // namespace

int main()
{
    TestSuite suite("Cover Art Tests");
    suite.addTest(std::make_unique<SniffTest>());
    suite.addTest(std::make_unique<DataUriTest>());
    suite.addTest(std::make_unique<CoverSelectionTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
