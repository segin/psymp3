#include "psymp3.h"
#include <cassert>
#include <iostream>

#ifdef HAVE_DBUS

using namespace PsyMP3::MPRIS;

// Test helper macros
#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "ASSERTION FAILED: " << #expected << " != " << #actual \
                      << " (expected: " << (expected) << ", actual: " << (actual) << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << #condition << " is false" << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            std::cerr << "ASSERTION FAILED: " << #condition << " is true" << std::endl; \
            return false; \
        } \
    } while(0)

// Test DBusVariant construction and type safety
bool test_dbus_variant_construction() {
    std::cout << "Testing DBusVariant construction..." << std::endl;
    
    // Test string variant
    DBusVariant str_var{std::string("test string")};
    ASSERT_EQ(static_cast<int>(DBusVariant::String), static_cast<int>(str_var.type));
    ASSERT_EQ(std::string("test string"), str_var.get<std::string>());
    
    // Test string array variant
    std::vector<std::string> arr = {"item1", "item2", "item3"};
    DBusVariant arr_var{arr};
    ASSERT_EQ(DBusVariant::StringArray, arr_var.type);
    auto retrieved_arr = arr_var.get<std::vector<std::string>>();
    ASSERT_EQ(3u, retrieved_arr.size());
    ASSERT_EQ(std::string("item1"), retrieved_arr[0]);
    ASSERT_EQ(std::string("item2"), retrieved_arr[1]);
    ASSERT_EQ(std::string("item3"), retrieved_arr[2]);
    
    // Test int64 variant
    DBusVariant int64_var{static_cast<int64_t>(-12345)};
    ASSERT_EQ(DBusVariant::Int64, int64_var.type);
    ASSERT_EQ(static_cast<int64_t>(-12345), int64_var.get<int64_t>());
    
    // Test uint64 variant
    DBusVariant uint64_var{static_cast<uint64_t>(98765)};
    ASSERT_EQ(DBusVariant::UInt64, uint64_var.type);
    ASSERT_EQ(static_cast<uint64_t>(98765), uint64_var.get<uint64_t>());
    
    // Test double variant
    DBusVariant double_var{3.14159};
    ASSERT_EQ(DBusVariant::Double, double_var.type);
    ASSERT_EQ(3.14159, double_var.get<double>());
    
    // Test boolean variant
    DBusVariant bool_var{true};
    ASSERT_EQ(DBusVariant::Boolean, bool_var.type);
    ASSERT_EQ(true, bool_var.get<bool>());
    
    return true;
}

// Test DBusVariant toString functionality
bool test_dbus_variant_to_string() {
    std::cout << "Testing DBusVariant toString..." << std::endl;
    
    DBusVariant str_var{std::string("hello")};
    ASSERT_EQ(std::string("\"hello\""), str_var.toString());
    
    std::vector<std::string> arr = {"a", "b", "c"};
    DBusVariant arr_var{arr};
    ASSERT_EQ(std::string("[\"a\", \"b\", \"c\"]"), arr_var.toString());
    
    DBusVariant int_var{static_cast<int64_t>(42)};
    ASSERT_EQ(std::string("42"), int_var.toString());
    
    DBusVariant uint_var{static_cast<uint64_t>(123)};
    ASSERT_EQ(std::string("123"), uint_var.toString());
    
    DBusVariant double_var{2.5};
    ASSERT_TRUE(double_var.toString().find("2.5") != std::string::npos);
    
    DBusVariant bool_true{true};
    ASSERT_EQ(std::string("true"), bool_true.toString());
    
    DBusVariant bool_false{false};
    ASSERT_EQ(std::string("false"), bool_false.toString());
    
    return true;
}

// Test MPRISMetadata functionality
bool test_mpris_metadata() {
    std::cout << "Testing MPRISMetadata..." << std::endl;
    
    MPRISMetadata metadata;
    
    // Test empty metadata
    ASSERT_TRUE(metadata.isEmpty());
    
    // Test setting metadata
    metadata.artist = "Test Artist";
    metadata.title = "Test Title";
    metadata.album = "Test Album";
    metadata.track_id = "/test/track/1";
    metadata.length_us = 180000000; // 3 minutes in microseconds
    metadata.art_url = "file:///path/to/art.jpg";
    
    ASSERT_FALSE(metadata.isEmpty());
    
    // Test conversion to D-Bus dictionary
    auto dict = metadata.toDBusDict();
    
    ASSERT_EQ(6u, dict.size());
    ASSERT_EQ(std::string("Test Artist"), dict["xesam:artist"].get<std::vector<std::string>>()[0]);
    ASSERT_EQ(std::string("Test Title"), dict["xesam:title"].get<std::string>());
    ASSERT_EQ(std::string("Test Album"), dict["xesam:album"].get<std::string>());
    ASSERT_EQ(std::string("/test/track/1"), dict["mpris:trackid"].get<std::string>());
    ASSERT_EQ(static_cast<int64_t>(180000000), dict["mpris:length"].get<int64_t>());
    ASSERT_EQ(std::string("file:///path/to/art.jpg"), dict["mpris:artUrl"].get<std::string>());
    
    // Test clearing metadata
    metadata.clear();
    ASSERT_TRUE(metadata.isEmpty());
    ASSERT_EQ(0u, metadata.toDBusDict().size());
    
    return true;
}

