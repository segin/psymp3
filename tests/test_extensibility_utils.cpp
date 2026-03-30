/*
 * test_extensibility_utils.cpp - Tests for Demuxer ExtensibilityUtils
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <iostream>
#include <cassert>

void test_isValidURI() {
    std::cout << "Testing ExtensibilityUtils::isValidURI..." << std::endl;

    // Valid URIs with protocols
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("http://example.com/file.mp3"));
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("https://example.com"));
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("file:///path/to/file.mp3"));
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("custom-scheme://test"));

    // Local file paths (assumed valid if no protocol format `://` is present)
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("/path/to/local/file.mp3"));
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("local_file.mp3"));
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("example.com"));
    assert(PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("scheme:path"));

    // Invalid URIs
    assert(!PsyMP3::Demuxer::ExtensibilityUtils::isValidURI(""));        // Empty string
    assert(!PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("http://")); // Missing path (protocol_pos + 3 >= uri.length())
    assert(!PsyMP3::Demuxer::ExtensibilityUtils::isValidURI("://"));     // Empty protocol string

    std::cout << "ExtensibilityUtils::isValidURI tests passed!" << std::endl;
}

int main() {
    test_isValidURI();
    std::cout << "All ExtensibilityUtils tests passed!" << std::endl;
    return 0;
}
