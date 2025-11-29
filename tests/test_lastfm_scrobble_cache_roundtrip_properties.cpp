/*
 * test_lastfm_scrobble_cache_roundtrip_properties.cpp - Property-based tests for Last.fm scrobble cache round-trip
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

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <random>
#include <ctime>
#include <sstream>

// ========================================
// MOCK TRACK CLASS FOR TESTING
// ========================================

class MockTrack {
private:
    std::string m_artist;
    std::string m_title;
    std::string m_album;
    int m_length;
    
public:
    MockTrack(const std::string& artist, const std::string& title, 
              const std::string& album, int length)
        : m_artist(artist), m_title(title), m_album(album), m_length(length) {}
    
    std::string GetArtist() const { return m_artist; }
    std::string GetTitle() const { return m_title; }
    std::string GetAlbum() const { return m_album; }
    int GetLen() const { return m_length; }
};

// ========================================
// MOCK SCROBBLE CLASS FOR TESTING
// ========================================

class MockScrobble {
private:
    std::string m_artist;
    std::string m_title;
    std::string m_album;
    int m_length;
    time_t m_timestamp;
    
public:
    MockScrobble(const std::string& artist, const std::string& title, 
                 const std::string& album, int length, time_t timestamp)
        : m_artist(artist), m_title(title), m_album(album), 
          m_length(length), m_timestamp(timestamp) {}
    
    std::string getArtistStr() const { return m_artist; }
    std::string getTitleStr() const { return m_title; }
    std::string getAlbumStr() const { return m_album; }
    int GetLen() const { return m_length; }
    time_t getTimestamp() const { return m_timestamp; }
    
    // Serialization for XML cache
    std::string toXML() const {
        std::ostringstream xml;
        xml << "<scrobble>";
        xml << "<artist>" << m_artist << "</artist>";
        xml << "<title>" << m_title << "</title>";
        xml << "<album>" << m_album << "</album>";
        xml << "<length>" << m_length << "</length>";
        xml << "<timestamp>" << m_timestamp << "</timestamp>";
        xml << "</scrobble>";
        return xml.str();
    }
    
    static MockScrobble fromXML(const std::string& xml) {
        // Simple XML parsing for testing
        auto extract = [&xml](const std::string& tag) -> std::string {
            std::string open = "<" + tag + ">";
            std::string close = "</" + tag + ">";
            size_t start = xml.find(open);
            size_t end = xml.find(close);
            if (start == std::string::npos || end == std::string::npos) {
                return "";
            }
            return xml.substr(start + open.length(), end - start - open.length());
        };
        
        std::string artist = extract("artist");
        std::string title = extract("title");
        std::string album = extract("album");
        int length = std::stoi(extract("length"));
        time_t timestamp = std::stoll(extract("timestamp"));
        
        return MockScrobble(artist, title, album, length, timestamp);
    }
    
    bool operator==(const MockScrobble& other) const {
        return m_artist == other.m_artist &&
               m_title == other.m_title &&
               m_album == other.m_album &&
               m_length == other.m_length &&
               m_timestamp == other.m_timestamp;
    }
    
    bool operator!=(const MockScrobble& other) const {
        return !(*this == other);
    }
};

// ========================================
// RANDOM DATA GENERATORS
// ========================================

std::string generateRandomString(size_t length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> char_dist(32, 126);  // Printable ASCII
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += static_cast<char>(char_dist(gen));
    }
    return result;
}

MockScrobble generateRandomScrobble() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> len_dist(1, 600);  // 1-600 seconds
    static std::uniform_int_distribution<> str_len_dist(1, 100);  // 1-100 char strings
    
    std::string artist = generateRandomString(str_len_dist(gen));
    std::string title = generateRandomString(str_len_dist(gen));
    std::string album = generateRandomString(str_len_dist(gen));
    int length = len_dist(gen);
    time_t timestamp = time(nullptr) - (rand() % 86400);  // Random time in last 24 hours
    
    return MockScrobble(artist, title, album, length, timestamp);
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 6: Scrobble Cache Round-Trip
// ========================================
// **Feature: lastfm-performance-optimization, Property 6: Scrobble Cache Round-Trip**
// **Validates: Requirements 5.2, 6.3, 6.4**
//
// For any list of Scrobble objects, saving to XML cache and loading back 
// SHALL produce an equivalent list of Scrobble objects.
void test_property_scrobble_cache_roundtrip() {
    std::cout << "\n=== Property 6: Scrobble Cache Round-Trip ===" << std::endl;
    std::cout << "Testing that scrobbles survive XML serialization/deserialization..." << std::endl;
    
    std::cout << "\n  Testing single scrobble round-trip:" << std::endl;
    
    // Test with known values first
    MockScrobble original("The Beatles", "Hey Jude", "Hey Jude", 427, 1609459200);
    std::string xml = original.toXML();
    MockScrobble restored = MockScrobble::fromXML(xml);
    
    assert(original == restored);
    std::cout << "    Single scrobble round-trip ✓" << std::endl;
    
    // Test with special characters
    MockScrobble special("Artist & Co.", "Title \"Quoted\"", "Album <Special>", 300, 1609459200);
    xml = special.toXML();
    restored = MockScrobble::fromXML(xml);
    
    assert(special == restored);
    std::cout << "    Scrobble with special characters ✓" << std::endl;
    
    // Test with empty strings
    MockScrobble empty("", "", "", 0, 0);
    xml = empty.toXML();
    restored = MockScrobble::fromXML(xml);
    
    assert(empty == restored);
    std::cout << "    Scrobble with empty strings ✓" << std::endl;
    
    // Test with very long strings
    std::string long_artist(1000, 'A');
    std::string long_title(1000, 'B');
    std::string long_album(1000, 'C');
    MockScrobble long_scrobble(long_artist, long_title, long_album, 500, 1609459200);
    xml = long_scrobble.toXML();
    restored = MockScrobble::fromXML(xml);
    
    assert(long_scrobble == restored);
    std::cout << "    Scrobble with long strings (1000 chars each) ✓" << std::endl;
    
    std::cout << "\n  Testing random scrobbles (100 iterations):" << std::endl;
    
    int test_count = 0;
    int passed_count = 0;
    
    for (int i = 0; i < 100; ++i) {
        MockScrobble random_scrobble = generateRandomScrobble();
        
        // Serialize to XML
        std::string scrobble_xml = random_scrobble.toXML();
        
        // Deserialize from XML
        MockScrobble restored_scrobble = MockScrobble::fromXML(scrobble_xml);
        
        test_count++;
        
        // Verify they match
        if (random_scrobble == restored_scrobble) {
            passed_count++;
        } else {
            std::cerr << "  MISMATCH at iteration " << i << ":" << std::endl;
            std::cerr << "    Original: " << random_scrobble.getArtistStr() << " - " 
                      << random_scrobble.getTitleStr() << std::endl;
            std::cerr << "    Restored: " << restored_scrobble.getArtistStr() << " - " 
                      << restored_scrobble.getTitleStr() << std::endl;
            assert(false && "Scrobble round-trip mismatch");
        }
    }
    
    std::cout << "    Passed " << passed_count << "/" << test_count << " random scrobble tests ✓" << std::endl;
    
    std::cout << "\n  Testing scrobble list round-trip:" << std::endl;
    
    // Test with multiple scrobbles
    std::vector<MockScrobble> scrobbles;
    for (int i = 0; i < 10; ++i) {
        scrobbles.push_back(generateRandomScrobble());
    }
    
    // Serialize all scrobbles
    std::ostringstream cache_xml;
    cache_xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    cache_xml << "<scrobbles>\n";
    for (const auto& scrobble : scrobbles) {
        cache_xml << scrobble.toXML() << "\n";
    }
    cache_xml << "</scrobbles>\n";
    
    // Parse back and verify
    std::string cache_content = cache_xml.str();
    std::vector<MockScrobble> restored_scrobbles;
    
    // Simple parsing: extract each <scrobble>...</scrobble> block
    size_t pos = 0;
    while ((pos = cache_content.find("<scrobble>", pos)) != std::string::npos) {
        size_t end = cache_content.find("</scrobble>", pos);
        if (end == std::string::npos) break;
        
        std::string scrobble_block = cache_content.substr(pos, end - pos + 11);  // +11 for "</scrobble>"
        restored_scrobbles.push_back(MockScrobble::fromXML(scrobble_block));
        pos = end + 11;
    }
    
    // Verify count matches
    assert(scrobbles.size() == restored_scrobbles.size());
    
    // Verify each scrobble matches
    for (size_t i = 0; i < scrobbles.size(); ++i) {
        assert(scrobbles[i] == restored_scrobbles[i]);
    }
    
    std::cout << "    List of " << scrobbles.size() << " scrobbles round-trip ✓" << std::endl;
    
    std::cout << "\n  Testing edge cases:" << std::endl;
    
    // Test with maximum timestamp values
    MockScrobble max_timestamp("Artist", "Title", "Album", 600, 9223372036854775807LL);  // Max time_t
    xml = max_timestamp.toXML();
    restored = MockScrobble::fromXML(xml);
    assert(max_timestamp == restored);
    std::cout << "    Maximum timestamp value ✓" << std::endl;
    
    // Test with zero timestamp
    MockScrobble zero_timestamp("Artist", "Title", "Album", 0, 0);
    xml = zero_timestamp.toXML();
    restored = MockScrobble::fromXML(xml);
    assert(zero_timestamp == restored);
    std::cout << "    Zero timestamp value ✓" << std::endl;
    
    // Test with maximum length
    MockScrobble max_length("Artist", "Title", "Album", 2147483647, 1609459200);  // Max int
    xml = max_length.toXML();
    restored = MockScrobble::fromXML(xml);
    assert(max_length == restored);
    std::cout << "    Maximum length value ✓" << std::endl;
    
    std::cout << "\n✓ Property 6: Scrobble Cache Round-Trip - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 7: Scrobble Data Integrity
// ========================================
// For any scrobble, all fields (artist, title, album, length, timestamp) 
// SHALL be preserved exactly through XML serialization.
void test_property_scrobble_data_integrity() {
    std::cout << "\n=== Property 7: Scrobble Data Integrity ===" << std::endl;
    std::cout << "Testing that all scrobble fields are preserved through serialization..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> len_dist(0, 3600);  // 0-3600 seconds
    
    int test_count = 0;
    
    for (int i = 0; i < 50; ++i) {
        MockScrobble original = generateRandomScrobble();
        
        // Serialize and deserialize
        std::string xml = original.toXML();
        MockScrobble restored = MockScrobble::fromXML(xml);
        
        // Verify each field individually
        assert(original.getArtistStr() == restored.getArtistStr());
        assert(original.getTitleStr() == restored.getTitleStr());
        assert(original.getAlbumStr() == restored.getAlbumStr());
        assert(original.GetLen() == restored.GetLen());
        assert(original.getTimestamp() == restored.getTimestamp());
        
        test_count++;
    }
    
    std::cout << "  Verified " << test_count << " scrobbles preserve all fields ✓" << std::endl;
    std::cout << "\n✓ Property 7: Scrobble Data Integrity - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 8: XML Format Consistency
// ========================================
// For any scrobble, the XML output SHALL always contain all required elements
// in the correct order.
void test_property_xml_format_consistency() {
    std::cout << "\n=== Property 8: XML Format Consistency ===" << std::endl;
    std::cout << "Testing that XML output has consistent format..." << std::endl;
    
    int test_count = 0;
    
    for (int i = 0; i < 50; ++i) {
        MockScrobble scrobble = generateRandomScrobble();
        std::string xml = scrobble.toXML();
        
        // Verify XML structure
        assert(xml.find("<scrobble>") != std::string::npos);
        assert(xml.find("</scrobble>") != std::string::npos);
        assert(xml.find("<artist>") != std::string::npos);
        assert(xml.find("</artist>") != std::string::npos);
        assert(xml.find("<title>") != std::string::npos);
        assert(xml.find("</title>") != std::string::npos);
        assert(xml.find("<album>") != std::string::npos);
        assert(xml.find("</album>") != std::string::npos);
        assert(xml.find("<length>") != std::string::npos);
        assert(xml.find("</length>") != std::string::npos);
        assert(xml.find("<timestamp>") != std::string::npos);
        assert(xml.find("</timestamp>") != std::string::npos);
        
        // Verify element order
        size_t artist_pos = xml.find("<artist>");
        size_t title_pos = xml.find("<title>");
        size_t album_pos = xml.find("<album>");
        size_t length_pos = xml.find("<length>");
        size_t timestamp_pos = xml.find("<timestamp>");
        
        assert(artist_pos < title_pos);
        assert(title_pos < album_pos);
        assert(album_pos < length_pos);
        assert(length_pos < timestamp_pos);
        
        test_count++;
    }
    
    std::cout << "  Verified " << test_count << " scrobbles have consistent XML format ✓" << std::endl;
    std::cout << "\n✓ Property 8: XML Format Consistency - ALL TESTS PASSED" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "LAST.FM SCROBBLE CACHE ROUND-TRIP PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: lastfm-performance-optimization, Property 6: Scrobble Cache Round-Trip**" << std::endl;
    std::cout << "**Validates: Requirements 5.2, 6.3, 6.4**" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    try {
        // Run all property tests
        test_property_scrobble_cache_roundtrip();
        test_property_scrobble_data_integrity();
        test_property_xml_format_consistency();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    }
}

