#include <cassert>
#include <iostream>
#include <map>
#include <string>

#include "psymp3.h"
#include "demuxer/DemuxerExtensibility.h"

void test_formatConfigString_empty() {
    std::map<std::string, std::string> empty_map;
    std::string result = PsyMP3::Demuxer::ExtensibilityUtils::formatConfigString(empty_map);
    assert(result == "" && "Empty map should return empty string");
    std::cout << "test_formatConfigString_empty passed." << std::endl;
}

void test_formatConfigString_single() {
    std::map<std::string, std::string> single_map = {{"key1", "value1"}};
    std::string result = PsyMP3::Demuxer::ExtensibilityUtils::formatConfigString(single_map);
    assert(result == "key1=value1" && "Single pair map formatting failed");
    std::cout << "test_formatConfigString_single passed." << std::endl;
}

void test_formatConfigString_multiple() {
    std::map<std::string, std::string> multi_map = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"}
    };
    std::string result = PsyMP3::Demuxer::ExtensibilityUtils::formatConfigString(multi_map);
    // std::map is ordered by keys, so "key1", "key2", "key3" will be sorted
    assert(result == "key1=value1;key2=value2;key3=value3" && "Multiple pair map formatting failed");
    std::cout << "test_formatConfigString_multiple passed." << std::endl;
}

void test_formatConfigString_special_chars() {
    std::map<std::string, std::string> special_map = {
        {"key with spaces", "value with spaces"},
        {"key=with=equals", "value;with;semicolons"},
        {"", "empty_key"}
    };
    std::string result = PsyMP3::Demuxer::ExtensibilityUtils::formatConfigString(special_map);
    // std::map sorts by keys: "", "key with spaces", "key=with=equals"
    assert(result == "=empty_key;key with spaces=value with spaces;key=with=equals=value;with;semicolons" && "Special characters map formatting failed");
    std::cout << "test_formatConfigString_special_chars passed." << std::endl;
}

int main() {
    std::cout << "Running ExtensibilityUtils tests..." << std::endl;

    test_formatConfigString_empty();
    test_formatConfigString_single();
    test_formatConfigString_multiple();
    test_formatConfigString_special_chars();

    std::cout << "All ExtensibilityUtils tests passed successfully!" << std::endl;
    return 0;
}
