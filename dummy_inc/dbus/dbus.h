#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <mutex>
#include <variant>

#define DBUS_HANDLER_RESULT_HANDLED 1
#define DBUS_HANDLER_RESULT_NOT_YET_HANDLED 0
#define DBUS_HANDLER_RESULT_NEED_MEMORY 2
#define DBUS_TYPE_STRING 1
#define DBUS_TYPE_VARIANT 2
#define DBUS_TYPE_INT64 3
#define DBUS_TYPE_UINT64 4
#define DBUS_TYPE_DOUBLE 5
#define DBUS_TYPE_BOOLEAN 6
#define DBUS_TYPE_ARRAY 7
#define DBUS_TYPE_DICT_ENTRY 8
#define DBUS_TYPE_OBJECT_PATH 9
#define TRUE 1
#define FALSE 0

typedef int DBusHandlerResult;
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusMessageIter DBusMessageIter;
typedef int dbus_bool_t;
typedef long long dbus_int64_t;
typedef unsigned long long dbus_uint64_t;

extern "C" {
    const char* dbus_message_get_interface(DBusMessage*);
    const char* dbus_message_get_member(DBusMessage*);
    DBusMessage* dbus_message_new_method_return(DBusMessage*);
    DBusMessage* dbus_message_new_error(DBusMessage*, const char*, const char*);
    void dbus_connection_send(DBusConnection*, DBusMessage*, void*);
    void dbus_message_unref(DBusMessage*);
    void dbus_message_iter_init_append(DBusMessage*, DBusMessageIter*);
    void dbus_message_iter_open_container(DBusMessageIter*, int, const char*, DBusMessageIter*);
    void dbus_message_iter_append_basic(DBusMessageIter*, int, const void*);
    void dbus_message_iter_close_container(DBusMessageIter*, DBusMessageIter*);
    int dbus_message_iter_init(DBusMessage*, DBusMessageIter*);
    int dbus_message_iter_get_arg_type(DBusMessageIter*);
    void dbus_message_iter_get_basic(DBusMessageIter*, void*);
    int dbus_message_iter_next(DBusMessageIter*);
    void dbus_message_iter_recurse(DBusMessageIter*, DBusMessageIter*);
}
