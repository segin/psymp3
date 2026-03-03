#pragma once
#include <stdint.h>

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusMessageIter {
    void *dummy1;
    void *dummy2;
    uint32_t dummy3;
    int dummy4;
    int dummy5;
    int dummy6;
    int dummy7;
    int dummy8;
    int dummy9;
} DBusMessageIter;

#define DBUS_TYPE_STRING 's'
#define DBUS_TYPE_VARIANT 'v'
#define DBUS_TYPE_ARRAY 'a'
#define DBUS_TYPE_DICT_ENTRY 'e'
#define DBUS_TYPE_INT64 'x'
#define DBUS_TYPE_UINT64 't'
#define DBUS_TYPE_DOUBLE 'd'
#define DBUS_TYPE_BOOLEAN 'b'
#define DBUS_TYPE_OBJECT_PATH 'o'

typedef int dbus_bool_t;
typedef int64_t dbus_int64_t;
typedef uint64_t dbus_uint64_t;

#define TRUE 1
#define FALSE 0

enum DBusHandlerResult {
    DBUS_HANDLER_RESULT_HANDLED,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
    DBUS_HANDLER_RESULT_NEED_MEMORY
};

// Mock functions
inline void dbus_message_iter_init_append(DBusMessage*, DBusMessageIter*) {}
inline void dbus_message_iter_open_container(DBusMessageIter*, int, const char*, DBusMessageIter*) {}
inline void dbus_message_iter_close_container(DBusMessageIter*, DBusMessageIter*) {}
inline void dbus_message_iter_append_basic(DBusMessageIter*, int, const void*) {}
inline const char* dbus_message_get_interface(DBusMessage*) { return ""; }
inline const char* dbus_message_get_member(DBusMessage*) { return ""; }
inline DBusMessage* dbus_message_new_method_return(DBusMessage*) { return nullptr; }
inline DBusMessage* dbus_message_new_error(DBusMessage*, const char*, const char*) { return nullptr; }
inline void dbus_connection_send(DBusConnection*, DBusMessage*, void*) {}
inline void dbus_message_unref(DBusMessage*) {}
inline int dbus_message_iter_init(DBusMessage*, DBusMessageIter*) { return 0; }
inline int dbus_message_iter_get_arg_type(DBusMessageIter*) { return 0; }
inline void dbus_message_iter_get_basic(DBusMessageIter*, void*) {}
inline int dbus_message_iter_next(DBusMessageIter*) { return 0; }
inline void dbus_message_iter_recurse(DBusMessageIter*, DBusMessageIter*) {}
