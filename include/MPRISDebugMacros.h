#ifndef MPRIS_DEBUG_MACROS_H
#define MPRIS_DEBUG_MACROS_H

// Replacement macros that integrate with the existing PsyMP3 debug system
// These maintain component information in debug output while using Debug::log()

// Helper macro to determine the appropriate debug channel based on component
#define MPRIS_GET_DEBUG_CHANNEL(component) \
    (strstr(component, "DBus") != nullptr ? "dbus" : "mpris")

// Replacement macros that map MPRIS_LOG_* calls to Debug::log() calls
#define MPRIS_LOG_TRACE(component, message) \
    Debug::log(MPRIS_GET_DEBUG_CHANNEL(component), "[", component, "] ", message)

#define MPRIS_LOG_DEBUG(component, message) \
    Debug::log(MPRIS_GET_DEBUG_CHANNEL(component), "[", component, "] ", message)

#define MPRIS_LOG_INFO(component, message) \
    Debug::log(MPRIS_GET_DEBUG_CHANNEL(component), "[", component, "] ", message)

#define MPRIS_LOG_WARN(component, message) \
    Debug::log(MPRIS_GET_DEBUG_CHANNEL(component), "[", component, "] WARNING: ", message)

#define MPRIS_LOG_ERROR(component, message) \
    Debug::log(MPRIS_GET_DEBUG_CHANNEL(component), "[", component, "] ERROR: ", message)

#define MPRIS_LOG_FATAL(component, message) \
    Debug::log(MPRIS_GET_DEBUG_CHANNEL(component), "[", component, "] FATAL: ", message)

// Simplified D-Bus message tracing using existing debug system
#define MPRIS_TRACE_DBUS_MESSAGE(direction, message, context) \
    Debug::log("dbus", direction, " D-Bus message (", context, ")")

// Remove the complex lock measurement system - unnecessary complexity
#define MPRIS_MEASURE_LOCK(lock_name) \
    // Removed - unnecessary complexity for debug integration

#endif // MPRIS_DEBUG_MACROS_H