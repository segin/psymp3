# MPRIS Architecture Documentation

## Overview

The MPRIS (Media Player Remote Interfacing Specification) implementation in PsyMP3 provides D-Bus integration for desktop media control. This document describes the architecture, design patterns, and implementation details of the refactored MPRIS system.

## Architecture Overview

The MPRIS system follows a modular, thread-safe design with clear separation of concerns:

```
Player Class
    ↓
MPRISManager (Central Coordinator)
    ├── DBusConnectionManager (Connection Lifecycle)
    ├── PropertyManager (State Caching)
    ├── MethodHandler (D-Bus Method Processing)
    └── SignalEmitter (Asynchronous Notifications)
```

## Core Components

### 1. MPRISManager

**Purpose**: Central coordinator that implements the public API and manages all MPRIS components.

**Key Features**:
- Implements the public/private lock pattern for thread safety
- Manages component lifecycle and error propagation
- Provides graceful degradation when D-Bus is unavailable
- Handles automatic reconnection on connection loss

**Threading Safety**: All public methods acquire locks and call private `_unlocked` implementations.

### 2. DBusConnectionManager

**Purpose**: Manages D-Bus connection lifecycle with automatic error recovery.

**Key Features**:
- RAII-based resource management for D-Bus connections
- Automatic reconnection with exponential backoff
- Connection state monitoring and cleanup
- Thread-safe connection access

**Error Handling**: Gracefully handles D-Bus service unavailability and network issues.

### 3. PropertyManager

**Purpose**: Thread-safe caching and management of MPRIS properties.

**Key Features**:
- Caches Player state to avoid frequent queries
- Thread-safe property getters and setters
- Timestamp-based position interpolation
- Property-to-D-Bus conversion utilities

**Performance**: Minimizes Player lock contention by caching frequently accessed properties.

### 4. MethodHandler

**Purpose**: Processes incoming D-Bus method calls with comprehensive error handling.

**Key Features**:
- Individual handlers for each MPRIS method (Play, Pause, Stop, etc.)
- Input validation and error response generation
- Safe Player command execution without holding internal locks
- Malformed message rejection

**Safety**: Never calls Player methods while holding internal MPRIS locks to prevent deadlocks.

### 5. SignalEmitter

**Purpose**: Sends MPRIS property change signals asynchronously.

**Key Features**:
- Non-blocking signal emission using worker thread
- Signal batching to reduce D-Bus traffic
- Bounded queue to prevent memory growth
- Error handling and retry logic

**Performance**: Asynchronous operation ensures Player operations are never blocked by D-Bus communication.

## Threading Safety Patterns

### Public/Private Lock Pattern

All MPRIS classes follow the established threading safety pattern:

```cpp
class MPRISComponent {
public:
    // Public API - acquires locks
    ReturnType publicMethod(parameters) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return publicMethod_unlocked(parameters);
    }
    
private:
    // Private implementation - assumes locks held
    ReturnType publicMethod_unlocked(parameters) {
        // Implementation here
        // Can safely call other _unlocked methods
    }
    
    mutable std::mutex m_mutex;
};
```

### Lock Acquisition Order

To prevent deadlocks, the following lock acquisition order is enforced:

1. **Player locks** (when calling Player methods)
2. **MPRIS component locks** (internal synchronization)
3. **D-Bus system locks** (handled by libdbus)

### Callback Safety

MPRIS components never invoke callbacks while holding internal locks:

```cpp
void MPRISComponent::operationWithCallback() {
    CallbackInfo callback_info;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Prepare callback but don't invoke
        callback_info = prepareCallback_unlocked();
    }
    
    // Invoke callback after releasing locks
    callback_info.invoke();
}
```

## Error Handling Strategy

### Error Categories

1. **Connection Errors**: D-Bus service unavailability, network issues
2. **Message Errors**: Malformed D-Bus messages, invalid parameters
3. **Player State Errors**: Invalid state transitions, resource conflicts
4. **Threading Errors**: Deadlock prevention, race condition handling

### Error Recovery

- **Connection Loss**: Automatic reconnection with exponential backoff
- **Invalid Messages**: Graceful rejection with appropriate error responses
- **Player Errors**: Sensible default responses to maintain MPRIS functionality
- **Resource Exhaustion**: Graceful degradation without affecting core playback

