# MPRIS Migration Notes

## Overview

This document describes the changes made during the MPRIS refactoring and provides guidance for developers working with the new MPRIS implementation.

## What Changed

### Architecture Overhaul

The MPRIS implementation was completely rewritten from a monolithic design to a modular, thread-safe architecture:

**Before (Legacy):**
- Single `MPRIS` class handling all functionality
- Direct D-Bus calls without proper error handling
- No thread safety guarantees
- Manual resource management
- Limited error recovery

**After (Refactored):**
- Modular design with specialized components:
  - `MPRISManager` - Central coordinator
  - `DBusConnectionManager` - Connection lifecycle
  - `PropertyManager` - State caching
  - `MethodHandler` - D-Bus method processing
  - `SignalEmitter` - Asynchronous notifications
- Comprehensive error handling and recovery
- Thread-safe design following public/private lock pattern
- RAII resource management
- Automatic reconnection capabilities

### API Changes

#### Player Class Integration

**Before:**
```cpp
// Legacy MPRIS class (if it existed)
class MPRIS {
    // Direct, unsafe integration
};
```

**After:**
```cpp
class Player {
    friend class MPRISManager;  // Controlled access
    
#ifdef HAVE_DBUS
    std::unique_ptr<MPRISManager> m_mpris_manager;
#endif
};
```

#### Initialization Changes

**Before:**
```cpp
// Manual, error-prone initialization
mpris = new MPRIS();
mpris->init();  // Could fail silently
```

**After:**
```cpp
#ifdef HAVE_DBUS
    m_mpris_manager = std::make_unique<MPRISManager>(this);
    auto init_result = m_mpris_manager->initialize();
    if (!init_result.isSuccess()) {
        // Graceful degradation
        m_mpris_manager.reset();
    }
#endif
```

#### State Update Pattern

**Before:**
```cpp
// Direct, potentially unsafe calls
if (mpris) {
    mpris->updateStatus(status);  // No error handling
}
```

**After:**
```cpp
#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
    }
#endif
```

### Threading Safety Improvements

#### Lock Pattern Implementation

All MPRIS components now follow the public/private lock pattern:

```cpp
class MPRISComponent {
public:
    // Public API - acquires locks
    void updateProperty(const Value& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        updateProperty_unlocked(value);
    }
    
private:
    // Private implementation - assumes locks held
    void updateProperty_unlocked(const Value& value) {
        // Safe implementation
    }
    
    mutable std::mutex m_mutex;
};
```

#### Callback Safety

**Before:**
```cpp
// Unsafe - could cause deadlocks
void handleCallback() {
    lock();
    callback();  // Callback might call back into MPRIS
    unlock();
}
```

**After:**
```cpp
// Safe - callbacks invoked without locks
void handleCallback() {
    CallbackInfo info;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        info = prepareCallback_unlocked();
    }
    info.invoke();  // Safe callback invocation
}
```

### Error Handling Improvements

#### Result Pattern

The new implementation uses a `Result<T>` pattern for comprehensive error handling:

```cpp
template<typename T>
class Result {
public:
    static Result success(T value);
    static Result error(const std::string& message);
    
    bool isSuccess() const;
    const T& getValue() const;
    const std::string& getError() const;
};

// Usage
auto result = mpris_manager->initialize();
if (!result.isSuccess()) {
    Debug::log("mpris", "Initialization failed: ", result.getError());
}
```

#### Graceful Degradation

**Before:**
```cpp
// All-or-nothing approach
if (!mpris->init()) {
    // Application might fail or behave unpredictably
}
```

**After:**
```cpp
// Graceful degradation
auto init_result = m_mpris_manager->initialize();
if (!init_result.isSuccess()) {
    Debug::log("mpris", "MPRIS initialization failed: ", init_result.getError());
    // Continue without MPRIS - core functionality unaffected
    m_mpris_manager.reset();
}
```

## Migration Guide for Developers

### For Core PsyMP3 Developers

#### Adding New MPRIS Functionality

1. **New Properties**: Add to `PropertyManager` class
2. **New Methods**: Add to `MethodHandler` class  
3. **New Signals**: Add to `SignalEmitter` class
4. **Integration**: Update `MPRISManager` to coordinate new functionality

#### Threading Safety Requirements

When modifying MPRIS code, ensure:

1. **Public/Private Pattern**: All public methods must have `_unlocked` counterparts
2. **Lock Order**: Follow documented lock acquisition order
3. **Callback Safety**: Never invoke callbacks while holding locks
4. **RAII**: Use RAII for all resource management

#### Testing Requirements

All MPRIS changes must include:

1. **Unit Tests**: Test individual components in isolation
2. **Integration Tests**: Test with real Player instance
3. **Threading Tests**: Verify thread safety under load
4. **Error Tests**: Test error handling and recovery

### For External Integrators

#### D-Bus Interface Compatibility

The D-Bus interface remains fully compatible with the MPRIS specification:

- **Service Name**: `org.mpris.MediaPlayer2.psymp3`
- **Object Path**: `/org/mpris/MediaPlayer2`
- **Interfaces**: Standard MPRIS interfaces unchanged

#### Client Code

Existing MPRIS client code should work without changes:

```bash
# These commands work the same as before
dbus-send --session --dest=org.mpris.MediaPlayer2.psymp3 \
    --type=method_call /org/mpris/MediaPlayer2 \
    org.mpris.MediaPlayer2.Player.PlayPause

dbus-send --session --dest=org.mpris.MediaPlayer2.psymp3 \
    --type=method_call --print-reply /org/mpris/MediaPlayer2 \
    org.freedesktop.DBus.Properties.Get \
    string:org.mpris.MediaPlayer2.Player string:Metadata
```

## Build System Changes

### Conditional Compilation

MPRIS support is now properly conditionally compiled:

```cpp
#ifdef HAVE_DBUS
    // MPRIS-specific code
    #include "MPRISManager.h"
    // ...
#endif
```

### Configure Options

The build system now properly detects and handles D-Bus:

```bash
# Automatic detection
./configure

# Explicit control
./configure --enable-dbus    # Force enable (fail if not found)
./configure --disable-dbus   # Explicitly disable
```

## Debugging Changes

### Debug Channels

MPRIS now uses the debug channel system:

```cpp
Debug::log("mpris", "Message");           // Info level
Debug::log("mpris", "Warning: ", msg);    // Warning level
Debug::log("mpris", "Error: ", error);    // Error level
```

### Debug Output

Enable MPRIS debugging:

```bash
./psymp3 --debug 2>&1 | grep mpris
```

### Logging Improvements

- **Structured Logging**: Consistent format and categorization
- **Error Context**: Detailed error information for troubleshooting
- **Performance Metrics**: Optional performance monitoring
- **State Tracking**: Detailed state change logging

## Performance Improvements

### Reduced Lock Contention

- **Property Caching**: Reduces Player lock acquisition
- **Asynchronous Signals**: Non-blocking D-Bus communication
- **Batched Updates**: Combines multiple property changes

### Memory Efficiency

- **RAII Management**: Automatic resource cleanup
- **Bounded Queues**: Prevents memory growth under load
- **String Optimization**: Reduced string copying

### Connection Management

- **Automatic Reconnection**: Handles D-Bus service restarts
- **Connection Pooling**: Efficient D-Bus connection reuse
- **Error Recovery**: Robust handling of connection issues

## Compatibility Notes

### Backward Compatibility

- **D-Bus Interface**: Fully compatible with MPRIS specification
- **Player API**: No changes to public Player interface
- **Build System**: Maintains existing configure options

### Breaking Changes

- **Internal APIs**: MPRIS internal APIs completely changed
- **Debug Output**: Debug message format changed
- **Error Handling**: Error reporting mechanism changed

### Deprecations

- **Legacy MPRIS Class**: Completely removed (if it existed)
- **Manual Resource Management**: Replaced with RAII
- **Synchronous Operations**: Replaced with asynchronous where appropriate

## Future Considerations

### Planned Enhancements

1. **Extended MPRIS Support**: Additional MPRIS interfaces
2. **Performance Monitoring**: Built-in performance metrics
3. **Configuration Options**: User-configurable MPRIS behavior
4. **Plugin Architecture**: Extensible MPRIS functionality

### API Stability

The new MPRIS architecture is designed for stability:

- **Modular Design**: Changes can be isolated to specific components
- **Interface Abstraction**: Internal changes don't affect external APIs
- **Version Compatibility**: Forward and backward compatibility considerations

## Getting Help

### Documentation

- [MPRIS Architecture](mpris-architecture.md) - Detailed architecture overview
- [MPRIS Troubleshooting](mpris-troubleshooting.md) - Common issues and solutions
- [Threading Safety Guidelines](threading-safety-patterns.md) - Threading requirements

### Support

For questions about the MPRIS migration:

1. Check existing documentation
2. Review debug output with `--debug` flag
3. Test with minimal reproduction case
4. Include system information in bug reports

### Contributing

When contributing to MPRIS code:

1. Follow threading safety guidelines
2. Include comprehensive tests
3. Update documentation as needed
4. Ensure backward compatibility where possible