# Design Document

## Overview

This design addresses critical threading safety issues in the PsyMP3 codebase where public API methods that acquire resource locks can cause deadlocks when called from other public lock-acquiring methods within the same class. The analysis reveals several classes with complex threading patterns that need systematic refactoring to follow a consistent public/private lock pattern.

## Architecture

### Current Threading Issues Identified

Based on code analysis, the following classes have threading safety concerns:

1. **Audio Class** (`src/audio.cpp`, `include/audio.h`)
   - Uses multiple mutexes: `m_buffer_mutex`, `m_stream_mutex`, `m_player_mutex`
   - Public methods like `isFinished()`, `setStream()`, `resetBuffer()` acquire locks
   - Potential for deadlock when public methods call each other

2. **IOHandler Class** (`src/IOHandler.cpp`, `include/IOHandler.h`)
   - Uses `s_memory_mutex` (static) and `m_operation_mutex` (shared_mutex)
   - Multiple public methods acquire locks: `read()`, `seek()`, `tell()`, `close()`
   - Static methods like `getMemoryStats()`, `setMemoryLimits()` also acquire locks

3. **MemoryPoolManager Class** (`src/MemoryPoolManager.cpp`, `include/MemoryPoolManager.h`)
   - Uses `m_mutex` for protecting internal state
   - Public methods like `allocateBuffer()`, `releaseBuffer()`, `getMemoryStats()` acquire locks
   - Complex callback system that could lead to deadlocks

4. **Surface Class** (`src/surface.cpp`)
   - Uses SDL surface locking with `SDL_LockSurface()`/`SDL_UnlockSurface()`
   - Multiple public drawing methods acquire surface locks

### Threading Safety Pattern Design

The solution implements a consistent **Public/Private Lock Pattern** where:

- **Public methods** provide the external API and handle lock acquisition
- **Private methods** with `_unlocked` suffix perform the actual work without acquiring locks
- **Internal calls** between methods use the private unlocked versions

## Components and Interfaces

### 1. Lock Pattern Interface

```cpp
class ThreadSafeClass {
public:
    // Public API - acquires locks
    ReturnType publicMethod(params);
    
private:
    // Private implementation - assumes locks are held
    ReturnType publicMethod_unlocked(params);
    
    mutable std::mutex m_mutex;
};
```

### 2. Audio Class Refactoring

**Current Issues:**
- `setStream()` acquires both `m_stream_mutex` and `m_buffer_mutex`
- `isFinished()` acquires `m_buffer_mutex`
- If `setStream()` internally needed to check if finished, deadlock could occur

**Proposed Structure:**
```cpp
class Audio {
public:
    bool isFinished() const;
    std::unique_ptr<Stream> setStream(std::unique_ptr<Stream> new_stream);
    void resetBuffer();
    uint64_t getBufferLatencyMs() const;
    
private:
    bool isFinished_unlocked() const;
    std::unique_ptr<Stream> setStream_unlocked(std::unique_ptr<Stream> new_stream);
    void resetBuffer_unlocked();
    uint64_t getBufferLatencyMs_unlocked() const;
};
```

### 3. IOHandler Class Refactoring

**Current Issues:**
- Multiple static methods acquire `s_memory_mutex`
- Instance methods acquire `m_operation_mutex`
- Memory management methods could be called from within I/O operations

**Proposed Structure:**
```cpp
class IOHandler {
public:
    size_t read(void* buffer, size_t size, size_t count);
    int seek(off_t offset, int whence);
    off_t tell();
    int close();
    static std::map<std::string, size_t> getMemoryStats();
    
private:
    size_t read_unlocked(void* buffer, size_t size, size_t count);
    int seek_unlocked(off_t offset, int whence);
    off_t tell_unlocked();
    int close_unlocked();
    static std::map<std::string, size_t> getMemoryStats_unlocked();
};
```

### 4. MemoryPoolManager Class Refactoring

**Current Issues:**
- `notifyPressureCallbacks()` called from within locked methods
- Callback functions might call back into MemoryPoolManager methods
- Complex interaction between memory tracking and pool management

**Proposed Structure:**
```cpp
class MemoryPoolManager {
public:
    uint8_t* allocateBuffer(size_t size, const std::string& component_name);
    void releaseBuffer(uint8_t* buffer, size_t size, const std::string& component_name);
    std::map<std::string, size_t> getMemoryStats() const;
    
private:
    uint8_t* allocateBuffer_unlocked(size_t size, const std::string& component_name);
    void releaseBuffer_unlocked(uint8_t* buffer, size_t size, const std::string& component_name);
    std::map<std::string, size_t> getMemoryStats_unlocked() const;
    void notifyPressureCallbacks_unlocked();
};
```

## Data Models

