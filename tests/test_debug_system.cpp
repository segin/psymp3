/*
 * test_debug_system.cpp - Test the enhanced debug system
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

void testBasicLogging() {
    // Basic logging without location info
    Debug::log("test", "Basic log message");
    Debug::log("test", "Message with value: ", 42);
}

void testLocationLogging() {
    // Logging with function and line number using the macro
    DEBUG_LOG("test", "This message includes function and line");
    DEBUG_LOG("test", "Value: ", 123, ", String: ", "hello");
}

void testSubChannels() {
    // Sub-channel examples
    Debug::log("test:init", "Initialization message");
    Debug::log("test:process", "Processing message");
    Debug::log("test:cleanup", "Cleanup message");
    
    // With location info
    DEBUG_LOG("test:init", "Init with location");
    DEBUG_LOG("test:process", "Process with location");
}

void testChannelFiltering() {
    // These will only show if their channels are enabled
    Debug::log("audio", "Audio system message");
    Debug::log("audio:buffer", "Audio buffer message");
    Debug::log("audio:playback", "Audio playback message");
    
    Debug::log("flac", "FLAC general message");
    Debug::log("flac:frame", "FLAC frame parsing");
    Debug::log("flac:metadata", "FLAC metadata parsing");
}

int main(int argc, char* argv[]) {
    std::cout << "=== Debug System Test ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Test 1: Enable 'test' channel (should see all test messages)" << std::endl;
    Debug::init("", {"test"});
    testBasicLogging();
    testLocationLogging();
    testSubChannels();
    Debug::shutdown();
    std::cout << std::endl;
    
    std::cout << "Test 2: Enable only 'test:process' sub-channel" << std::endl;
    Debug::init("", {"test:process"});
    testSubChannels();
    Debug::shutdown();
    std::cout << std::endl;
    
    std::cout << "Test 3: Enable 'audio' parent channel (should see all audio sub-channels)" << std::endl;
    Debug::init("", {"audio"});
    testChannelFiltering();
    Debug::shutdown();
    std::cout << std::endl;
    
    std::cout << "Test 4: Enable only 'audio:buffer' sub-channel" << std::endl;
    Debug::init("", {"audio:buffer"});
    testChannelFiltering();
    Debug::shutdown();
    std::cout << std::endl;
    
    std::cout << "Test 5: Enable 'all' channel" << std::endl;
    Debug::init("", {"all"});
    testBasicLogging();
    testLocationLogging();
    testSubChannels();
    testChannelFiltering();
    Debug::shutdown();
    std::cout << std::endl;
    
    std::cout << "=== All tests complete ===" << std::endl;
    return 0;
}
