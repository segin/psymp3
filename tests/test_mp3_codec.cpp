#define HAVE_MP3
#define UNIT_TESTING

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <functional>

// Mocks
#include "mocks/taglib/tstring.h"
#include "mocks/mpg123.h"
#include "mocks/io/IOHandler.h"
#include "mocks/stream.h"
#include "mocks/io/FileIOHandler.h"
#include "mocks/io/URI.h"

// Mock Debug class
class Debug {
public:
    template<typename... Args>
    static void log(Args&&... args) {}
};

// Mock Exceptions
namespace PsyMP3 {
namespace Core {
    class InvalidMediaException : public std::runtime_error {
    public:
        InvalidMediaException(const std::string& msg) : std::runtime_error(msg) {}
    };
    class BadFormatException : public std::runtime_error {
    public:
        BadFormatException(const std::string& msg) : std::runtime_error(msg) {}
    };
}
}
using PsyMP3::Core::InvalidMediaException;
using PsyMP3::Core::BadFormatException;

// Include the header
#include "../include/codecs/mp3/MP3Codec.h"
// Include the source file directly
#include "../src/codecs/mp3/MP3Codec.cpp"

// Mock mpg123 implementation
static void (*s_cleanup_callback)(void*) = nullptr;
static void* s_cleanup_handle = nullptr;
static bool s_mpg123_new_fail = false;

mpg123_handle *mpg123_new(const char *decoder, int *error) {
    if (s_mpg123_new_fail) {
        if (error) *error = MPG123_ERR;
        return nullptr;
    }
    if (error) *error = MPG123_OK;
    return (mpg123_handle*)1; // Return dummy non-null handle
}
void mpg123_delete(mpg123_handle *mh) {}
int mpg123_param(mpg123_handle *mh, long type, long value, double fvalue) { return MPG123_OK; }

int mpg123_open_handle(mpg123_handle *mh, void *iohandle) {
    s_cleanup_handle = iohandle;
    return MPG123_OK;
}

int mpg123_replace_reader_handle(mpg123_handle *mh, ssize_t (*r_read) (void *, void *, size_t), off_t (*r_lseek) (void *, off_t, int), void (*cleanup) (void *)) {
    s_cleanup_callback = cleanup;
    return MPG123_OK;
}

int mpg123_getformat(mpg123_handle *mh, long *rate, int *channels, int *encoding) {
    if (rate) *rate = 44100;
    if (channels) *channels = 2;
    if (encoding) *encoding = MPG123_ENC_SIGNED_16;
    return MPG123_OK;
}
int mpg123_format_none(mpg123_handle *mh) { return MPG123_OK; }
int mpg123_format(mpg123_handle *mh, long rate, int channels, int encodings) { return MPG123_OK; }

int mpg123_close(mpg123_handle *mh) {
    if (s_cleanup_callback && s_cleanup_handle) {
        s_cleanup_callback(s_cleanup_handle);
    }
    return MPG123_OK;
}

off_t mpg123_length(mpg123_handle *mh) { return 0; }
off_t mpg123_tell(mpg123_handle *mh) { return 0; }
int mpg123_read(mpg123_handle *mh, unsigned char *outmemory, size_t outmemsize, size_t *done) {
    if (done) *done = 0;
    return MPG123_DONE; // EOF
}
off_t mpg123_seek(mpg123_handle *mh, off_t sampleoff, int whence) { return 0; }
const char* mpg123_plain_strerror(int errcode) { return "Mock Error"; }


// Test Mock IOHandler
class MockIOHandler : public IOHandler {
public:
    bool* closed_flag;

    MockIOHandler(bool* flag) : closed_flag(flag) {}

    size_t read(void* buffer, size_t size, size_t count) override { return 0; }
    int seek(off_t offset, int whence) override { return 0; }
    off_t tell() override { return 0; }
    int close() override {
        if (closed_flag) *closed_flag = true;
        return 0;
    }
};

int main() {
    std::cout << "Running MP3Codec Test..." << std::endl;

    // Reset global state
    s_cleanup_callback = nullptr;
    s_cleanup_handle = nullptr;

    bool was_closed = false;

    try {
        auto mock_handler = std::make_unique<MockIOHandler>(&was_closed);

        // Scope for Libmpg123 to trigger destructor
        {
            PsyMP3::Codec::MP3::Libmpg123 mp3(std::move(mock_handler));

            // Verify initialization
            if (!s_cleanup_callback) {
                std::cerr << "FAIL: cleanup callback not registered" << std::endl;
                return 1;
            }
            if (s_cleanup_handle == nullptr) {
                std::cerr << "FAIL: cleanup handle not set" << std::endl;
                return 1;
            }
        }
        // Destructor called here -> mpg123_close -> cleanup_callback -> IOHandler::close

        if (was_closed) {
            std::cout << "SUCCESS: IOHandler::close() was called." << std::endl;
        } else {
            std::cerr << "FAIL: IOHandler::close() was NOT called." << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception during test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Running Constructor Error Test..." << std::endl;
    s_mpg123_new_fail = true;
    try {
        auto mock_handler = std::make_unique<MockIOHandler>(nullptr);
        PsyMP3::Codec::MP3::Libmpg123 mp3(std::move(mock_handler));
        std::cerr << "FAIL: Constructor should have thrown exception" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "SUCCESS: Caught expected exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "FAIL: Caught unexpected exception type" << std::endl;
        return 1;
    }

    return 0;
}