### Lock Acquisition Order

To prevent deadlocks, establish a consistent lock acquisition order:

1. **Global/Static locks** (e.g., `IOHandler::s_memory_mutex`)
2. **Manager-level locks** (e.g., `MemoryPoolManager::m_mutex`)
3. **Instance-level locks** (e.g., `Audio::m_stream_mutex`, `Audio::m_buffer_mutex`)
4. **SDL/System locks** (e.g., `SDL_LockSurface`)

### Lock Hierarchy Documentation

```cpp
// Lock acquisition order (to prevent deadlocks):
// 1. IOHandler::s_memory_mutex (global memory tracking)
// 2. MemoryPoolManager::m_mutex (pool management)
// 3. Audio::m_stream_mutex (stream operations)
// 4. Audio::m_buffer_mutex (buffer operations)
// 5. SDL surface locks (graphics operations)
```

### Method Naming Convention

- **Public methods**: Standard naming (e.g., `allocateBuffer`)
- **Private unlocked methods**: Append `_unlocked` (e.g., `allocateBuffer_unlocked`)
- **Internal helper methods**: Use descriptive names without `_unlocked` if they don't correspond to public methods

## Error Handling

### Deadlock Prevention Strategies

1. **Lock Timeout**: Implement timeout-based lock acquisition where appropriate
2. **Lock-Free Atomics**: Use atomic operations for simple state queries
3. **RAII Lock Guards**: Ensure locks are always released via RAII
4. **Exception Safety**: Ensure locks are released even when exceptions occur

### Error Recovery Patterns

```cpp
// Pattern for safe lock acquisition with timeout
template<typename Mutex>
bool safe_lock_with_timeout(Mutex& mutex, std::chrono::milliseconds timeout) {
    if (mutex.try_lock_for(timeout)) {
        return true;
    }
    Debug::log("threading", "Lock acquisition timeout - potential deadlock detected");
    return false;
}
```

### Callback Safety

For classes like MemoryPoolManager that use callbacks:

1. **Callback Isolation**: Never call callbacks while holding internal locks
2. **Callback Queuing**: Queue callback notifications and process them after releasing locks
3. **Reentrancy Protection**: Detect and prevent reentrant callback calls

## Testing Strategy

### Unit Testing Approach

1. **Lock Contention Tests**: Verify that multiple threads can safely call public methods
2. **Deadlock Detection Tests**: Create scenarios that would cause deadlocks with the old pattern
3. **Performance Tests**: Ensure the refactoring doesn't significantly impact performance
4. **Stress Tests**: High-concurrency scenarios to validate thread safety

### Test Categories

#### 1. Basic Thread Safety Tests
- Multiple threads calling the same public method
- Multiple threads calling different public methods on the same object
- Verify no data races or corruption

#### 2. Deadlock Prevention Tests
- Simulate scenarios where old code would deadlock
- Verify new pattern prevents these deadlocks
- Test lock acquisition order compliance

#### 3. Callback Safety Tests (MemoryPoolManager)
- Verify callbacks don't cause deadlocks
- Test callback reentrancy protection
- Validate callback isolation from internal locks

#### 4. Performance Regression Tests
- Benchmark critical paths before and after refactoring
- Ensure lock overhead is minimized
- Validate that unlocked methods have no performance penalty

### Integration Testing

1. **Audio Playback Tests**: Verify audio streaming works correctly with new threading
2. **I/O Handler Tests**: Test file and HTTP I/O operations under concurrent access
3. **Memory Management Tests**: Validate memory pool operations under high concurrency
4. **System Integration Tests**: Full application testing with multiple concurrent operations

### Test Implementation Framework

```cpp
// Example test structure for threading safety
class ThreadSafetyTest {
public:
    void testConcurrentAccess() {
        const int num_threads = 8;
        const int operations_per_thread = 1000;
        
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    try {
                        // Test concurrent operations
                        testObject.publicMethod();
                    } catch (...) {
                        errors++;
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        ASSERT_EQ(0, errors.load());
    }
};
```

## Implementation Priority

### Phase 1: Critical Audio Components
- Audio class refactoring (highest priority - affects playback)
- Basic thread safety tests for Audio class

### Phase 2: I/O Subsystem
- IOHandler class refactoring
- FileIOHandler and HTTPIOHandler derived classes
- I/O subsystem thread safety tests

### Phase 3: Memory Management
- MemoryPoolManager class refactoring
- Callback safety implementation
- Memory management thread safety tests

### Phase 4: Graphics and UI
- Surface class SDL lock refactoring
- Widget system thread safety (if needed)
- UI thread safety tests

### Phase 5: Documentation and Enforcement
- Update steering documents with threading guidelines
- Create static analysis rules (where possible)
- Comprehensive integration testing