#ifndef MPRISTYPES_H
#define MPRISTYPES_H

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <exception>

// Forward declarations for D-Bus types
struct DBusConnection;
struct DBusMessage;

namespace MPRISTypes {

// Enumerations for MPRIS protocol
enum class PlaybackStatus {
    Playing,
    Paused,
    Stopped
};

enum class LoopStatus {
    None,
    Track,
    Playlist
};

// DBus variant type for property values
struct DBusVariant {
    enum Type { 
        String = 0, 
        StringArray = 1, 
        Int64 = 2, 
        UInt64 = 3, 
        Double = 4, 
        Boolean = 5 
    } type;
    
    std::variant<std::string, std::vector<std::string>, int64_t, uint64_t, double, bool> value;
    
    // Default constructor
    DBusVariant() : type(String), value(std::string{}) {}
    
    // Constructors for different types
    explicit DBusVariant(const std::string& str) : type(String), value(str) {}
    explicit DBusVariant(const std::vector<std::string>& arr) : type(StringArray), value(arr) {}
    explicit DBusVariant(int64_t i) : type(Int64), value(i) {}
    explicit DBusVariant(uint64_t u) : type(UInt64), value(u) {}
    explicit DBusVariant(double d) : type(Double), value(d) {}
    explicit DBusVariant(bool b) : type(Boolean), value(b) {}
    
    // Type-safe getters
    template<typename T>
    const T& get() const {
        return std::get<T>(value);
    }
    
    // String conversion for debugging
    std::string toString() const;
};

// MPRIS metadata structure
struct MPRISMetadata {
    std::string artist;
    std::string title;
    std::string album;
    std::string track_id;
    uint64_t length_us = 0;
    std::string art_url;
    
    // Convert to D-Bus dictionary format
    std::map<std::string, DBusVariant> toDBusDict() const;
    
    // Clear all metadata
    void clear();
    
    // Check if metadata is empty
    bool isEmpty() const;
};

// RAII deleters for D-Bus resources
struct DBusConnectionDeleter {
    void operator()(DBusConnection* conn);
};

struct DBusMessageDeleter {
    void operator()(DBusMessage* msg);
};

// RAII wrappers for D-Bus resources
using DBusConnectionPtr = std::unique_ptr<DBusConnection, DBusConnectionDeleter>;
using DBusMessagePtr = std::unique_ptr<DBusMessage, DBusMessageDeleter>;

// Result template class for error handling
template<typename T>
class Result {
public:
    // Factory methods
    static Result success(T value) { 
        return Result(std::move(value)); 
    }
    
    static Result error(const std::string& message) { 
        return Result(ErrorTag{}, message); 
    }
    
    // State checking
    bool isSuccess() const { return m_success; }
    bool isError() const { return !m_success; }
    
    // Value access (throws if error)
    const T& getValue() const {
        if (!m_success) {
            throw std::runtime_error("Attempted to get value from error result: " + m_error);
        }
        return m_value;
    }
    
    T& getValue() {
        if (!m_success) {
            throw std::runtime_error("Attempted to get value from error result: " + m_error);
        }
        return m_value;
    }
    
    // Error access
    const std::string& getError() const { return m_error; }
    
    // Move value out (for efficiency)
    T moveValue() {
        if (!m_success) {
            throw std::runtime_error("Attempted to move value from error result: " + m_error);
        }
        return std::move(m_value);
    }
    
    // Conversion operators for convenience
    explicit operator bool() const { return m_success; }
    
private:
    // Private constructors to enforce factory pattern
    explicit Result(T value) : m_success(true), m_value(std::move(value)) {}
    
    // Use a tag to distinguish error constructor
    struct ErrorTag {};
    explicit Result(ErrorTag, const std::string& error) : m_success(false), m_error(error) {}
    
    bool m_success;
    T m_value;
    std::string m_error;
};

// Specialization for void results
template<>
class Result<void> {
public:
    static Result success() { 
        return Result(true); 
    }
    
    static Result error(const std::string& message) { 
        return Result(false, message); 
    }
    
    bool isSuccess() const { return m_success; }
    bool isError() const { return !m_success; }
    
    const std::string& getError() const { return m_error; }
    
    explicit operator bool() const { return m_success; }
    
private:
    explicit Result(bool success) : m_success(success) {}
    Result(bool success, const std::string& error) : m_success(success), m_error(error) {}
    
    bool m_success;
    std::string m_error;
};

// Utility functions for type conversions
std::string playbackStatusToString(PlaybackStatus status);
PlaybackStatus stringToPlaybackStatus(const std::string& str);

std::string loopStatusToString(LoopStatus status);
LoopStatus stringToLoopStatus(const std::string& str);

} // namespace MPRISTypes

#endif // MPRISTYPES_H