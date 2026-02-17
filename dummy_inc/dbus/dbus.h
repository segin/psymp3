#pragma once
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusMessageIter DBusMessageIter;
typedef int dbus_bool_t;
typedef long dbus_int64_t;
typedef unsigned long dbus_uint64_t;
typedef int DBusHandlerResult;
#define DBUS_HANDLER_RESULT_HANDLED 0
#define DBUS_HANDLER_RESULT_NOT_YET_HANDLED 1
#define DBUS_HANDLER_RESULT_NEED_MEMORY 2
#define DBUS_TYPE_STRING 's'
#define DBUS_TYPE_VARIANT 'v'
#define DBUS_TYPE_INT64 'x'
#define DBUS_TYPE_UINT64 't'
#define DBUS_TYPE_BOOLEAN 'b'
#define DBUS_TYPE_DOUBLE 'd'
#define DBUS_TYPE_ARRAY 'a'
#define DBUS_TYPE_DICT_ENTRY 'e'
#define DBUS_TYPE_OBJECT_PATH 'o'
#define TRUE 1
#define FALSE 0
