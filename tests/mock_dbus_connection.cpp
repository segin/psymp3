/*
 * mock_dbus_connection.cpp - Mock D-Bus connection implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS
#include "mock_dbus_connection.h"

namespace TestFramework {

// Static member initialization
uint32_t MockDBusMessage::s_next_serial = 1;

// MockDBusMessage implementation

MockDBusMessage::MockDBusMessage(Type type, const std::string& interface, const std::string& member)
    : m_type(type), m_interface(interface), m_member(member), m_serial(s_next_serial++) {
}

MockDBusMessage::~MockDBusMessage() = default;

void MockDBusMessage::addStringArgument(const std::string& value) {
    m_string_args.push_back(value);
}

void MockDBusMessage::addInt64Argument(int64_t value) {
    m_int64_args.push_back(value);
}

void MockDBusMessage::addUInt64Argument(uint64_t value) {
    m_uint64_args.push_back(value);
}

void MockDBusMessage::addDoubleArgument(double value) {
    m_double_args.push_back(value);
}

void MockDBusMessage::addBooleanArgument(bool value) {
    m_boolean_args.push_back(value);
}

void MockDBusMessage::addDictArgument(const std::map<std::string, std::string>& dict) {
    m_dict_args.push_back(dict);
}

bool MockDBusMessage::isValid() const {
    // Basic validation rules
    if (m_interface.empty() || m_member.empty()) {
        return false;
    }
    
    if (m_type == Type::Error && m_error_name.empty()) {
        return false;
    }
    
    return true;
}

std::string MockDBusMessage::getValidationError() const {
    if (m_interface.empty()) {
        return "Interface name is empty";
    }
    if (m_member.empty()) {
        return "Member name is empty";
    }
    if (m_type == Type::Error && m_error_name.empty()) {
        return "Error message missing error name";
    }
    return "";
}

std::string MockDBusMessage::toString() const {
    std::ostringstream oss;
    oss << "MockDBusMessage{";
    oss << "type=";
    switch (m_type) {
        case Type::MethodCall: oss << "MethodCall"; break;
        case Type::MethodReturn: oss << "MethodReturn"; break;
        case Type::Error: oss << "Error"; break;
        case Type::Signal: oss << "Signal"; break;
    }
    oss << ", interface=" << m_interface;
    oss << ", member=" << m_member;
    oss << ", path=" << m_path;
    oss << ", serial=" << m_serial;
    
    if (!m_string_args.empty()) {
        oss << ", string_args=[";
        for (size_t i = 0; i < m_string_args.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << m_string_args[i] << "\"";
        }
        oss << "]";
    }
    
    if (m_type == Type::Error) {
        oss << ", error_name=" << m_error_name;
        oss << ", error_message=" << m_error_message;
    }
    
    oss << "}";
    return oss.str();
}

// MockDBusConnection implementation

MockDBusConnection::MockDBusConnection(const Config& config) 
    : m_config(config), m_state(State::Disconnected) {
}

MockDBusConnection::~MockDBusConnection() {
    disconnect();
}

bool MockDBusConnection::connect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return connect_unlocked();
}

bool MockDBusConnection::connect_unlocked() {
    if (m_state == State::Connected) {
        return true;
    }
    
    setState_unlocked(State::Connecting);
    updateStatistics_unlocked("connect", true);
    
    // Simulate connection delay
    if (m_config.connection_delay.count() > 0) {
        std::this_thread::sleep_for(m_config.connection_delay);
    }
    
    // Simulate connection failure
    if (m_config.simulate_connection_failures && shouldSimulateFailure(m_config.connection_failure_rate)) {
        setLastError("Simulated connection failure");
        setState_unlocked(State::Error);
        updateStatistics_unlocked("connect", false);
        return false;
    }
    
    setState_unlocked(State::Connected);
    m_statistics.last_connection_time = std::chrono::system_clock::now();
    return true;
}

void MockDBusConnection::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    disconnect_unlocked();
}

void MockDBusConnection::disconnect_unlocked() {
    if (m_state == State::Disconnected) {
        return;
    }
    
    setState_unlocked(State::Disconnected);
    m_owned_names.clear();
    clearMessageQueue_unlocked();
    m_statistics.last_disconnection_time = std::chrono::system_clock::now();
}

bool MockDBusConnection::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == State::Connected;
}

MockDBusConnection::State MockDBusConnection::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

bool MockDBusConnection::requestName(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != State::Connected) {
        setLastError("Not connected to D-Bus");
        return false;
    }
    
    // Check if name is already owned
    auto it = std::find(m_owned_names.begin(), m_owned_names.end(), service_name);
    if (it != m_owned_names.end()) {
        return true; // Already owned
    }
    
    m_owned_names.push_back(service_name);
    return true;
}

bool MockDBusConnection::releaseName(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find(m_owned_names.begin(), m_owned_names.end(), service_name);
    if (it != m_owned_names.end()) {
        m_owned_names.erase(it);
        return true;
    }
    
    return false;
}

std::vector<std::string> MockDBusConnection::getOwnedNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_owned_names;
}

void MockDBusConnection::setMessageHandler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_message_handler = handler;
}

bool MockDBusConnection::sendMessage(std::unique_ptr<MockDBusMessage> message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != State::Connected) {
        setLastError("Not connected to D-Bus");
        updateStatistics_unlocked("send_message", false);
        return false;
    }
    
    if (!message || !message->isValid()) {
        setLastError("Invalid message");
        updateStatistics_unlocked("send_message", false);
        return false;
    }
    
    // Simulate message failure
    if (m_config.simulate_message_failures && shouldSimulateFailure(m_config.message_failure_rate)) {
        setLastError("Simulated message send failure");
        updateStatistics_unlocked("send_message", false);
        return false;
    }
    
    // Log message if enabled
    if (m_config.enable_message_logging) {
        logMessage_unlocked("SEND", *message);
    }
    
    // Store message for inspection
    m_sent_messages.push_back(std::make_unique<MockDBusMessage>(*message));
    
    // Process with message handler if available
    if (m_message_handler) {
        auto response = m_message_handler(*message);
        if (response) {
            // Add response to message queue
            if (m_message_queue.size() < m_config.max_message_queue_size) {
                m_message_queue.push(std::move(response));
            }
        }
    }
    
    updateStatistics_unlocked("send_message", true);
    return true;
}

std::unique_ptr<MockDBusMessage> MockDBusConnection::receiveMessage(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != State::Connected) {
        return nullptr;
    }
    
    if (m_message_queue.empty()) {
        return nullptr;
    }
    
    auto message = std::move(m_message_queue.front());
    m_message_queue.pop();
    
    // Log message if enabled
    if (m_config.enable_message_logging) {
        logMessage_unlocked("RECV", *message);
    }
    
    updateStatistics_unlocked("receive_message", true);
    return message;
}

size_t MockDBusConnection::getMessageQueueSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_message_queue.size();
}

void MockDBusConnection::clearMessageQueue() {
    std::lock_guard<std::mutex> lock(m_mutex);
    clearMessageQueue_unlocked();
}

void MockDBusConnection::clearMessageQueue_unlocked() {
    while (!m_message_queue.empty()) {
        m_message_queue.pop();
    }
}

std::vector<std::unique_ptr<MockDBusMessage>> MockDBusConnection::getAllMessages() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::unique_ptr<MockDBusMessage>> messages;
    while (!m_message_queue.empty()) {
        messages.push_back(std::move(m_message_queue.front()));
        m_message_queue.pop();
    }
    
    return messages;
}

void MockDBusConnection::setStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state_change_callback = callback;
}

void MockDBusConnection::simulateConnectionLoss() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == State::Connected) {
        setState_unlocked(State::Error);
        setLastError("Simulated connection loss");
    }
}

void MockDBusConnection::simulateConnectionRestore() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == State::Error) {
        setState_unlocked(State::Connected);
        m_last_error.clear();
    }
}

void MockDBusConnection::setConnectionFailureRate(double rate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.connection_failure_rate = rate;
}

void MockDBusConnection::setMessageFailureRate(double rate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.message_failure_rate = rate;
}

MockDBusConnection::Statistics MockDBusConnection::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

void MockDBusConnection::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statistics = Statistics{};
}

std::vector<MockDBusMessage*> MockDBusConnection::findMessagesByInterface(const std::string& interface) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<MockDBusMessage*> matches;
    for (const auto& message : m_sent_messages) {
        if (message->getInterface() == interface) {
            matches.push_back(message.get());
        }
    }
    
    return matches;
}

std::vector<MockDBusMessage*> MockDBusConnection::findMessagesByMember(const std::string& member) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<MockDBusMessage*> matches;
    for (const auto& message : m_sent_messages) {
        if (message->getMember() == member) {
            matches.push_back(message.get());
        }
    }
    
    return matches;
}

MockDBusMessage* MockDBusConnection::findLastMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_sent_messages.empty()) {
        return nullptr;
    }
    
    return m_sent_messages.back().get();
}

MockDBusMessage* MockDBusConnection::findLastMessageByType(MockDBusMessage::Type type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto it = m_sent_messages.rbegin(); it != m_sent_messages.rend(); ++it) {
        if ((*it)->getType() == type) {
            return it->get();
        }
    }
    
    return nullptr;
}

bool MockDBusConnection::validateMessage(const MockDBusMessage& message) const {
    return message.isValid();
}

std::string MockDBusConnection::getLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_last_error;
}

void MockDBusConnection::setLastError(const std::string& error) {
    m_last_error = error;
}

void MockDBusConnection::updateConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

// Private methods

void MockDBusConnection::setState_unlocked(State new_state) {
    State old_state = m_state;
    m_state = new_state;
    
    if (m_state_change_callback && old_state != new_state) {
        // Call callback without holding lock to prevent deadlocks
        auto callback = m_state_change_callback;
        // Note: We can't release the lock here safely, so we accept the risk
        // In a real implementation, we'd queue the callback for later execution
        callback(old_state, new_state);
    }
}

bool MockDBusConnection::shouldSimulateFailure(double failure_rate) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    return dist(gen) < failure_rate;
}

void MockDBusConnection::updateStatistics_unlocked(const std::string& operation, bool success) {
    if (operation == "connect") {
        m_statistics.connection_attempts++;
        if (!success) {
            m_statistics.connection_failures++;
        }
    } else if (operation == "send_message") {
        if (success) {
            m_statistics.messages_sent++;
        } else {
            m_statistics.messages_failed++;
        }
    } else if (operation == "receive_message") {
        if (success) {
            m_statistics.messages_received++;
        }
    }
}

void MockDBusConnection::logMessage_unlocked(const std::string& direction, const MockDBusMessage& message) {
    // Simple logging to stderr for now
    // In a real implementation, this would use a proper logging system
    std::cerr << "[MockDBus] " << direction << ": " << message.toString() << std::endl;
}

// MockDBusMessageFactory implementation

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createPlayMethodCall() {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "Play"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createPauseMethodCall() {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "Pause"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createStopMethodCall() {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "Stop"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createSeekMethodCall(int64_t offset_us) {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "Seek"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    message->addInt64Argument(offset_us);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createPropertiesChangedSignal(
    const std::string& interface, const std::map<std::string, std::string>& changed_properties) {
    
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::Signal,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    message->addStringArgument(interface);
    message->addDictArgument(changed_properties);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createNextMethodCall() {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "Next"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createPreviousMethodCall() {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "Previous"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createSetPositionMethodCall(const std::string& track_id, uint64_t position_us) {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "SetPosition"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    message->addStringArgument(track_id);
    message->addUInt64Argument(position_us);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createGetPropertyCall(const std::string& interface, const std::string& property) {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.freedesktop.DBus.Properties",
        "Get"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    message->addStringArgument(interface);
    message->addStringArgument(property);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createSetPropertyCall(const std::string& interface, const std::string& property, const std::string& value) {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.freedesktop.DBus.Properties",
        "Set"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    message->addStringArgument(interface);
    message->addStringArgument(property);
    message->addStringArgument(value);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createGetAllPropertiesCall(const std::string& interface) {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.freedesktop.DBus.Properties",
        "GetAll"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    message->addStringArgument(interface);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createSeekedSignal(uint64_t position_us) {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::Signal,
        "org.mpris.MediaPlayer2.Player",
        "Seeked"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    message->addUInt64Argument(position_us);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createMethodReturn() {
    return std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodReturn,
        "",
        ""
    );
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createErrorResponse(
    const std::string& error_name, const std::string& error_message) {
    
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::Error,
        "",
        ""
    );
    message->setErrorName(error_name);
    message->setErrorMessage(error_message);
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createMalformedMessage() {
    // Create a message with empty interface (invalid)
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "", // Empty interface - invalid
        "InvalidMethod"
    );
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createMessageWithInvalidArguments() {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.mpris.MediaPlayer2.Player",
        "Seek"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    // Missing required argument for Seek method
    return message;
}

std::unique_ptr<MockDBusMessage> MockDBusMessageFactory::createMessageWithMissingArguments() {
    auto message = std::make_unique<MockDBusMessage>(
        MockDBusMessage::Type::MethodCall,
        "org.freedesktop.DBus.Properties",
        "Get"
    );
    message->setPath("/org/mpris/MediaPlayer2");
    // Missing required arguments for Properties.Get
    return message;
}

// MockDBusConnectionManager implementation

MockDBusConnectionManager::MockDBusConnectionManager() 
    : m_connection(std::make_unique<MockDBusConnection>()) {
}

MockDBusConnectionManager::~MockDBusConnectionManager() {
    disconnect();
}

bool MockDBusConnectionManager::connect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return connect_unlocked();
}

bool MockDBusConnectionManager::connect_unlocked() {
    if (!m_connection) {
        m_connection = std::make_unique<MockDBusConnection>();
    }
    
    return m_connection->connect();
}

void MockDBusConnectionManager::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    disconnect_unlocked();
}

void MockDBusConnectionManager::disconnect_unlocked() {
    if (m_connection) {
        m_connection->disconnect();
    }
}

bool MockDBusConnectionManager::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return isConnected_unlocked();
}

bool MockDBusConnectionManager::isConnected_unlocked() const {
    return m_connection && m_connection->isConnected();
}

void MockDBusConnectionManager::enableAutoReconnect(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_auto_reconnect = enable;
}

bool MockDBusConnectionManager::attemptReconnection() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_auto_reconnect) {
        return false;
    }
    
    // Implement simple reconnection logic
    disconnect_unlocked();
    return connect_unlocked();
}

void MockDBusConnectionManager::injectConnectionError(const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connection) {
        m_connection->setLastError(error);
        m_connection->simulateConnectionLoss();
    }
}

void MockDBusConnectionManager::simulateConnectionLoss() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connection) {
        m_connection->simulateConnectionLoss();
    }
}

void MockDBusConnectionManager::simulateConnectionRestore() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connection) {
        m_connection->simulateConnectionRestore();
    }
}

void MockDBusConnectionManager::setConnectionConfig(const MockDBusConnection::Config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connection) {
        m_connection->updateConfig(config);
    }
}

} // namespace TestFramework

#endif // HAVE_DBUS