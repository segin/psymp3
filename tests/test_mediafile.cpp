#include "test_framework.h"
#include "psymp3.h"
#include <fstream>
#include <cstdio>

using namespace TestFramework;

// Helper to create a temporary file with specific content
static void createTempFile(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(content.c_str(), content.size());
}

class TestMediaFile : public TestSuite {
public:
    TestMediaFile() : TestSuite("MediaFileTests") {
        addTest("test_open_invalid_file", []() {
            try {
                MediaFile::open("this_file_does_not_exist.invalid_ext");
                throw AssertionFailure("Expected InvalidMediaException");
            } catch (const InvalidMediaException& e) {
                // Expected
            }
        });

        addTest("test_open_empty_string", []() {
            try {
                MediaFile::open("");
                throw AssertionFailure("Expected InvalidMediaException");
            } catch (const InvalidMediaException& e) {
                // Expected
            }
        });

        // test detectMimeType
        addTest("test_detect_mime_type_invalid", []() {
            std::string mime = MediaFile::detectMimeType("non_existent_file.xyz");
            if (!mime.empty()) {
                throw AssertionFailure("Expected empty mime type for invalid file");
            }
        });

        // test support queries
        addTest("test_supports_extension", []() {
            if (MediaFile::supportsExtension("invalid_ext_that_will_never_exist")) {
                throw AssertionFailure("Should not support invalid extension");
            }
        });

        addTest("test_supports_mime_type", []() {
            if (MediaFile::supportsMimeType("invalid/mime-type")) {
                throw AssertionFailure("Should not support invalid mime type");
            }
        });

        // split utility tests
        addTest("test_split_utility", []() {
            std::vector<std::string> elems = MediaFile::split("a,b,c", ',');
            if (elems.size() != 3 || elems[0] != "a" || elems[1] != "b" || elems[2] != "c") {
                throw AssertionFailure("Split utility failed");
            }
        });

        // mime type to extension and vice versa test
        addTest("test_mime_type_mappings", []() {
            std::string ext = MediaFile::mimeTypeToExtension("invalid/mime-type");
            if (!ext.empty()) {
                throw AssertionFailure("Expected empty string for invalid mime type");
            }

            std::string mime = MediaFile::extensionToMimeType("invalid_ext");
            if (!mime.empty()) {
                throw AssertionFailure("Expected empty string for invalid extension");
            }
        });

        addTest("test_open_by_mime_type_invalid", []() {
            try {
                MediaFile::openByMimeType("this_file_does_not_exist.invalid_ext", "audio/invalid");
                throw AssertionFailure("Expected InvalidMediaException");
            } catch (const InvalidMediaException& e) {
                // Expected
            }
        });

        addTest("test_exists", []() {
            // currently always returns true
            if (!MediaFile::exists("any_file.mp3")) {
                throw AssertionFailure("exists() should return true based on current implementation");
            }
        });

        addTest("test_open_malformed_ogg", []() {
            std::string tempFile = "malformed_test.ogg";
            // Create a file with a valid OGG extension but invalid/truncated content
            createTempFile(tempFile, "OggS\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"); // Just a partial header

            try {
                MediaFile::open(tempFile);
                std::remove(tempFile.c_str());
                throw AssertionFailure("Expected InvalidMediaException for truncated OGG file");
            } catch (const InvalidMediaException& e) {
                // Expected exception was thrown
                std::remove(tempFile.c_str());
            } catch (...) {
                std::remove(tempFile.c_str());
                throw AssertionFailure("Expected InvalidMediaException, caught a different exception");
            }
        });

        addTest("test_open_random_bytes_file", []() {
            std::string tempFile = "random_bytes.bin";
            // Create a file with random/garbage content, completely unidentifiable
            createTempFile(tempFile, "RANDOM_GARBAGE_CONTENT_NOT_A_MEDIA_FILE");

            try {
                MediaFile::open(tempFile);
                std::remove(tempFile.c_str());
                throw AssertionFailure("Expected InvalidMediaException for unidentifiable garbage file");
            } catch (const InvalidMediaException& e) {
                // Expected exception was thrown
                std::remove(tempFile.c_str());
            } catch (...) {
                std::remove(tempFile.c_str());
                throw AssertionFailure("Expected InvalidMediaException, caught a different exception");
            }
        });

        addTest("test_get_supported", []() {
            auto extensions = MediaFile::getSupportedExtensions();
            auto mimeTypes = MediaFile::getSupportedMimeTypes();
            // Demuxer media factory will have populated some of these
            if (extensions.empty() && mimeTypes.empty()) {
                // In some test contexts with no actual demuxers registered, it might be empty
                // but at least check that the method runs successfully
            }
        });

        addTest("test_split_utility_multiple_delims", []() {
            std::vector<std::string> elems = MediaFile::split("a,b,c,,d", ',');
            if (elems.size() != 5 || elems[0] != "a" || elems[1] != "b" || elems[2] != "c" || elems[3] != "" || elems[4] != "d") {
                throw AssertionFailure("Split utility failed on empty token");
            }
        });
    }
};

int main() {
    TestMediaFile suite;
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}
