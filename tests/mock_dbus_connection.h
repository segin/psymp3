/*
 * mock_dbus_connection.h - Mock D-Bus connection for MPRIS testing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MOCK_DBUS_CONNECTION_H
#define MOCK_DBUS_CONNECTION_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>

// Forward declarations for D-Bus types
struct DBusConnection;
struct DBusMessage;

namespace TestFramework {

/**
 * @brief Mock D-Bus message for testing
 */
class MockDBusMessage {
public:
    enum class Type {
        MethodCall,
        MethodReturn,
        Error,
        Signal
    };
    
    MockDBusMessage(Type type, const std::string& interface, const std::string& member);
    ~MockDBusMessage();
    
    // Message properties
    Type getType() const { return m_type; }
    const std::string& getInterface() const { return m_interface; }
    const std::string& getMember() const { return m_member; }
    const std::string& getPath() const { return m_path; }
    const std::string& getDestination() const { return m_destination; }
    const std::string& getSender() const { return m_sender; }
    uint32_t getSerial() const { return m_serial; }
    
    // Message modification
    void setPath(const std::string& path) { m_path = path; }
    void setDestination(const std::string& destination) { m_destination = destination; }
    void setSender(const std::string& sender) { m_sender = sender; }
    void setSerial(uint32_t serial) { m_serial = serial; }
    
    // Arguments
    void addStringArgument(const std::string& value);
    void addInt64Argument(int64_t value);
    void addUInt64Argument(uint64_t value);
    void addDoubleArgument(double value);
    void addBooleanArgument(bool value);
    void addDictArgument(const std::map<std::string, std::string>& dict);
    
    const std::vector<std::string>& getStringArguments() const { return m_string_args; }
    const std::vector<int64_t>& getInt64Arguments() const { return m_int64_args; }
    const std::vector<uint64_t>& getUInt64Arguments() const { return m_uint64_args; }
    const std::vector<double>& getDoubleArguments() const { return m_double_args; }
    const std::vector<bool>& getBooleanArguments() const { return m_boolean_args; }
    const std::vector<std::map<std::string, std::string>>& getDictArguments() const { return m_dict_args; }
    
    // Error information (for Error type messages)
    void setErrorName(const std::string& error_name) { m_error_name = error_name; }
    void setErrorMessage(const std::string& error_message) { m_error_message = error_message; }
    const std::string& getErrorName() const { return m_error_name; }
    const std::string& getErrorMessage() const { return m_error_message; }
    
    // Validation
    bool isValid() const;
    std::string getValidationError() const;
    
    // Conversion to string for debugging
    std::string toString() const;
    
    // Stream operator for testing
    friend std::ostream& operator<<(std::ostream& os, Type type) {
        switch (type) {
            case Type::MethodCall: return os << "MethodCall";
            case Type::MethodReturn: return os << "MethodReturn";
            case Type::Error: return os << "Error";
            case Type::Signal: return os << "Signal";
            default: return os << "Unknown";
        }
    }

private:
    Type m_type;
    std::string m_interface;
    std::string m_member;
    std::string m_path;
    std::string m_destination;
    std::string m_sender;
    uint32_t m_serial = 0;
    
    // Arguments by type
    std::vector<std::string> m_string_args;
    std::vector<int64_t> m_int64_args;
    std::vector<uint64_t> m_uint64_args;
    std::vector<double> m_double_args;
    std::vector<bool> m_boolean_args;
    std::vector<std::map<std::string, std::string>> m_dict_args;
    
    // Error information
    std::string m_error_name;
    std::string m_error_message;
    
    static uint32_t s_next_serial;
};

/**
 * @brief Mock D-Bus connection for testing MPRIS functionality
 */