// Test playback status conversions
bool test_playback_status_conversions() {
    std::cout << "Testing playback status conversions..." << std::endl;
    
    // Test enum to string
    ASSERT_EQ(std::string("Playing"), playbackStatusToString(PlaybackStatus::Playing));
    ASSERT_EQ(std::string("Paused"), playbackStatusToString(PlaybackStatus::Paused));
    ASSERT_EQ(std::string("Stopped"), playbackStatusToString(PlaybackStatus::Stopped));
    
    // Test string to enum (compare as strings)
    ASSERT_EQ(std::string("Playing"), playbackStatusToString(stringToPlaybackStatus("Playing")));
    ASSERT_EQ(std::string("Paused"), playbackStatusToString(stringToPlaybackStatus("Paused")));
    ASSERT_EQ(std::string("Stopped"), playbackStatusToString(stringToPlaybackStatus("Stopped")));
    
    // Test invalid string (should default to Stopped)
    ASSERT_EQ(std::string("Stopped"), playbackStatusToString(stringToPlaybackStatus("Invalid")));
    ASSERT_EQ(std::string("Stopped"), playbackStatusToString(stringToPlaybackStatus("")));
    
    return true;
}

// Test loop status conversions
bool test_loop_status_conversions() {
    std::cout << "Testing loop status conversions..." << std::endl;
    
    // Test enum to string
    ASSERT_EQ(std::string("None"), loopStatusToString(LoopStatus::None));
    ASSERT_EQ(std::string("Track"), loopStatusToString(LoopStatus::Track));
    ASSERT_EQ(std::string("Playlist"), loopStatusToString(LoopStatus::Playlist));
    
    // Test string to enum (compare as strings)
    ASSERT_EQ(std::string("None"), loopStatusToString(stringToLoopStatus("None")));
    ASSERT_EQ(std::string("Track"), loopStatusToString(stringToLoopStatus("Track")));
    ASSERT_EQ(std::string("Playlist"), loopStatusToString(stringToLoopStatus("Playlist")));
    
    // Test invalid string (should default to None)
    ASSERT_EQ(std::string("None"), loopStatusToString(stringToLoopStatus("Invalid")));
    ASSERT_EQ(std::string("None"), loopStatusToString(stringToLoopStatus("")));
    
    return true;
}

// Test Result<T> template class
bool test_result_template() {
    std::cout << "Testing Result<T> template..." << std::endl;
    
    // Test successful result
    auto success_result = Result<int>::success(42);
    ASSERT_TRUE(success_result.isSuccess());
    ASSERT_FALSE(success_result.isError());
    ASSERT_EQ(42, success_result.getValue());
    ASSERT_TRUE(static_cast<bool>(success_result));
    
    // Test error result
    auto error_result = Result<int>::error("Test error message");
    ASSERT_FALSE(error_result.isSuccess());
    ASSERT_TRUE(error_result.isError());
    ASSERT_EQ(std::string("Test error message"), error_result.getError());
    ASSERT_FALSE(static_cast<bool>(error_result));
    
    // Test exception on getValue() for error result
    bool exception_thrown = false;
    try {
        error_result.getValue();
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
        ASSERT_TRUE(std::string(e.what()).find("Test error message") != std::string::npos);
    }
    ASSERT_TRUE(exception_thrown);
    
    // Test moveValue()
    auto move_result = Result<std::string>::success("movable string");
    std::string moved = move_result.moveValue();
    ASSERT_EQ(std::string("movable string"), moved);
    
    return true;
}

