#ifndef METHODHANDLER_H
#define METHODHANDLER_H

#include <string>
#include <map>
#include <mutex>
#include <functional>

// Forward declarations
class Player;
struct DBusConnection;
struct DBusMessage;

namespace PsyMP3 {
namespace MPRIS {
    class PropertyManager;
}
}

namespace PsyMP3 {
namespace MPRIS {
    struct DBusVariant;
    template<typename T> class Result;
}
}

// D-Bus handler result type
#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#else
// Define our own enum for when D-Bus is not available
enum DBusHandlerResult {
    DBUS_HANDLER_RESULT_HANDLED,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
    DBUS_HANDLER_RESULT_NEED_MEMORY
};
// Forward declarations for types used in member functions
struct DBusMessageIter;
#endif

/**
 * MethodHandler - Processes incoming D-Bus method calls with proper error handling
 * 
 * This class follows the public/private lock pattern established in the threading
 * safety guidelines. All public methods acquire locks and call private _unlocked
 * implementations. Internal method calls use the _unlocked versions.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. MethodHandler::m_mutex (this class)
 * 2. PropertyManager locks (when calling PropertyManager methods)
 * 3. Player locks (when calling Player methods)
 */

namespace PsyMP3 {
namespace MPRIS {

class MethodHandler {
public:
    /**
     * Constructor
     * @param player Pointer to Player instance for command execution (non-owning)
     * @param properties Pointer to PropertyManager for state queries (non-owning)
     */
    MethodHandler(Player* player, PropertyManager* properties);
    
    /**
     * Destructor - ensures clean shutdown
     */
    ~MethodHandler();
    
    // Public API - acquires locks and calls private implementations
    
    /**
     * Handle incoming D-Bus message
     * @param connection D-Bus connection
     * @param message Incoming D-Bus message
     * @return Handler result indicating if message was processed
     */
    DBusHandlerResult handleMessage(DBusConnection* connection, DBusMessage* message);
    
    /**
     * Check if handler is ready to process messages
     * @return true if handler is initialized and ready
     */
    bool isReady() const;

private:
    // Private implementations - assume locks are already held
    
    DBusHandlerResult handleMessage_unlocked(DBusConnection* connection, DBusMessage* message);
    bool isReady_unlocked() const;
    
    // Individual method handlers for MPRIS MediaPlayer2 interface
    DBusHandlerResult handleRaise_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handleQuit_unlocked(DBusConnection* connection, DBusMessage* message);
    
    // Individual method handlers for MPRIS MediaPlayer2.Player interface
    DBusHandlerResult handlePlay_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handlePause_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handleStop_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handlePlayPause_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handleNext_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handlePrevious_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handleSeek_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handleSetPosition_unlocked(DBusConnection* connection, DBusMessage* message);
    
    // Property interface handlers (org.freedesktop.DBus.Properties)
    DBusHandlerResult handleGetProperty_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handleSetProperty_unlocked(DBusConnection* connection, DBusMessage* message);
    DBusHandlerResult handleGetAllProperties_unlocked(DBusConnection* connection, DBusMessage* message);
    
    // Utility methods for D-Bus message handling
    void sendMethodReturn_unlocked(DBusConnection* connection, DBusMessage* message);
    void sendErrorReply_unlocked(DBusConnection* connection, DBusMessage* message, 
                                const std::string& error_name, const std::string& error_message);
    
    // Input validation helpers
    PsyMP3::MPRIS::Result<int64_t> validateSeekOffset_unlocked(int64_t offset);
    PsyMP3::MPRIS::Result<uint64_t> validatePosition_unlocked(uint64_t position_us);
    PsyMP3::MPRIS::Result<std::string> validateTrackId_unlocked(const std::string& track_id);
    
    // D-Bus message parsing helpers
    PsyMP3::MPRIS::Result<int64_t> parseSeekArguments_unlocked(DBusMessage* message);
    PsyMP3::MPRIS::Result<std::pair<std::string, uint64_t>> parseSetPositionArguments_unlocked(DBusMessage* message);
    PsyMP3::MPRIS::Result<std::pair<std::string, std::string>> parsePropertyArguments_unlocked(DBusMessage* message);
    
    // Property value serialization for D-Bus responses
    void appendVariantToMessage_unlocked(DBusMessage* reply, const PsyMP3::MPRIS::DBusVariant& variant);
    void appendVariantToIter_unlocked(DBusMessageIter* iter, const PsyMP3::MPRIS::DBusVariant& variant);
    void appendPropertyToMessage_unlocked(DBusMessage* reply, const std::string& property_name);
    void appendAllPropertiesToMessage_unlocked(DBusMessage* reply, const std::string& interface_name);
    
    // Error handling and logging
    void logMethodCall_unlocked(const std::string& interface_name, const std::string& method_name);
    void logError_unlocked(const std::string& context, const std::string& error_message);
    void logValidationError_unlocked(const std::string& method_name, const std::string& parameter, 
                                   const std::string& error_message);
    
    // Thread synchronization
    mutable std::mutex m_mutex;
    
    // Component references (non-owning)
    Player* m_player;
    PropertyManager* m_properties;
    
    // Handler state
    bool m_initialized;
    
    // Method dispatch table for efficient routing
    std::map<std::string, std::function<DBusHandlerResult(DBusConnection*, DBusMessage*)>> m_method_handlers;
    
    // Initialize method dispatch table
    void initializeMethodHandlers_unlocked();
    
    // Constants for MPRIS interfaces
    static constexpr const char* MPRIS_MEDIAPLAYER2_INTERFACE = "org.mpris.MediaPlayer2";
    static constexpr const char* MPRIS_PLAYER_INTERFACE = "org.mpris.MediaPlayer2.Player";
    static constexpr const char* DBUS_PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";
    
    // Constants for validation limits
    static constexpr int64_t MAX_SEEK_OFFSET_US = 3600000000LL; // 1 hour in microseconds
    static constexpr uint64_t MAX_POSITION_US = 86400000000ULL; // 24 hours in microseconds
};

} // namespace MPRIS
} // namespace PsyMP3
#endif // METHODHANDLER_H