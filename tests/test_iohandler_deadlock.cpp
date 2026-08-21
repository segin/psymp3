/*
 * test_iohandler_deadlock.cpp - Test to reproduce the IOHandler deadlock
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "flac_test_data_utils.h"
#include <iostream>

int main() {
    std::cout << "IOHandler Deadlock Reproduction Test" << std::endl;
    std::cout << "====================================" << std::endl;

    // Any readable file exercises the read/tell/seek lock ordering; use the
    // generated fixture and SKIP (77) when no test data is provisioned.
    FLACTestDataUtils::validateTestDataAvailable("IOHandler deadlock");
    const std::string test_file = FLACTestDataUtils::findAvailableTestFile();

    try {
        std::cout << "Creating FileIOHandler..." << std::endl;
        auto handler = std::make_unique<FileIOHandler>(test_file);
        std::cout << "✓ FileIOHandler created" << std::endl;
        
        std::cout << "Testing basic read..." << std::endl;
        uint8_t buffer[16];
        size_t bytes_read = handler->read(buffer, 1, 16);
        std::cout << "✓ Read " << bytes_read << " bytes" << std::endl;
        
        std::cout << "Testing tell() method..." << std::endl;
        off_t pos1 = handler->tell();
        std::cout << "✓ tell() returned: " << pos1 << std::endl;
        
        std::cout << "Testing seek() method (this should deadlock)..." << std::endl;
        int seek_result = handler->seek(0, SEEK_SET);
        std::cout << "✓ seek() returned: " << seek_result << std::endl;
        
        std::cout << "All operations completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }
}