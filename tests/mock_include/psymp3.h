/*
 * Mock psymp3.h for testing MethodHandler compilation
 */

#ifndef MOCK_PSYMP3_H
#define MOCK_PSYMP3_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <mutex>
#include <memory>
#include <variant>
#include <stdexcept>

// Mock DBus types
struct DBusConnection {};
struct DBusMessage {};
struct DBusMessageIter {};
typedef int dbus_bool_t;
typedef long long dbus_int64_t;
typedef unsigned long long dbus_uint64_t;

#define DBUS_TYPE_STRING 's'
#define DBUS_TYPE_VARIANT 'v'
#define DBUS_TYPE_ARRAY 'a'
#define DBUS_TYPE_DICT_ENTRY 'e'
#define DBUS_TYPE_INT64 'x'
#define DBUS_TYPE_UINT64 't'
#define DBUS_TYPE_DOUBLE 'd'
#define DBUS_TYPE_BOOLEAN 'b'
#define DBUS_TYPE_OBJECT_PATH 'o'

#define TRUE 1
#define FALSE 0

#define DBUS_HANDLER_RESULT_HANDLED 0
#define DBUS_HANDLER_RESULT_NOT_YET_HANDLED 1
#define DBUS_HANDLER_RESULT_NEED_MEMORY 2

typedef int DBusHandlerResult;

// Mock DBus functions
extern "C" {
    const char* dbus_message_get_interface(DBusMessage*);
    const char* dbus_message_get_member(DBusMessage*);
    DBusMessage* dbus_message_new_method_return(DBusMessage*);
    DBusMessage* dbus_message_new_error(DBusMessage*, const char*, const char*);
    void dbus_connection_send(DBusConnection*, DBusMessage*, void*);
    void dbus_message_unref(DBusMessage*);

    int dbus_message_iter_init(DBusMessage*, DBusMessageIter*);
    void dbus_message_iter_init_append(DBusMessage*, DBusMessageIter*);
    int dbus_message_iter_get_arg_type(DBusMessageIter*);
    void dbus_message_iter_get_basic(DBusMessageIter*, void*);
    int dbus_message_iter_next(DBusMessageIter*);
    void dbus_message_iter_recurse(DBusMessageIter*, DBusMessageIter*);

    void dbus_message_iter_open_container(DBusMessageIter*, int, const char*, DBusMessageIter*);
    void dbus_message_iter_append_basic(DBusMessageIter*, int, const void*);
    void dbus_message_iter_close_container(DBusMessageIter*, DBusMessageIter*);
}

// Namespace setup
namespace PsyMP3 {
    enum LoopMode { None, One, All };

    class Player {
    public:
        bool play() { return true; }
        bool pause() { return true; }
        bool stop() { return true; }
        bool playPause() { return true; }
        void nextTrack() {}
        void prevTrack() {}
        void seekTo(unsigned long) {}
        void setVolume(double) {}
        double getVolume() const { return 1.0; }
        void setLoopMode(LoopMode) {}
        static void synthesizeUserEvent(int, void*, void*) {}
    };

    namespace MPRIS {
        // Types
        using DBusDictionary = std::map<std::string, struct DBusVariant>;

        struct DBusVariant {
            enum Type { String, StringArray, Int64, UInt64, Double, Boolean, Dictionary };
            Type type;

            DBusVariant(const std::string&) : type(String) {}
            DBusVariant(const std::vector<std::string>&) : type(StringArray) {}
            DBusVariant(int64_t) : type(Int64) {}
            DBusVariant(uint64_t) : type(UInt64) {}
            DBusVariant(double) : type(Double) {}
            DBusVariant(bool) : type(Boolean) {}
            DBusVariant(std::shared_ptr<DBusDictionary>) : type(Dictionary) {}

            template<typename T> const T& get() const {
                static T val;
                return val; // Dummy
            }
        };

        template<typename T>
        class Result {
        public:
            static Result success(T val) { return Result(); }
            static Result error(const std::string&) { return Result(); }
            bool isSuccess() const { return true; }
            T getValue() const { return T(); }
            std::string getError() const { return ""; }
        };

        std::string loopStatusToString(LoopMode) { return "None"; }

        const int64_t MAX_SEEK_OFFSET_US = 3600000000LL;
        const uint64_t MAX_POSITION_US = 86400000000ULL;

        class PropertyManager {
        public:
            bool canGoNext() { return true; }
            bool canGoPrevious() { return true; }
            bool canSeek() { return true; }
            bool canControl() { return true; }
            uint64_t getPosition() { return 0; }
            uint64_t getLength() { return 0; }
            std::string getPlaybackStatus() { return "Stopped"; }
            std::map<std::string, std::string> getMetadata() { return {}; }
            LoopMode getLoopStatus() { return LoopMode::None; }

            DBusDictionary getAllProperties() { return {}; }
        };

        class MethodHandler {
        public:
            MethodHandler(Player*, PropertyManager*);
            ~MethodHandler();
            DBusHandlerResult handleMessage(DBusConnection*, DBusMessage*);
            bool isReady() const;

        private:
            Player* m_player;
            PropertyManager* m_properties;
            bool m_initialized;
            mutable std::mutex m_mutex;
            std::map<std::string, std::function<DBusHandlerResult(DBusConnection*, DBusMessage*)>> m_method_handlers;

            DBusHandlerResult handleMessage_unlocked(DBusConnection*, DBusMessage*);
            bool isReady_unlocked() const;
            void initializeMethodHandlers_unlocked();

            // Handlers
            DBusHandlerResult handleRaise_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleQuit_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handlePlay_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handlePause_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleStop_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handlePlayPause_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleNext_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handlePrevious_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleSeek_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleSetPosition_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleGetProperty_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleSetProperty_unlocked(DBusConnection*, DBusMessage*);
            DBusHandlerResult handleGetAllProperties_unlocked(DBusConnection*, DBusMessage*);

            // Helpers
            void appendVariantToMessage_unlocked(DBusMessage*, const DBusVariant&);
            void appendVariantToIter_unlocked(DBusMessageIter*, const DBusVariant&);
            void appendPropertyToMessage_unlocked(DBusMessage*, const std::string&);
            void appendAllPropertiesToMessage_unlocked(DBusMessage*, const std::string&);

            void sendMethodReturn_unlocked(DBusConnection*, DBusMessage*);
            void sendErrorReply_unlocked(DBusConnection*, DBusMessage*, const std::string&, const std::string&);

            void logMethodCall_unlocked(const std::string&, const std::string&);
            void logError_unlocked(const std::string&, const std::string&);
            void logValidationError_unlocked(const std::string&, const std::string&, const std::string&);

            Result<int64_t> validateSeekOffset_unlocked(int64_t);
            Result<uint64_t> validatePosition_unlocked(uint64_t);
            Result<std::string> validateTrackId_unlocked(const std::string&);

            Result<int64_t> parseSeekArguments_unlocked(DBusMessage*);
            Result<std::pair<std::string, uint64_t>> parseSetPositionArguments_unlocked(DBusMessage*);
            Result<std::pair<std::string, std::string>> parsePropertyArguments_unlocked(DBusMessage*);
        };
    }
}

// Constants
#define MPRIS_MEDIAPLAYER2_INTERFACE "org.mpris.MediaPlayer2"
#define MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define QUIT_APPLICATION 0

#endif // MOCK_PSYMP3_H
