#ifndef PSYM_MOCK_H
#define PSYM_MOCK_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <variant>

#define HAVE_DBUS 1

// Mock D-Bus types
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
struct DBusMessageIter { int dummy; };
typedef int dbus_bool_t;
typedef long long dbus_int64_t;
typedef unsigned long long dbus_uint64_t;
#define TRUE 1
#define FALSE 0
#define DBUS_TYPE_STRING 's'
#define DBUS_TYPE_VARIANT 'v'
#define DBUS_TYPE_ARRAY 'a'
#define DBUS_TYPE_INT64 'x'
#define DBUS_TYPE_UINT64 't'
#define DBUS_TYPE_DOUBLE 'd'
#define DBUS_TYPE_BOOLEAN 'b'
#define DBUS_TYPE_DICT_ENTRY 'e'
#define DBUS_TYPE_OBJECT_PATH 'o'

// Mock D-Bus functions
enum DBusHandlerResult {
    DBUS_HANDLER_RESULT_HANDLED,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
    DBUS_HANDLER_RESULT_NEED_MEMORY
};

inline const char* dbus_message_get_interface(DBusMessage*) { return ""; }
inline const char* dbus_message_get_member(DBusMessage*) { return ""; }
inline DBusMessage* dbus_message_new_method_return(DBusMessage*) { return (DBusMessage*)1; }
inline DBusMessage* dbus_message_new_error(DBusMessage*, const char*, const char*) { return (DBusMessage*)1; }
inline void dbus_connection_send(DBusConnection*, DBusMessage*, void*) {}
inline void dbus_message_unref(DBusMessage*) {}
inline void dbus_message_iter_init_append(DBusMessage*, DBusMessageIter*) {}
inline void dbus_message_iter_open_container(DBusMessageIter*, int, const char*, DBusMessageIter*) {}
inline void dbus_message_iter_close_container(DBusMessageIter*, DBusMessageIter*) {}
inline void dbus_message_iter_append_basic(DBusMessageIter*, int, const void*) {}
inline int dbus_message_iter_init(DBusMessage*, DBusMessageIter*) { return 0; }
inline int dbus_message_iter_get_arg_type(DBusMessageIter*) { return 0; }
inline void dbus_message_iter_get_basic(DBusMessageIter*, void*) {}
inline int dbus_message_iter_next(DBusMessageIter*) { return 0; }
inline void dbus_message_iter_recurse(DBusMessageIter*, DBusMessageIter*) {}

namespace PsyMP3 {
    enum class LoopMode { None, One, All };

    class Player {
    public:
        static void synthesizeUserEvent(int, void*, void*) {}
        bool play() { return true; }
        bool pause() { return true; }
        bool stop() { return true; }
        bool playPause() { return true; }
        void nextTrack() {}
        void prevTrack() {}
        void seekTo(unsigned long) {}
        double getVolume() { return 1.0; }
        void setVolume(double) {}
        void setLoopMode(LoopMode) {}
    };


    namespace MPRIS {
        struct DBusVariant;
        using DBusDictionary = std::map<std::string, DBusVariant>;

        struct DBusVariant {
            enum Type { String, StringArray, Int64, UInt64, Double, Boolean, Dictionary };
            Type type;
            DBusVariant() : type(String) {}
            DBusVariant(const std::string&) : type(String) {}
            DBusVariant(const std::vector<std::string>&) : type(StringArray) {}
            DBusVariant(const DBusDictionary&) : type(Dictionary) {}

            template<typename T> const T& get() const { static T val; return val; }
        };

        template<typename T>
        class Result {
        public:
            static Result<T> success(T val) { return Result<T>(); }
            static Result<T> error(std::string msg) { return Result<T>(); }
            bool isSuccess() { return true; }
            std::string getError() { return ""; }
            T getValue() { return T(); }
        };

        inline std::string loopStatusToString(LoopMode) { return "None"; }

        class PropertyManager {
        public:
            bool canGoNext() { return true; }
            bool canGoPrevious() { return true; }
            bool canSeek() { return true; }
            bool canControl() { return true; }
            uint64_t getPosition() { return 0; }
            uint64_t getLength() { return 0; }
            std::string getPlaybackStatus() { return ""; }
            PsyMP3::MPRIS::DBusDictionary getMetadata() { return {}; }
            PsyMP3::LoopMode getLoopStatus() { return LoopMode::None; }
            std::map<std::string, PsyMP3::MPRIS::DBusVariant> getAllProperties() { return {}; }
        };
    }
}

// Include the real header we are testing
#include "mpris/MethodHandler.h"

#endif // PSYM_MOCK_H
