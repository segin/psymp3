# Design Document

## Overview

This design outlines the integration of MPRIS debug output with the existing PsyMP3 debug system. The current MPRIS implementation uses a separate `MPRISLogger` class that creates inconsistent debug output. The solution is straightforward: replace all MPRIS logging calls with the existing `Debug::log()` system and remove the unnecessary `MPRISLogger` complexity.

## Architecture

### Current State Analysis

**Existing Debug System:**
- Uses `Debug` class with template-based logging
- Format: `HH:MM:SS.microseconds [channel]: message`
- Channel-based filtering (e.g., "player", "audio")
- Simple enable/disable per channel
- Thread-safe with mutex protection

**Current MPRIS Logging:**
- Uses `MPRISLogger` singleton with unnecessary complexity
- Format: `[YYYY-MM-DD HH:MM:SS.milliseconds] [LEVEL] [component] message`
- Multiple log levels and performance tracking that aren't needed

### Integration Strategy

Simple replacement approach:

1. **Replace MPRIS logging macros** with `Debug::log()` calls
2. **Map MPRIS components** to debug channels
3. **Remove MPRISLogger class** entirely
4. **Update debug channel documentation**

## Components and Interfaces

### Debug Channel Mapping

MPRIS components will use these debug channels:

```cpp
MPRISManager        -> "mpris"
DBusConnectionManager -> "dbus" 
PropertyManager     -> "mpris"
MethodHandler       -> "mpris"
SignalEmitter       -> "mpris"
```

### Replacement Macros

Simple macros that map to the existing debug system:

```cpp
// Replace MPRIS_LOG_* macros with Debug::log calls
#define MPRIS_LOG_TRACE(component, message) \
    Debug::log("mpris", "[", component, "] ", message)

#define MPRIS_LOG_DEBUG(component, message) \
    Debug::log("mpris", "[", component, "] ", message)

#define MPRIS_LOG_INFO(component, message) \
    Debug::log("mpris", "[", component, "] ", message)

#define MPRIS_LOG_WARN(component, message) \
    Debug::log("mpris", "[", component, "] WARNING: ", message)

#define MPRIS_LOG_ERROR(component, message) \
    Debug::log("mpris", "[", component, "] ERROR: ", message)

#define MPRIS_LOG_FATAL(component, message) \
    Debug::log("mpris", "[", component, "] FATAL: ", message)
```

### D-Bus Message Tracing

Simplified D-Bus tracing using existing debug system:

```cpp
#define MPRIS_TRACE_DBUS_MESSAGE(direction, message, context) \
    Debug::log("dbus", direction, " D-Bus message (", context, ")")

#define MPRIS_MEASURE_LOCK(lock_name) \
    // Remove - unnecessary complexity
```

## Data Models

### Files to Modify

1. **Replace in headers**: Update `MPRISLogger.h` with simple macros
2. **Update source files**: Replace all `MPRIS_LOG_*` calls 
3. **Remove implementation**: Delete `MPRISLogger.cpp`
4. **Update documentation**: Add MPRIS channels to `docs/debug-channels.md`

### Connection State Handling

Connection state logging will be simplified to use regular debug output instead of the complex state tracking system.

## Error Handling

Error handling becomes much simpler:
- All errors go through the standard debug system
- No special error recovery needed
- Debug channel filtering handles verbosity

## Testing Strategy

### Basic Testing

1. **Functionality Test**: Verify MPRIS still works after logging changes
2. **Debug Output Test**: Check that debug messages appear with correct format
3. **Channel Filtering Test**: Verify `--debug mpris` and `--debug dbus` work correctly

### Validation

1. **Build Test**: Ensure clean compilation after removing MPRISLogger
2. **Runtime Test**: Verify no crashes or missing functionality
3. **Debug Format Test**: Confirm consistent debug output format

## Implementation Plan

### Phase 1: Create Replacement Macros
- Create new simple macros in a temporary header
- Test that they work with existing debug system

### Phase 2: Replace Usage
- Update all MPRIS source files to use new macros
- Remove all references to MPRISLogger methods

### Phase 3: Remove MPRISLogger
- Delete MPRISLogger.h and MPRISLogger.cpp
- Update build system
- Update debug channel documentation

### Phase 4: Testing and Documentation
- Test MPRIS functionality with new debug output
- Update `docs/debug-channels.md` with MPRIS channels
- Verify debug output consistency