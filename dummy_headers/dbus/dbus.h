typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusMessageIter DBusMessageIter;
typedef int DBusHandlerResult;
#define DBUS_HANDLER_RESULT_HANDLED 0
#define DBUS_HANDLER_RESULT_NOT_YET_HANDLED 1
typedef int dbus_bool_t;
#define TRUE 1
#define FALSE 0
#define DBUS_TYPE_STRING 's'
#define DBUS_TYPE_VARIANT 'v'
#define DBUS_TYPE_INT64 'x'
#define DBUS_TYPE_UINT64 't'
#define DBUS_TYPE_DOUBLE 'd'
#define DBUS_TYPE_BOOLEAN 'b'
#define DBUS_TYPE_ARRAY 'a'
#define DBUS_TYPE_DICT_ENTRY 'e'
#define DBUS_TYPE_OBJECT_PATH 'o'
#define DBUS_TYPE_INVALID 0

inline void dbus_message_iter_init_append(DBusMessage*, DBusMessageIter*) {}
inline void dbus_message_iter_open_container(DBusMessageIter*, int, const char*, DBusMessageIter*) {}
inline void dbus_message_iter_close_container(DBusMessageIter*, DBusMessageIter*) {}
inline void dbus_message_iter_append_basic(DBusMessageIter*, int, const void*) {}
inline int dbus_message_iter_init(DBusMessage*, DBusMessageIter*) { return 0; }
inline int dbus_message_iter_get_arg_type(DBusMessageIter*) { return 0; }
inline void dbus_message_iter_get_basic(DBusMessageIter*, void*) {}
inline int dbus_message_iter_next(DBusMessageIter*) { return 0; }
inline void dbus_message_iter_recurse(DBusMessageIter*, DBusMessageIter*) {}
inline DBusMessage* dbus_message_new_method_return(DBusMessage*) { return 0; }
inline DBusMessage* dbus_message_new_error(DBusMessage*, const char*, const char*) { return 0; }
inline void dbus_message_unref(DBusMessage*) {}
inline void dbus_connection_send(DBusConnection*, DBusMessage*, void*) {}
inline const char* dbus_message_get_interface(DBusMessage*) { return ""; }
inline const char* dbus_message_get_member(DBusMessage*) { return ""; }
typedef long long dbus_int64_t;
typedef unsigned long long dbus_uint64_t;
struct DBusMessageIter {
    void *dummy1;
    void *dummy2;
    int dummy3;
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