// Test Result<void> specialization
bool test_result_void() {
    std::cout << "Testing Result<void> specialization..." << std::endl;
    
    // Test successful void result
    auto success_result = Result<void>::success();
    ASSERT_TRUE(success_result.isSuccess());
    ASSERT_FALSE(success_result.isError());
    ASSERT_TRUE(static_cast<bool>(success_result));
    
    // Test error void result
    auto error_result = Result<void>::error("Void error message");
    ASSERT_FALSE(error_result.isSuccess());
    ASSERT_TRUE(error_result.isError());
    ASSERT_EQ(std::string("Void error message"), error_result.getError());
    ASSERT_FALSE(static_cast<bool>(error_result));
    
    return true;
}

// Test RAII deleters (basic functionality - actual D-Bus testing would require mock)
bool test_raii_deleters() {
    std::cout << "Testing RAII deleters..." << std::endl;
    
    // Test that deleters can be instantiated and called with nullptr
    DBusConnectionDeleter conn_deleter;
    conn_deleter(nullptr); // Should not crash
    
    DBusMessageDeleter msg_deleter;
    msg_deleter(nullptr); // Should not crash
    
    // Test that unique_ptr can be created with custom deleters
    DBusConnectionPtr conn_ptr(nullptr);
    DBusMessagePtr msg_ptr(nullptr);
    
    // These should compile and not crash when destructed
    ASSERT_TRUE(true); // If we get here, the test passed
    
    return true;
}

// Test metadata with partial data
bool test_metadata_partial_data() {
    std::cout << "Testing metadata with partial data..." << std::endl;
    
    MPRISMetadata metadata;
    
    // Set only some fields
    metadata.artist = "Partial Artist";
    metadata.length_us = 120000000; // 2 minutes
    
    ASSERT_FALSE(metadata.isEmpty());
    
    auto dict = metadata.toDBusDict();
    ASSERT_EQ(2u, dict.size());
    ASSERT_TRUE(dict.find("xesam:artist") != dict.end());
    ASSERT_TRUE(dict.find("mpris:length") != dict.end());
    ASSERT_TRUE(dict.find("xesam:title") == dict.end());
    ASSERT_TRUE(dict.find("xesam:album") == dict.end());
    
    return true;
}

// Main test runner
int main() {
    std::cout << "Running MPRIS Types unit tests..." << std::endl;
    
    bool all_passed = true;
    
    all_passed &= test_dbus_variant_construction();
    all_passed &= test_dbus_variant_to_string();
    all_passed &= test_mpris_metadata();
    all_passed &= test_playback_status_conversions();
    all_passed &= test_loop_status_conversions();
    all_passed &= test_result_template();
    all_passed &= test_result_void();
    all_passed &= test_raii_deleters();
    all_passed &= test_metadata_partial_data();
    all_passed &= test_dbus_variant_dictionary();
    
    if (all_passed) {
        std::cout << "All MPRIS Types tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some MPRIS Types tests FAILED!" << std::endl;
        return 1;
    }
}

// Test DBusVariant with dictionary (nested)
bool test_dbus_variant_dictionary() {
    std::cout << "Testing DBusVariant dictionary..." << std::endl;

    DBusDictionary dict;
    dict["title"] = DBusVariant(std::string("Test Title"));
    dict["artist"] = DBusVariant(std::vector<std::string>{"Artist 1", "Artist 2"});
    dict["year"] = DBusVariant(static_cast<int64_t>(2025));

    DBusVariant var{dict};
    ASSERT_EQ(DBusVariant::Dictionary, var.type);

    auto retrieved_ptr = var.get<std::shared_ptr<DBusDictionary>>();
    ASSERT_TRUE(retrieved_ptr != nullptr);
    ASSERT_EQ(3u, retrieved_ptr->size());
    ASSERT_EQ(std::string("Test Title"), (*retrieved_ptr)["title"].get<std::string>());

    // Test toString
    std::string s = var.toString();
    ASSERT_TRUE(s.find("\"title\": \"Test Title\"") != std::string::npos);
    ASSERT_TRUE(s.find("\"year\": 2025") != std::string::npos);

    // Test nesting
    DBusDictionary outer;
    outer["Metadata"] = var;
    DBusVariant outer_var{outer};

    std::string s_outer = outer_var.toString();
    ASSERT_TRUE(s_outer.find("\"Metadata\": {") != std::string::npos);
    ASSERT_TRUE(s_outer.find("\"title\": \"Test Title\"") != std::string::npos);

    return true;
}

#else // !HAVE_DBUS

int main() {
    std::cout << "MPRIS Types tests skipped - D-Bus support not available" << std::endl;
    return 0;
}

#endif // HAVE_DBUS