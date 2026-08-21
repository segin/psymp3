/*
 * test_method_handler_comprehensive.cpp - Comprehensive unit tests for MethodHandler
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "test_framework_threading.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstdlib>

#include <dbus/dbus.h>

using namespace TestFramework;
using namespace TestFramework::Threading;
using namespace PsyMP3::MPRIS;

namespace {

constexpr const char* kMprisPath = "/org/mpris/MediaPlayer2";
constexpr const char* kPlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr const char* kPropertiesInterface = "org.freedesktop.DBus.Properties";

/**
 * @brief RAII wrapper building a GENUINE libdbus method-call message.
 *
 * The previous incarnation of this suite reinterpret_cast plain C++ mock
 * objects to DBusMessage and DBusConnection pointers and handed them to the
 * real MethodHandler, which promptly crashed inside
 * dbus_message_get_interface. libdbus types are opaque C structs; only real
 * ones can cross that API.
 */
class ScopedMessage {
public:
    ScopedMessage(const char* interface, const char* member)
        : m_msg(dbus_message_new_method_call("org.mpris.MediaPlayer2.psymp3",
                                             kMprisPath, interface, member)) {
        // Replies reference the request serial; a never-sent client message
        // has none, so stamp one like a bus would.
        if (m_msg) dbus_message_set_serial(m_msg, 1);
    }
    ~ScopedMessage() { if (m_msg) dbus_message_unref(m_msg); }
    ScopedMessage(const ScopedMessage&) = delete;
    ScopedMessage& operator=(const ScopedMessage&) = delete;
    ScopedMessage(ScopedMessage&& other) noexcept : m_msg(other.m_msg) { other.m_msg = nullptr; }

    DBusMessage* get() const { return m_msg; }

    void appendString(const char* value) {
        dbus_message_append_args(m_msg, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);
    }
    void appendTwoStrings(const char* a, const char* b) {
        dbus_message_append_args(m_msg, DBUS_TYPE_STRING, &a,
                                 DBUS_TYPE_STRING, &b, DBUS_TYPE_INVALID);
    }
    void appendInt64(int64_t value) {
        dbus_int64_t v = value;
        dbus_message_append_args(m_msg, DBUS_TYPE_INT64, &v, DBUS_TYPE_INVALID);
    }
    void appendObjectPathAndInt64(const char* path, int64_t value) {
        dbus_int64_t v = value;
        dbus_message_append_args(m_msg, DBUS_TYPE_OBJECT_PATH, &path,
                                 DBUS_TYPE_INT64, &v, DBUS_TYPE_INVALID);
    }

    static ScopedMessage playerCall(const char* member) {
        return ScopedMessage(kPlayerInterface, member);
    }
    static ScopedMessage propertyGet(const char* interface, const char* property) {
        ScopedMessage msg(kPropertiesInterface, "Get");
        msg.appendTwoStrings(interface, property);
        return msg;
    }
    static ScopedMessage propertyGetAll(const char* interface) {
        ScopedMessage msg(kPropertiesInterface, "GetAll");
        msg.appendString(interface);
        return msg;
    }

private:
    DBusMessage* m_msg;
};

} // namespace

/**
 * @brief Test class for MethodHandler comprehensive testing
 *
 * The handler is constructed with a null Player (an explicitly supported
 * configuration: every dispatch path null-guards and sends an error reply,
 * which still counts as DBUS_HANDLER_RESULT_HANDLED). This exercises the
 * real message parsing, dispatch table, and reply generation without
 * needing a fake Player object.
 */
class MethodHandlerTest : public TestCase {
public:
    MethodHandlerTest() : TestCase("MethodHandlerTest") {}

protected:
    void setUp() override {
        m_property_manager = std::make_unique<PropertyManager>(nullptr);
        m_method_handler = std::make_unique<MethodHandler>(nullptr, m_property_manager.get());

        // Replies are sent on a real private session-bus connection; without
        // a bus this suite cannot run (main() skips before we get here).
        DBusError error;
        dbus_error_init(&error);
        m_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
        if (dbus_error_is_set(&error)) dbus_error_free(&error);
    }

    void tearDown() override {
        m_method_handler.reset();
        m_property_manager.reset();
        if (m_connection) {
            dbus_connection_close(m_connection);
            dbus_connection_unref(m_connection);
            m_connection = nullptr;
        }
    }