### Result Pattern

The implementation uses a `Result<T>` pattern for error handling:

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
```

## Integration with Player

### Initialization

MPRIS is initialized in the Player constructor with conditional compilation:

```cpp
#ifdef HAVE_DBUS
    m_mpris_manager = std::make_unique<MPRISManager>(this);
    auto init_result = m_mpris_manager->initialize();
    if (!init_result.isSuccess()) {
        // Graceful degradation - continue without MPRIS
        m_mpris_manager.reset();
    }
#endif
```

### State Updates

Player notifies MPRIS of state changes at key points:

- **Playback Status**: Updated on play, pause, stop operations
- **Metadata**: Updated when new tracks are loaded
- **Position**: Updated periodically during playback and on seek operations

### Lock Safety

Player-to-MPRIS calls are made outside of Player locks to prevent deadlocks:

```cpp
// In Player::updateGUI()
#ifdef HAVE_DBUS
    // Update MPRIS position (outside of Player mutex)
    if (m_mpris_manager && current_stream && state == PlayerState::Playing) {
        m_mpris_manager->updatePosition(current_pos_ms * 1000);
    }
#endif
```

## Performance Considerations

### Optimization Strategies

1. **Lazy Updates**: Only emit signals when properties actually change
2. **Batched Signals**: Combine multiple property changes into single D-Bus signal
3. **Asynchronous Operations**: Non-blocking signal emission and reconnection
4. **Property Caching**: Cache frequently accessed properties to avoid Player queries

### Memory Management

1. **RAII Throughout**: All D-Bus resources managed with smart pointers
2. **Bounded Queues**: Prevent memory growth under high load
3. **String Optimization**: Use string views where possible to avoid copies
4. **Resource Cleanup**: Automatic cleanup on component destruction

## Build Integration

### Conditional Compilation

MPRIS support is conditionally compiled based on D-Bus availability:

```cpp
#ifdef HAVE_DBUS
    // MPRIS-specific code
#endif
```

### Build Configuration

The build system detects D-Bus availability and sets the `HAVE_DBUS` preprocessor flag:

```bash
./configure  # Automatically detects D-Bus
# or
./configure --disable-dbus  # Explicitly disable MPRIS
```

## Testing Strategy

### Unit Testing

- **Mock D-Bus**: Isolated testing without real D-Bus dependency
- **Threading Safety**: Concurrent access validation
- **Error Injection**: Error handling and recovery testing
- **Property Synchronization**: State consistency verification

### Integration Testing

- **Real D-Bus**: Testing against actual D-Bus daemon
- **Player Integration**: Full system testing with real Player instance
- **Stress Testing**: High-concurrency scenarios
- **Reconnection Testing**: Network failure simulation

## Debugging and Logging

### Debug Channels

MPRIS uses the debug channel system for logging:

```cpp
Debug::log("mpris", "MPRIS initialized successfully");
Debug::log("mpris", "Connection lost, attempting reconnection");
```

### Logging Levels

- **Info**: Initialization, shutdown, major state changes
- **Warning**: Recoverable errors, connection issues
- **Error**: Unrecoverable errors, critical failures
- **Debug**: Detailed operation tracing, message dumps

### Troubleshooting

Common issues and solutions are documented in the troubleshooting guide (see `docs/mpris-troubleshooting.md`).

## Future Enhancements

### Planned Features

1. **Extended MPRIS Interface**: Support for additional MPRIS methods
2. **Performance Metrics**: Lock contention and latency monitoring
3. **Configuration Options**: User-configurable MPRIS behavior
4. **Advanced Error Recovery**: More sophisticated reconnection strategies

### Extensibility

The modular design allows for easy extension:

- New MPRIS methods can be added to MethodHandler
- Additional property types can be supported in PropertyManager
- Custom signal types can be implemented in SignalEmitter
- Alternative D-Bus backends can be integrated via DBusConnectionManager

## References

- [MPRIS Specification](https://specifications.freedesktop.org/mpris-spec/latest/)
- [D-Bus Specification](https://dbus.freedesktop.org/doc/dbus-specification.html)
- [PsyMP3 Threading Safety Guidelines](threading-safety-patterns.md)