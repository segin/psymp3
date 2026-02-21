#ifndef MOCK_DBUS_H
#define MOCK_DBUS_H

#include <cstdint>

#define DBUS_HANDLER_RESULT_HANDLED ((DBusHandlerResult)0)
#define DBUS_HANDLER_RESULT_NOT_YET_HANDLED ((DBusHandlerResult)1)
#define DBUS_HANDLER_RESULT_NEED_MEMORY ((DBusHandlerResult)2)

#define DBUS_TYPE_STRING 's'
#define DBUS_TYPE_VARIANT 'v'
#define DBUS_TYPE_INT64 'x'
#define DBUS_TYPE_UINT64 't'
#define DBUS_TYPE_DOUBLE 'd'
#define DBUS_TYPE_BOOLEAN 'b'
#define DBUS_TYPE_ARRAY 'a'
#define DBUS_TYPE_DICT_ENTRY 'e'
#define DBUS_TYPE_OBJECT_PATH 'o'

#define TRUE 1
#define FALSE 0

typedef uint32_t dbus_bool_t;
typedef int64_t dbus_int64_t;
typedef uint64_t dbus_uint64_t;

struct DBusConnection;
struct DBusMessage;
struct DBusMessageIter {
    void *dummy1;
    void *dummy2;
    uint32_t dummy3;
    int dummy4;
    int dummy5;
    int dummy6;
    int dummy7;
    int dummy8;
    int dummy9;
    int dummy10;
    int dummy11;
    int dummy12;
    void *dummy13;
};

enum DBusHandlerResult {
    DBUS_HANDLER_RESULT_HANDLED_ENUM = 0
};

// Function mocks (declarations only for syntax check)
extern "C" {
const char *dbus_message_get_interface(DBusMessage *message);
const char *dbus_message_get_member(DBusMessage *message);
DBusMessage *dbus_message_new_method_return(DBusMessage *method_call);
DBusMessage *dbus_message_new_error(DBusMessage *reply_to, const char *error_name, const char *error_message);
void dbus_connection_send(DBusConnection *connection, DBusMessage *message, uint32_t *serial);
void dbus_message_unref(DBusMessage *message);
void dbus_message_iter_init_append(DBusMessage *message, DBusMessageIter *iter);
void dbus_message_iter_open_container(DBusMessageIter *iter, int type, const char *contained_signature, DBusMessageIter *sub);
void dbus_message_iter_close_container(DBusMessageIter *iter, DBusMessageIter *sub);
void dbus_message_iter_append_basic(DBusMessageIter *iter, int type, const void *value);
int dbus_message_iter_init(DBusMessage *message, DBusMessageIter *iter);
int dbus_message_iter_get_arg_type(DBusMessageIter *iter);
void dbus_message_iter_get_basic(DBusMessageIter *iter, void *value);
int dbus_message_iter_next(DBusMessageIter *iter);
void dbus_message_iter_recurse(DBusMessageIter *iter, DBusMessageIter *sub);
}

#endif

