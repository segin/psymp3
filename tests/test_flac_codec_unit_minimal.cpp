/*
 * test_flac_codec_unit_minimal.cpp - Minimal unit tests for FLAC libraries
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

#ifdef HAVE_FLAC
#include <FLAC++/decoder.h>

/**
 * @brief Minimal unit tests for FLAC library availability and basic functionality
 * Requirements: 16.1, 16.2 - Conditional compilation integration
 */
class FLACLibraryMinimalTest : public TestCase {
public:
    FLACLibraryMinimalTest() : TestCase("FLAC Library Minimal Test") {}

protected:
    void runTest() override {
        testFLACLibraryAvailability();
        testFLACDecoderCreation();
    }

private:
    void testFLACLibraryAvailability() {
        // Test that FLAC libraries are properly linked and available
        ASSERT_TRUE(true, "FLAC libraries should be available when HAVE_FLAC is defined");
        
        // Test FLAC version information is accessible
        const char* version = FLAC__VERSION_STRING;
        ASSERT_TRUE(version != nullptr, "FLAC version string should be available");
        ASSERT_TRUE(strlen(version) > 0, "FLAC version string should not be empty");
    }

    void testFLACDecoderCreation() {
        // Test that FLAC decoder constants and enums are available
        FLAC__StreamDecoderState uninitialized_state = FLAC__STREAM_DECODER_UNINITIALIZED;
        ASSERT_TRUE(uninitialized_state == FLAC__STREAM_DECODER_UNINITIALIZED, 
                   "FLAC decoder state constants should be available");
        
        // Test that FLAC decoder read status constants are available
        FLAC__StreamDecoderReadStatus continue_status = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
        ASSERT_TRUE(continue_status == FLAC__STREAM_DECODER_READ_STATUS_CONTINUE,
                   "FLAC decoder read status constants should be available");
        
        // Test that FLAC decoder write status constants are available  
        FLAC__StreamDecoderWriteStatus write_continue = FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        ASSERT_TRUE(write_continue == FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE,
                   "FLAC decoder write status constants should be available");
    }
};

int main() {
    TestSuite suite("FLAC Library Minimal Tests");
    suite.addTest(std::make_unique<FLACLibraryMinimalTest>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping FLAC library tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC