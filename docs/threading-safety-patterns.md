# Threading Safety Patterns Documentation

## Overview

This document analyzes the current lock acquisition patterns in the PsyMP3 codebase and identifies potential deadlock scenarios. This analysis supports the threading safety audit requirements 1.1, 1.3, and 5.1.

## Current Lock Acquisition Patterns

### 1. Audio Class (`src/audio.cpp`, `include/audio.h`)

**Mutex Members:**
- `m_buffer_mutex` - Protects audio buffer operations
- `m_stream_mutex` - Protects stream state changes  
- `m_player_mutex` - Protects player state

**Lock Acquisition Pattern:**
```cpp
// Current problematic pattern:
bool Audio::isFinished() const {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    // ... buffer state check
}

std::unique_ptr<Stream> Audio::setStream(std::unique_ptr<Stream> new_stream) {
    std::lock_guard<std::mutex> stream_lock(m_stream_mutex);
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // If this method internally called isFinished(), deadlock would occur
    // because isFinished() tries to acquire m_buffer_mutex again
}
```

**Identified Issues:**
- Public methods acquire locks without private unlocked counterparts
- Potential for deadlock if public methods call each other
- Multiple mutex usage without documented lock ordering

### 2. IOHandler Class (`src/IOHandler.cpp`, `include/IOHandler.h`)

**Mutex Members:**
- `s_memory_mutex` (static) - Protects global memory tracking
- `m_operation_mutex` (shared_mutex) - Protects I/O operations

**Lock Acquisition Pattern:**
```cpp
// Current pattern with potential issues:
size_t IOHandler::read(void* buffer, size_t size, size_t count) {
    std::shared_lock<std::shared_mutex> lock(m_operation_mutex);
    // ... read operation
    updateMemoryUsage(); // This might acquire s_memory_mutex
}

static std::map<std::string, size_t> IOHandler::getMemoryStats() {
    std::lock_guard<std::mutex> lock(s_memory_mutex);
    // ... return stats
}
```

**Identified Issues:**
- Static and instance methods both acquire locks
- Memory tracking methods called from within I/O operations
- Potential lock ordering issues between static and instance mutexes

### 3. MemoryPoolManager Class (`src/MemoryPoolManager.cpp`)

**Mutex Members:**
- `m_mutex` - Protects pool state and allocations

**Lock Acquisition Pattern:**
```cpp
// Current pattern with callback issues:
uint8_t* MemoryPoolManager::allocateBuffer(size_t size, const std::string& component_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // ... allocation logic
    
    if (memory_pressure_detected) {
        notifyPressureCallbacks(); // Callbacks might call back into this class!
    }
}
```

**Identified Issues:**
- Callbacks invoked while holding internal locks
- Risk of reentrancy and deadlock through callback chains
- No protection against callback reentrancy

### 4. Surface Class (`src/surface.cpp`)

**Lock Pattern:**
- Uses SDL surface locking: `SDL_LockSurface()` / `SDL_UnlockSurface()`

**Current Pattern:**
```cpp
// SDL locking pattern:
void Surface::put_pixel(int x, int y, Uint32 color) {
    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }
    // ... pixel operations
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
}
```

**Identified Issues:**
- Multiple drawing methods each acquire SDL locks
- Complex drawing operations might call simpler drawing methods
- Potential for nested SDL lock acquisition

## Deadlock Scenarios Identified

### Scenario 1: Audio Class Internal Method Calls
```cpp
// Deadlock scenario:
std::unique_ptr<Stream> Audio::setStream(std::unique_ptr<Stream> new_stream) {
    std::lock_guard<std::mutex> stream_lock(m_stream_mutex);
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // If implementation needs to check if finished:
    if (isFinished()) {  // DEADLOCK! isFinished() tries to lock m_buffer_mutex again
        // ...
    }
}
```

### Scenario 2: IOHandler Memory Tracking
```cpp
// Potential deadlock in I/O operations:
size_t IOHandler::read(void* buffer, size_t size, size_t count) {
    std::shared_lock<std::shared_mutex> op_lock(m_operation_mutex);
    
    // During read, memory tracking is updated:
    updateMemoryUsage(); // Acquires s_memory_mutex
    
    // If another thread calls getMemoryStats() while holding s_memory_mutex
    // and then tries to perform I/O, deadlock occurs
}
```

### Scenario 3: MemoryPoolManager Callback Reentrancy
```cpp
// Callback reentrancy deadlock:
uint8_t* MemoryPoolManager::allocateBuffer(size_t size, const std::string& component_name) {
    std::lock_guard<std::mutex> lock(m_mutex);  // Lock acquired
    
    if (memory_pressure) {
        notifyPressureCallbacks(); // Callback invoked while locked
        // If callback calls allocateBuffer() or releaseBuffer(), deadlock!
    }
}
```

### Scenario 4: Surface Complex Drawing Operations
```cpp
// SDL surface lock nesting:
void Surface::complex_drawing_operation() {
    SDL_LockSurface(surface);  // Lock acquired
    
    // Complex operation calls simpler operations:
    put_pixel(x, y, color);    // Tries to lock surface again - potential issue
    
    SDL_UnlockSurface(surface);
}
```

## Lock Ordering Requirements

To prevent deadlocks, establish this lock acquisition order:

1. **Global/Static locks** (highest priority)
   - `IOHandler::s_memory_mutex`

2. **Manager-level locks**
   - `MemoryPoolManager::m_mutex`

3. **Instance-level locks** (acquire in alphabetical order)
   - `Audio::m_buffer_mutex`
   - `Audio::m_player_mutex`
   - `Audio::m_stream_mutex`
   - `IOHandler::m_operation_mutex`

4. **System locks** (lowest priority)
   - SDL surface locks

## Recommended Threading Safety Pattern

### Public/Private Lock Pattern

Every class with locks should follow this pattern:

```cpp
class ThreadSafeClass {
public:
    // Public API - acquires locks and calls private implementation
    ReturnType publicMethod(params) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return publicMethod_unlocked(params);
    }
    
private:
    // Private implementation - assumes locks are already held
    ReturnType publicMethod_unlocked(params) {
        // Actual implementation here
        // Can safely call other _unlocked methods
    }
    
    mutable std::mutex m_mutex;
};
```

### Benefits of This Pattern

1. **Deadlock Prevention**: Internal method calls use unlocked versions
2. **Clear Separation**: Lock acquisition logic separated from business logic
3. **Testability**: Private methods can be tested independently
4. **Maintainability**: Clear contract about lock state

## Implementation Priority

Based on criticality and complexity:

1. **Audio Class** - Highest priority (affects playback)
2. **IOHandler Class** - High priority (affects I/O performance)
3. **MemoryPoolManager** - Medium priority (affects memory management)
4. **Surface Class** - Lower priority (affects UI responsiveness)

## Validation Strategy

Each refactored class should include:

1. **Unit tests** for concurrent access to public methods
2. **Deadlock prevention tests** that would fail with old implementation
3. **Performance tests** to ensure minimal overhead
4. **Integration tests** with other threaded components

## Static Analysis Integration

The threading safety analysis script (`scripts/analyze_threading_safety.py`) should be:

1. Run as part of the build process
2. Integrated into code review workflows
3. Updated to detect new anti-patterns as they're identified
4. Used to validate that refactoring follows the established patterns