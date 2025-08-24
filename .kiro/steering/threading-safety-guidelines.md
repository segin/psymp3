# Threading Safety Guidelines

## Overview

This document establishes mandatory threading safety patterns for the PsyMP3 codebase to prevent deadlocks and ensure thread-safe operation. All developers must follow these guidelines when working with multi-threaded code.

## Core Threading Pattern: Public/Private Lock Pattern

### The Problem

When public methods that acquire locks call other public methods that also acquire locks within the same class, deadlocks can occur. This is especially problematic when:

- A public method acquires a lock and then calls another public method that tries to acquire the same lock
- Multiple locks are acquired in different orders by different methods
- Callback functions are invoked while holding locks, and those callbacks call back into the original class

### The Solution: Public/Private Lock Pattern

Every class that uses synchronization primitives (mutexes, spinlocks, etc.) MUST follow this pattern:

1. **Public methods** provide the external API and handle lock acquisition
2. **Private methods** with `_unlocked` suffix perform the actual work without acquiring locks
3. **Internal calls** between methods use the private unlocked versions

## Implementation Guidelines

### 1. Method Structure

```cpp
class ThreadSafeClass {
public:
    // Public API - acquires locks and calls private implementation
    ReturnType publicMethod(parameters) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return publicMethod_unlocked(parameters);
    }
    
private:
    // Private implementation - assumes locks are already held
    ReturnType publicMethod_unlocked(parameters) {
        // Actual implementation here
        // Can safely call other _unlocked methods
        return result;
    }
    
    mutable std::mutex m_mutex;
};
```

### 2. Naming Conventions

- **Public methods**: Use standard descriptive names (e.g., `allocateBuffer`, `setStream`, `isFinished`)
- **Private unlocked methods**: Append `_unlocked` to the public method name (e.g., `allocateBuffer_unlocked`, `setStream_unlocked`, `isFinished_unlocked`)
- **Internal helper methods**: Use descriptive names without `_unlocked` suffix if they don't correspond to public methods

### 3. Lock Acquisition Order

To prevent deadlocks when multiple locks are involved, establish and document a consistent lock acquisition order:

```cpp
// MANDATORY: Document lock acquisition order at class level
// Lock acquisition order (to prevent deadlocks):
// 1. Global/Static locks (e.g., IOHandler::s_memory_mutex)
// 2. Manager-level locks (e.g., MemoryPoolManager::m_mutex)  
// 3. Instance-level locks (e.g., Audio::m_stream_mutex, Audio::m_buffer_mutex)
// 4. System locks (e.g., SDL surface locks)
```

### 4. Exception Safety

Always use RAII lock guards to ensure locks are released even when exceptions occur:

```cpp
// CORRECT: Use RAII lock guards
ReturnType publicMethod(parameters) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return publicMethod_unlocked(parameters);
}

// INCORRECT: Manual lock/unlock (exception unsafe)
ReturnType publicMethod(parameters) {
    m_mutex.lock();
    auto result = publicMethod_unlocked(parameters);
    m_mutex.unlock();  // May not execute if exception thrown
    return result;
}
```

## Specific Implementation Patterns

### 1. Single Mutex Classes

For classes with a single mutex:

```cpp
class SimpleThreadSafeClass {
public:
    void setValue(int value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        setValue_unlocked(value);
    }
    
    int getValue() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return getValue_unlocked();
    }
    
private:
    void setValue_unlocked(int value) {
        m_value = value;
        // Can call other _unlocked methods safely
        notifyObservers_unlocked();
    }
    
    int getValue_unlocked() const {
        return m_value;
    }
    
    void notifyObservers_unlocked() {
        // Implementation that doesn't acquire locks
    }
    
    mutable std::mutex m_mutex;
    int m_value;
};
```

### 2. Multiple Mutex Classes

For classes with multiple mutexes, always acquire in documented order:

```cpp
class MultiMutexClass {
public:
    void complexOperation() {
        // Acquire locks in documented order
        std::lock_guard<std::mutex> stream_lock(m_stream_mutex);
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        complexOperation_unlocked();
    }
    
private:
    void complexOperation_unlocked() {
        // Implementation assumes both locks are held
        updateStream_unlocked();
        updateBuffer_unlocked();
    }
    
    void updateStream_unlocked() { /* ... */ }
    void updateBuffer_unlocked() { /* ... */ }
    
    // Lock acquisition order documented:
    // 1. m_stream_mutex (acquired first)
    // 2. m_buffer_mutex (acquired second)
    mutable std::mutex m_stream_mutex;
    mutable std::mutex m_buffer_mutex;
};
```

