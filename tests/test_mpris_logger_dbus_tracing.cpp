/*
 * test_mpris_logger_dbus_tracing.cpp - D-Bus message tracing tests
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

#include "MPRISLogger.h"
#include "test_framework.h"
#include <dbus/dbus.h>
#include <sstream>
#include <fstream>

class MPRISLoggerDBusTracingTest : public TestFramework {
protected:
    void SetUp() override {
        TestFramework::SetUp();
        
        // Reset logger to default state
        auto& logger = MPRIS::MPRISLogger::getInstance();
        logger.setLogLevel(MPRIS::LogLevel::TRACE);
        logger.enableConsoleOutput(false);
        logger.enableDebugMode(true);
        logger.enableMessageTracing(true);
        logger.enablePerformanceMetrics(true);
        
        // Create temporary log file
        m_temp_log_file = "/tmp/mpris_dbus_trace_" + std::to_string(std::time(nullptr)) + ".log";
        logger.setLogFile(m_temp_log_file);
        
        // Initialize D-Bus for testing
        dbus_error_init(&m_error);
    }
    
    void TearDown() override {
        if (m_message) {
            dbus_message_unref(m_message);
            m_message = nullptr;
        }
        
        if (m_connection) {
            dbus_connection_unref(m_connection);
            m_connection = nullptr;
        }
        
        dbus_error_free(&m_error);
        
        // Clean up temporary log file
        std::remove(m_temp_log_file.c_str());
        TestFramework::TearDown();
    }
    
    std::string readLogFile() {
        std::ifstream file(m_temp_log_file);
        if (!file.is_open()) {
            return "";
        }
        
        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }
    
    DBusMessage* createTestMethodCall() {
        return dbus_message_new_method_call(
            "org.mpris.MediaPlayer2.psymp3",
            "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2.Player",
            "Play"
        );
    }
    
    DBusMessage* createTestMethodReturn(DBusMessage* method_call) {
        return dbus_message_new_method_return(method_call);
    }
    
    DBusMessage* createTestSignal() {
        return dbus_message_new_signal(
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged"
        );
    }
    
    DBusMessage* createTestError(DBusMessage* method_call) {
        return dbus_message_new_error(method_call, 
                                     "org.mpris.MediaPlayer2.Error.Failed",
                                     "Test error message");
    }
    
    std::string m_temp_log_file;
    DBusError m_error;
    DBusConnection* m_connection = nullptr;
    DBusMessage* m_message = nullptr;
};

TEST_F(MPRISLoggerDBusTracingTest, TraceMethodCall) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    m_message = createTestMethodCall();
    ASSERT_NE(m_message, nullptr);
    
    logger.traceDBusMessage("SEND", m_message, "Test method call");
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("SEND D-Bus message") != std::string::npos);
    EXPECT_TRUE(log_content.find("Test method call") != std::string::npos);
    EXPECT_TRUE(log_content.find("type=METHOD_CALL") != std::string::npos);
    EXPECT_TRUE(log_content.find("interface=org.mpris.MediaPlayer2.Player") != std::string::npos);
    EXPECT_TRUE(log_content.find("member=Play") != std::string::npos);
    EXPECT_TRUE(log_content.find("path=/org/mpris/MediaPlayer2") != std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, TraceMethodReturn) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    DBusMessage* method_call = createTestMethodCall();
    ASSERT_NE(method_call, nullptr);
    
    m_message = createTestMethodReturn(method_call);
    ASSERT_NE(m_message, nullptr);
    
    logger.traceDBusMessage("RECV", m_message, "Method return");
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("RECV D-Bus message") != std::string::npos);
    EXPECT_TRUE(log_content.find("Method return") != std::string::npos);
    EXPECT_TRUE(log_content.find("type=METHOD_RETURN") != std::string::npos);
    
    dbus_message_unref(method_call);
}

TEST_F(MPRISLoggerDBusTracingTest, TraceSignal) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    m_message = createTestSignal();
    ASSERT_NE(m_message, nullptr);
    
    logger.traceDBusMessage("EMIT", m_message, "Properties changed signal");
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("EMIT D-Bus message") != std::string::npos);
    EXPECT_TRUE(log_content.find("Properties changed signal") != std::string::npos);
    EXPECT_TRUE(log_content.find("type=SIGNAL") != std::string::npos);
    EXPECT_TRUE(log_content.find("interface=org.freedesktop.DBus.Properties") != std::string::npos);
    EXPECT_TRUE(log_content.find("member=PropertiesChanged") != std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, TraceError) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    DBusMessage* method_call = createTestMethodCall();
    ASSERT_NE(method_call, nullptr);
    
    m_message = createTestError(method_call);
    ASSERT_NE(m_message, nullptr);
    
    logger.traceDBusMessage("SEND", m_message, "Error response");
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("SEND D-Bus message") != std::string::npos);
    EXPECT_TRUE(log_content.find("Error response") != std::string::npos);
    EXPECT_TRUE(log_content.find("type=ERROR") != std::string::npos);
    
    dbus_message_unref(method_call);
}

TEST_F(MPRISLoggerDBusTracingTest, TraceConnectionEvents) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Simulate connection events
    logger.traceDBusConnection("established", nullptr, "Session bus connection");
    logger.traceDBusConnection("lost", nullptr, "Connection timeout");
    logger.traceDBusConnection("restored", nullptr, "Reconnection successful");
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("Connection established") != std::string::npos);
    EXPECT_TRUE(log_content.find("Session bus connection") != std::string::npos);
    EXPECT_TRUE(log_content.find("Connection lost") != std::string::npos);
    EXPECT_TRUE(log_content.find("Connection timeout") != std::string::npos);
    EXPECT_TRUE(log_content.find("Connection restored") != std::string::npos);
    EXPECT_TRUE(log_content.find("Reconnection successful") != std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, TracingDisabled) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Disable message tracing
    logger.enableMessageTracing(false);
    
    m_message = createTestMethodCall();
    ASSERT_NE(m_message, nullptr);
    
    logger.traceDBusMessage("SEND", m_message, "This should not appear");
    
    std::string log_content = readLogFile();
    
    // Message should not appear in log when tracing is disabled
    EXPECT_TRUE(log_content.find("This should not appear") == std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, TracingWithLowLogLevel) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Set log level to INFO (higher than TRACE)
    logger.setLogLevel(MPRIS::LogLevel::INFO);
    
    m_message = createTestMethodCall();
    ASSERT_NE(m_message, nullptr);
    
    logger.traceDBusMessage("SEND", m_message, "This should not appear due to log level");
    
    std::string log_content = readLogFile();
    
    // Message should not appear when log level is too high
    EXPECT_TRUE(log_content.find("This should not appear due to log level") == std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, NullMessageHandling) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Test with null message - should not crash
    logger.traceDBusMessage("SEND", nullptr, "Null message test");
    
    std::string log_content = readLogFile();
    
    // Should not contain the trace since message is null
    EXPECT_TRUE(log_content.find("Null message test") == std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, MessageWithComplexArguments) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    m_message = dbus_message_new_method_call(
        "org.mpris.MediaPlayer2.psymp3",
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        "SetPosition"
    );
    ASSERT_NE(m_message, nullptr);
    
    // Add arguments to the message
    DBusMessageIter iter;
    dbus_message_iter_init_append(m_message, &iter);
    
    const char* track_id = "/org/mpris/MediaPlayer2/Track/1";
    dbus_int64_t position = 123456789;
    
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &track_id);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &position);
    
    logger.traceDBusMessage("SEND", m_message, "SetPosition with arguments");
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("SEND D-Bus message") != std::string::npos);
    EXPECT_TRUE(log_content.find("SetPosition with arguments") != std::string::npos);
    EXPECT_TRUE(log_content.find("member=SetPosition") != std::string::npos);
    EXPECT_TRUE(log_content.find("serial=") != std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, TracingMacro) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    m_message = createTestMethodCall();
    ASSERT_NE(m_message, nullptr);
    
    // Test the tracing macro
    MPRIS_TRACE_DBUS_MESSAGE("MACRO_SEND", m_message, "Testing macro");
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("MACRO_SEND D-Bus message") != std::string::npos);
    EXPECT_TRUE(log_content.find("Testing macro") != std::string::npos);
}

TEST_F(MPRISLoggerDBusTracingTest, HighVolumeTracing) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    const int num_messages = 100;
    
    for (int i = 0; i < num_messages; ++i) {
        DBusMessage* msg = createTestMethodCall();
        ASSERT_NE(msg, nullptr);
        
        logger.traceDBusMessage("BULK", msg, "Message " + std::to_string(i));
        
        dbus_message_unref(msg);
    }
    
    std::string log_content = readLogFile();
    
    // Check that first and last messages are present
    EXPECT_TRUE(log_content.find("Message 0") != std::string::npos);
    EXPECT_TRUE(log_content.find("Message " + std::to_string(num_messages - 1)) != std::string::npos);
    
    // Count occurrences of "BULK D-Bus message"
    size_t count = 0;
    size_t pos = 0;
    std::string search_str = "BULK D-Bus message";
    while ((pos = log_content.find(search_str, pos)) != std::string::npos) {
        count++;
        pos += search_str.length();
    }
    
    EXPECT_EQ(count, num_messages);
}

#else // !HAVE_DBUS

// Stub test when D-Bus is not available
TEST(MPRISLoggerDBusTracingTest, DBusNotAvailable) {
    EXPECT_TRUE(true); // Placeholder test
}

#endif // HAVE_DBUS