class MockDBusConnection {
public:
    /**
     * @brief Connection state for simulation
     */
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Error
    };
    
    /**
     * @brief Configuration for mock behavior
     */
    struct Config {
        bool auto_connect;
        bool simulate_connection_failures;
        double connection_failure_rate; // 10% failure rate
        std::chrono::milliseconds connection_delay;
        bool simulate_message_failures;
        double message_failure_rate; // 5% failure rate
        size_t max_message_queue_size;
        bool enable_message_logging;
        
        Config() : auto_connect(true), simulate_connection_failures(false), 
                   connection_failure_rate(0.1), connection_delay(10),
                   simulate_message_failures(false), message_failure_rate(0.05),
                   max_message_queue_size(1000), enable_message_logging(true) {}
    };
    
    /**
     * @brief Message handler function type
     */
    using MessageHandler = std::function<std::unique_ptr<MockDBusMessage>(const MockDBusMessage&)>;
    
    /**
     * @brief Connection state change callback
     */
    using StateChangeCallback = std::function<void(State old_state, State new_state)>;
    
    MockDBusConnection(const Config& config = Config());
    ~MockDBusConnection();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    State getState() const;
    
    // Service registration
    bool requestName(const std::string& service_name);
    bool releaseName(const std::string& service_name);
    std::vector<std::string> getOwnedNames() const;
    
    // Message handling
    void setMessageHandler(MessageHandler handler);
    bool sendMessage(std::unique_ptr<MockDBusMessage> message);
    std::unique_ptr<MockDBusMessage> receiveMessage(std::chrono::milliseconds timeout = std::chrono::milliseconds{100});
    
    // Message queue management
    size_t getMessageQueueSize() const;
    void clearMessageQueue();
    std::vector<std::unique_ptr<MockDBusMessage>> getAllMessages();
    
    // State change notifications
    void setStateChangeCallback(StateChangeCallback callback);
    
    // Error simulation
    void simulateConnectionLoss();
    void simulateConnectionRestore();
    void setConnectionFailureRate(double rate);
    void setMessageFailureRate(double rate);
    
    // Statistics and monitoring
    struct Statistics {
        size_t messages_sent = 0;
        size_t messages_received = 0;
        size_t messages_failed = 0;
        size_t connection_attempts = 0;
        size_t connection_failures = 0;
        std::chrono::system_clock::time_point last_connection_time;
        std::chrono::system_clock::time_point last_disconnection_time;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
    // Message inspection utilities
    std::vector<MockDBusMessage*> findMessagesByInterface(const std::string& interface) const;
    std::vector<MockDBusMessage*> findMessagesByMember(const std::string& member) const;
    MockDBusMessage* findLastMessage() const;
    MockDBusMessage* findLastMessageByType(MockDBusMessage::Type type) const;
    
    // Validation and testing utilities
    bool validateMessage(const MockDBusMessage& message) const;
    std::string getLastError() const;
    void setLastError(const std::string& error);
    
    // Configuration access
    const Config& getConfig() const { return m_config; }
    void updateConfig(const Config& config);
    
    // Thread safety testing
    void enableThreadSafetyTesting(bool enable) { m_thread_safety_testing = enable; }
    bool isThreadSafetyTestingEnabled() const { return m_thread_safety_testing; }
    
    // Stream operator for testing
    friend std::ostream& operator<<(std::ostream& os, State state) {
        switch (state) {
            case State::Disconnected: return os << "Disconnected";
            case State::Connecting: return os << "Connecting";
            case State::Connected: return os << "Connected";
            case State::Error: return os << "Error";
            default: return os << "Unknown";
        }
    }

private:
    Config m_config;
    mutable std::mutex m_mutex;
    
    State m_state = State::Disconnected;
    std::vector<std::string> m_owned_names;
    MessageHandler m_message_handler;
    StateChangeCallback m_state_change_callback;
    
    // Message queue
    std::queue<std::unique_ptr<MockDBusMessage>> m_message_queue;
    std::vector<std::unique_ptr<MockDBusMessage>> m_sent_messages; // For inspection
    
    // Error handling
    std::string m_last_error;
    
    // Statistics
    mutable Statistics m_statistics;
    
    // Thread safety testing
    std::atomic<bool> m_thread_safety_testing{false};
    
    // Internal methods
    bool connect_unlocked();
    void disconnect_unlocked();
    void setState_unlocked(State new_state);
    void clearMessageQueue_unlocked();
    bool shouldSimulateFailure(double failure_rate) const;
    void updateStatistics_unlocked(const std::string& operation, bool success);
    void logMessage_unlocked(const std::string& direction, const MockDBusMessage& message);
};

/**
 * @brief Factory for creating mock D-Bus messages
 */
class MockDBusMessageFactory {
public:
    // MPRIS method calls
    static std::unique_ptr<MockDBusMessage> createPlayMethodCall();
    static std::unique_ptr<MockDBusMessage> createPauseMethodCall();
    static std::unique_ptr<MockDBusMessage> createStopMethodCall();
    static std::unique_ptr<MockDBusMessage> createNextMethodCall();
    static std::unique_ptr<MockDBusMessage> createPreviousMethodCall();
    static std::unique_ptr<MockDBusMessage> createSeekMethodCall(int64_t offset_us);
    static std::unique_ptr<MockDBusMessage> createSetPositionMethodCall(const std::string& track_id, uint64_t position_us);
    
    // Property access
    static std::unique_ptr<MockDBusMessage> createGetPropertyCall(const std::string& interface, const std::string& property);
    static std::unique_ptr<MockDBusMessage> createSetPropertyCall(const std::string& interface, const std::string& property, const std::string& value);
    static std::unique_ptr<MockDBusMessage> createGetAllPropertiesCall(const std::string& interface);
    
    // Signals
    static std::unique_ptr<MockDBusMessage> createPropertiesChangedSignal(const std::string& interface, const std::map<std::string, std::string>& changed_properties);
    static std::unique_ptr<MockDBusMessage> createSeekedSignal(uint64_t position_us);
    
    // Responses
    static std::unique_ptr<MockDBusMessage> createMethodReturn();
    static std::unique_ptr<MockDBusMessage> createErrorResponse(const std::string& error_name, const std::string& error_message);
    
    // Malformed messages for error testing
    static std::unique_ptr<MockDBusMessage> createMalformedMessage();
    static std::unique_ptr<MockDBusMessage> createMessageWithInvalidArguments();
    static std::unique_ptr<MockDBusMessage> createMessageWithMissingArguments();
};

/**
 * @brief Mock D-Bus connection manager for testing
 */
class MockDBusConnectionManager {
public:
    MockDBusConnectionManager();
    ~MockDBusConnectionManager();
    
    // Connection lifecycle
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Mock connection access
    MockDBusConnection* getConnection() { return m_connection.get(); }
    const MockDBusConnection* getConnection() const { return m_connection.get(); }
    
    // Auto-reconnection simulation
    void enableAutoReconnect(bool enable);
    bool attemptReconnection();
    
    // Error injection for testing
    void injectConnectionError(const std::string& error);
    void simulateConnectionLoss();
    void simulateConnectionRestore();
    
    // Configuration
    void setConnectionConfig(const MockDBusConnection::Config& config);
    
private:
    std::unique_ptr<MockDBusConnection> m_connection;
    bool m_auto_reconnect = false;
    std::chrono::steady_clock::time_point m_last_reconnect_attempt;
    mutable std::mutex m_mutex;
    
    bool connect_unlocked();
    void disconnect_unlocked();
    bool isConnected_unlocked() const;
};

} // namespace TestFramework

#endif // MOCK_DBUS_CONNECTION_H