### 3. Callback Safety Pattern

For classes that invoke callbacks, never call callbacks while holding internal locks:

```cpp
class CallbackSafeClass {
public:
    void operationWithCallback() {
        std::vector<CallbackInfo> pending_callbacks;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Do internal work
            performOperation_unlocked();
            // Prepare callbacks but don't invoke them yet
            pending_callbacks = preparePendingCallbacks_unlocked();
        }
        
        // Invoke callbacks after releasing locks
        for (const auto& callback : pending_callbacks) {
            callback.invoke();
        }
    }
    
private:
    void performOperation_unlocked() { /* ... */ }
    std::vector<CallbackInfo> preparePendingCallbacks_unlocked() { /* ... */ }
    
    mutable std::mutex m_mutex;
};
```

## Common Anti-Patterns to Avoid

### 1. Public Method Calling Public Method

```cpp
// INCORRECT: Deadlock risk
class BadExample {
public:
    void methodA() {
        std::lock_guard<std::mutex> lock(m_mutex);
        methodB();  // DEADLOCK: methodB tries to acquire same lock
    }
    
    void methodB() {
        std::lock_guard<std::mutex> lock(m_mutex);  // DEADLOCK HERE
        // implementation
    }
};

// CORRECT: Use private unlocked methods
class GoodExample {
public:
    void methodA() {
        std::lock_guard<std::mutex> lock(m_mutex);
        methodB_unlocked();  // Safe: no lock acquisition
    }
    
    void methodB() {
        std::lock_guard<std::mutex> lock(m_mutex);
        methodB_unlocked();
    }
    
private:
    void methodB_unlocked() {
        // implementation
    }
};
```

### 2. Inconsistent Lock Order

```cpp
// INCORRECT: Inconsistent lock acquisition order
void methodX() {
    std::lock_guard<std::mutex> lock1(m_mutex_a);
    std::lock_guard<std::mutex> lock2(m_mutex_b);  // Order: A then B
}

void methodY() {
    std::lock_guard<std::mutex> lock1(m_mutex_b);
    std::lock_guard<std::mutex> lock2(m_mutex_a);  // Order: B then A - DEADLOCK RISK
}
```

### 3. Callback Invocation Under Lock

```cpp
// INCORRECT: Callback invoked while holding lock
void badCallbackMethod() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // ... do work ...
    m_callback();  // DANGEROUS: callback might call back into this class
}
```

## Code Review Checklist

When reviewing code that involves threading, verify:

- [ ] Every public method that acquires locks has a corresponding `_unlocked` private method
- [ ] Internal method calls use the `_unlocked` versions
- [ ] Lock acquisition order is documented and consistently followed
- [ ] RAII lock guards are used instead of manual lock/unlock
- [ ] Callbacks are never invoked while holding internal locks
- [ ] No public method calls other public methods that acquire the same locks
- [ ] Mutable mutexes are used for const methods that need to acquire locks
- [ ] Exception safety is maintained (locks released even on exceptions)

## Testing Requirements

All threading-related code changes must include:

1. **Basic Thread Safety Tests**: Multiple threads calling the same public method
2. **Deadlock Prevention Tests**: Scenarios that would cause deadlocks with incorrect patterns
3. **Stress Tests**: High-concurrency scenarios to validate thread safety
4. **Performance Tests**: Ensure lock overhead doesn't significantly impact performance

## Static Analysis Integration

Where possible, use static analysis tools to detect threading anti-patterns:

- Public methods that acquire locks without corresponding `_unlocked` private methods
- Inconsistent lock acquisition orders
- Potential deadlock scenarios

## Migration Strategy for Existing Code

When refactoring existing threading code:

1. **Identify** all public methods that acquire locks
2. **Create** corresponding `_unlocked` private methods
3. **Update** public methods to call private `_unlocked` versions
4. **Fix** all internal method calls to use `_unlocked` versions
5. **Test** thoroughly with threading safety tests
6. **Document** lock acquisition order

## Performance Considerations

- The public/private pattern adds minimal overhead (one function call)
- Use `inline` for simple `_unlocked` methods if performance is critical
- Consider lock-free alternatives for high-frequency, simple operations
- Profile before and after refactoring to ensure acceptable performance

## Enforcement

- All new code MUST follow these threading safety guidelines
- Code reviews MUST verify compliance with threading patterns
- Build system integration SHOULD detect common threading anti-patterns where possible
- Threading safety violations are considered critical bugs and must be fixed immediately