    void runTest() override {
        ASSERT_TRUE(m_connection != nullptr, "Session bus connection should be available");
        testBasicMethodHandling();
        testPlaybackControlMethods();
        testSeekingMethods();
        testPropertyAccessMethods();
        testMalformedMessageHandling();
        testConcurrentMethodCalls();
        testPerformanceUnderLoad();
    }

private:
    std::unique_ptr<PropertyManager> m_property_manager;
    std::unique_ptr<MethodHandler> m_method_handler;
    DBusConnection* m_connection = nullptr;

    DBusHandlerResult dispatch(const ScopedMessage& msg) {
        return m_method_handler->handleMessage(m_connection, msg.get());
    }

    void testBasicMethodHandling() {
        ASSERT_TRUE(m_method_handler != nullptr, "MethodHandler should be constructed");
        ASSERT_TRUE(m_method_handler->isReady(), "MethodHandler should be ready");

        // Null parameters must be rejected without crashing
        auto result = m_method_handler->handleMessage(nullptr, nullptr);
        ASSERT_EQUALS(DBUS_HANDLER_RESULT_NOT_YET_HANDLED, result, "Should handle null parameters gracefully");
    }

    void testPlaybackControlMethods() {
        // With a null player, each control method sends an error reply -
        // which is still HANDLED: the message was ours and was consumed.
        for (const char* member : {"Play", "Pause", "Stop", "Next", "Previous", "PlayPause"}) {
            auto msg = ScopedMessage::playerCall(member);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_HANDLED, dispatch(msg),
                          "Playback control method should be handled");
        }
    }

    void testSeekingMethods() {
        {
            auto msg = ScopedMessage::playerCall("Seek");
            msg.appendInt64(30000000);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_HANDLED, dispatch(msg), "Seek should be handled");
        }
        {
            auto msg = ScopedMessage::playerCall("SetPosition");
            msg.appendObjectPathAndInt64("/org/mpris/MediaPlayer2/Track/1", 120000000);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_HANDLED, dispatch(msg), "SetPosition should be handled");
        }
        {
            // Extreme values must not crash the argument parsing
            auto msg = ScopedMessage::playerCall("Seek");
            msg.appendInt64(INT64_MAX);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_HANDLED, dispatch(msg), "Extreme seek should be handled");
        }
        {
            auto msg = ScopedMessage::playerCall("Seek");
            msg.appendInt64(-200000000);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_HANDLED, dispatch(msg), "Negative seek should be handled");
        }
    }

    void testPropertyAccessMethods() {
        m_property_manager->updateMetadata("Test Artist", "Test Title", "Test Album");
        m_property_manager->updatePlaybackStatus(PlaybackStatus::Playing);
        m_property_manager->updatePosition(45000000);

        for (const char* property : {"PlaybackStatus", "Metadata", "Position"}) {
            auto msg = ScopedMessage::propertyGet(kPlayerInterface, property);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_HANDLED, dispatch(msg),
                          "Properties.Get should be handled");
        }

        {
            auto msg = ScopedMessage::propertyGetAll(kPlayerInterface);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_HANDLED, dispatch(msg),
                          "Properties.GetAll should be handled");
        }

        {
            // Unknown property: an error reply or a pass are both graceful
            auto msg = ScopedMessage::propertyGet(kPlayerInterface, "InvalidProperty");
            auto result = dispatch(msg);
            ASSERT_TRUE(result == DBUS_HANDLER_RESULT_HANDLED ||
                        result == DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
                        "Invalid property should be handled gracefully");
        }
    }

    void testMalformedMessageHandling() {
        {
            // Unknown interface: not ours, must not be swallowed
            ScopedMessage msg("org.example.NotMpris", "Whatever");
            auto result = dispatch(msg);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_NOT_YET_HANDLED, result,
                          "Foreign interface should not be handled");
        }
        {
            // Seek with a missing argument: parsing must fail gracefully
            auto msg = ScopedMessage::playerCall("Seek");
            auto result = dispatch(msg);
            ASSERT_TRUE(result == DBUS_HANDLER_RESULT_HANDLED ||
                        result == DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
                        "Missing arguments should be handled gracefully");
        }
        {
            // Seek with a wrong-typed argument
            auto msg = ScopedMessage::playerCall("Seek");
            msg.appendString("not a number");
            auto result = dispatch(msg);
            ASSERT_TRUE(result == DBUS_HANDLER_RESULT_HANDLED ||
                        result == DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
                        "Wrong-typed arguments should be handled gracefully");
        }
        {
            // Null message with a valid connection
            auto result = m_method_handler->handleMessage(m_connection, nullptr);
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_NOT_YET_HANDLED, result,
                          "Null message should be rejected");
        }
        {
            // Null connection with a valid message
            auto msg = ScopedMessage::playerCall("Play");
            auto result = m_method_handler->handleMessage(nullptr, msg.get());
            ASSERT_EQUALS(DBUS_HANDLER_RESULT_NOT_YET_HANDLED, result,
                          "Null connection should be rejected");
        }
    }

    void testConcurrentMethodCalls() {
        ThreadSafetyTester::Config config;
        config.num_threads = 6;
        config.operations_per_thread = 50;
        config.test_duration = std::chrono::milliseconds{3000};

        ThreadSafetyTester tester(config);

        std::atomic<size_t> method_counter{0};
        auto playback_test = [this, &method_counter]() -> bool {
            try {
                static const char* members[] = {"Play", "Pause", "Stop", "Next"};
                auto msg = ScopedMessage::playerCall(members[method_counter.fetch_add(1) % 4]);
                return dispatch(msg) == DBUS_HANDLER_RESULT_HANDLED;
            } catch (...) {
                return false;
            }
        };

        auto results = tester.runTest(playback_test, "ConcurrentPlaybackMethods");
        ASSERT_TRUE(results.successful_operations > 0, "Should have successful method calls");
        ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks");

        auto property_test = [this]() -> bool {
            try {
                auto msg = ScopedMessage::propertyGet(kPlayerInterface, "PlaybackStatus");
                return dispatch(msg) == DBUS_HANDLER_RESULT_HANDLED;
            } catch (...) {
                return false;
            }
        };

        auto property_results = tester.runTest(property_test, "ConcurrentPropertyAccess");
        ASSERT_FALSE(property_results.deadlock_detected, "Property access should not cause deadlocks");
    }

    void testPerformanceUnderLoad() {
        auto start_time = std::chrono::high_resolution_clock::now();

        const size_t num_operations = 500;
        size_t successful_operations = 0;

        for (size_t i = 0; i < num_operations; ++i) {
            DBusHandlerResult result;
            switch (i % 6) {
                case 0: result = dispatch(ScopedMessage::playerCall("Play")); break;
                case 1: result = dispatch(ScopedMessage::playerCall("Pause")); break;
                case 2: result = dispatch(ScopedMessage::playerCall("Stop")); break;
                case 3: {
                    auto msg = ScopedMessage::playerCall("Seek");
                    msg.appendInt64(static_cast<int64_t>(i) * 1000);
                    result = dispatch(msg);
                    break;
                }
                case 4: result = dispatch(ScopedMessage::propertyGet(kPlayerInterface, "PlaybackStatus")); break;
                default: result = dispatch(ScopedMessage::propertyGetAll(kPlayerInterface)); break;
            }
            if (result == DBUS_HANDLER_RESULT_HANDLED) {
                successful_operations++;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        ASSERT_TRUE(successful_operations > 0, "Should have successful method calls");

        // Performance should be reasonable (less than 10ms per operation on average)
        auto avg_time_per_op = duration.count() / num_operations;
        ASSERT_TRUE(avg_time_per_op < 10, "Method handling should be fast");
    }
};

// Test suite setup and execution
int main() {
    // The handler sends replies on a real bus connection; without a session
    // bus (bare CI container) the suite cannot run - SKIP, don't fail.
    if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
        DBusError probe;
        dbus_error_init(&probe);
        DBusConnection* conn = dbus_bus_get_private(DBUS_BUS_SESSION, &probe);
        if (!conn) {
            if (dbus_error_is_set(&probe)) dbus_error_free(&probe);
            printf("SKIP: no D-Bus session bus available\n");
            return 77;
        }
        dbus_connection_close(conn);
        dbus_connection_unref(conn);
        if (dbus_error_is_set(&probe)) dbus_error_free(&probe);
    }

    TestSuite suite("MethodHandler Comprehensive Tests");

    suite.addTest(std::make_unique<MethodHandlerTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results);
}
