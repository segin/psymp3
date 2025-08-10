/*
 * test_alaw_table_verification.cpp - Verify A-law lookup table values
 * This file is part of PsyMP3.
 */

#include "psymp3.h"

#ifdef ENABLE_ALAW_CODEC

int main() {
    // Create a StreamInfo for A-law codec
    StreamInfo stream_info;
    stream_info.codec_name = "alaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    // Create ALawCodec instance
    ALawCodec codec(stream_info);
    
    // Initialize the codec
    if (!codec.initialize()) {
        Debug::log("ERROR: Failed to initialize A-law codec");
        return 1;
    }
    
    // Test A-law silence value (0x55) maps to 0
    MediaChunk silence_chunk;
    silence_chunk.data = {0x55};
    
    AudioFrame silence_frame = codec.decode(silence_chunk);
    if (silence_frame.samples.empty()) {
        Debug::log("ERROR: No output from A-law codec for silence value");
        return 1;
    }
    
    const int16_t silence_value = silence_frame.samples[0];
    Debug::log("A-law silence value (0x55) maps to: %d", silence_value);
    
    if (silence_value != 0) {
        Debug::log("ERROR: A-law silence value should map to 0, got %d", silence_value);
        return 1;
    }
    
    // Test sign bit handling - test a few key values
    uint8_t test_values[] = {0x00, 0x7F, 0x80, 0xFF, 0x54, 0x56};
    const char* test_names[] = {"0x00", "0x7F", "0x80", "0xFF", "0x54", "0x56"};
    
    for (int i = 0; i < 6; ++i) {
        MediaChunk test_chunk;
        test_chunk.data = {test_values[i]};
        
        AudioFrame test_frame = codec.decode(test_chunk);
        if (test_frame.samples.empty()) {
            Debug::log("ERROR: No output from A-law codec for value %s", test_names[i]);
            return 1;
        }
        
        int16_t value = test_frame.samples[0];
        Debug::log("A-law %s maps to: %d", test_names[i], value);
    }
    
    // Verify sign bit handling for all values
    bool sign_test_passed = true;
    
    // Test all values 0x00-0x7F should be negative
    for (int i = 0x00; i <= 0x7F; ++i) {
        MediaChunk test_chunk;
        test_chunk.data = {static_cast<uint8_t>(i)};
        
        AudioFrame test_frame = codec.decode(test_chunk);
        if (test_frame.samples.empty()) {
            Debug::log("ERROR: No output from A-law codec for value 0x%02x", i);
            return 1;
        }
        
        if (test_frame.samples[0] >= 0) {
            Debug::log("ERROR: A-law value 0x%02x should be negative, got %d", i, test_frame.samples[0]);
            sign_test_passed = false;
            break;
        }
    }
    
    // Test all values 0x80-0xFF should be positive
    for (int i = 0x80; i <= 0xFF; ++i) {
        MediaChunk test_chunk;
        test_chunk.data = {static_cast<uint8_t>(i)};
        
        AudioFrame test_frame = codec.decode(test_chunk);
        if (test_frame.samples.empty()) {
            Debug::log("ERROR: No output from A-law codec for value 0x%02x", i);
            return 1;
        }
        
        if (test_frame.samples[0] <= 0) {
            Debug::log("ERROR: A-law value 0x%02x should be positive, got %d", i, test_frame.samples[0]);
            sign_test_passed = false;
            break;
        }
    }
    
    if (sign_test_passed) {
        Debug::log("PASS: All sign bit tests passed");
    } else {
        return 1;
    }
    
    Debug::log("A-law lookup table verification completed successfully");
    return 0;
}

#else
int main() {
    Debug::log("A-law codec not enabled in build");
    return 0;
}
#endif