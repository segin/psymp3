# Threading Safety Framework Usage Guide

## Overview

This document describes how to use the threading safety analysis and testing framework created for the PsyMP3 threading safety audit. The framework consists of three main components:

1. **Static Analysis Script** - Identifies threading issues in the codebase
2. **Threading Safety Documentation** - Documents current patterns and issues
3. **Testing Framework** - Validates threading safety implementations

## Components

### 1. Static Analysis Script (`scripts/analyze_threading_safety.py`)

**Purpose**: Automatically analyze C++ code to identify classes with mutex usage and potential threading issues.

**Usage**:
```bash
# Analyze specific directories
python3 scripts/analyze_threading_safety.py src include

# Analyze all default directories
python3 scripts/analyze_threading_safety.py
```

**Output**: Generates `threading_safety_analysis.md` with detailed findings.

**What it detects**:
- Classes with mutex/lock members
- Public methods that acquire locks
- Missing private unlocked method counterparts
- Multiple mutex usage (potential lock ordering issues)

### 2. Threading Safety Documentation

**Files**:
- `docs/threading-safety-patterns.md` - Current patterns and deadlock scenarios
- `threading_safety_analysis.md` - Generated analysis report

**Key Information**:
- Current lock acquisition patterns in major classes
- Identified deadlock scenarios
- Recommended public/private lock pattern
- Lock ordering requirements
- Implementation priorities

### 3. Testing Framework (`tests/test_framework_threading.h`)

**Purpose**: Provides utilities for testing thread safety and validating the public/private lock pattern.

**Key Classes**:
- `ThreadSafetyTestBase` - Base class for threading tests
- `ConcurrentAccessTest` - Tests concurrent access to public methods
- `DeadlockDetectionTest` - Detects deadlock scenarios
- `StressTest` - High-concurrency stress testing
- `ThreadingTestRunner` - Runs multiple tests and reports results

## Using the Testing Framework

### Basic Concurrent Access Test

```cpp
#include "test_framework_threading.h"

// Test concurrent access to a class
MyClass obj;
auto test = ThreadingTest::ConcurrentAccessTest<MyClass>(
    &obj,
    [](MyClass* o, int thread_id) {
        // Operations to test concurrently
        o->publicMethod();
    }
);

auto results = test.run();
if (results.success) {
    std::cout << "Test passed!" << std::endl;
}
```

### Deadlock Detection Test

```cpp
// Test for potential deadlocks
auto deadlock_test = ThreadingTest::DeadlockDetectionTest<MyClass>(
    &obj,
    [](MyClass* o, int thread_id) {
        // Operations that might cause deadlock
        o->methodThatCallsOtherMethods();
    }
);

auto results = deadlock_test.run();
// If this hangs, there's a deadlock issue
```

### Stress Testing

```cpp
// Multiple operations stress test
std::vector<std::function<void(MyClass*, int)>> operations = {
    [](MyClass* o, int tid) { o->method1(); },
    [](MyClass* o, int tid) { o->method2(); },
    [](MyClass* o, int tid) { o->method3(); }
};

auto stress_test = ThreadingTest::StressTest<MyClass>(&obj, operations);
auto results = stress_test.run();
```

### Test Runner for Multiple Tests

```cpp
ThreadingTest::ThreadingTestRunner runner;

runner.addTest(std::make_unique<ThreadingTest::ConcurrentAccessTest<MyClass>>(...));
runner.addTest(std::make_unique<ThreadingTest::DeadlockDetectionTest<MyClass>>(...));

bool all_passed = runner.runAllTests();
```

## Workflow for Implementing Threading Safety

### Step 1: Analyze Current Code

```bash
# Run static analysis
python3 scripts/analyze_threading_safety.py src include

# Review the generated report
cat threading_safety_analysis.md
```

### Step 2: Create Baseline Tests

Before refactoring, create tests that demonstrate current behavior:

```cpp
// Create tests for the class you're about to refactor
void testCurrentImplementation() {
    MyClass obj;
    
    // Test basic concurrent access
    auto test = ThreadingTest::ConcurrentAccessTest<MyClass>(
        &obj,
        [](MyClass* o, int tid) {
            o->existingPublicMethod();
        }
    );
    
    auto results = test.run();
    // Document current performance and behavior
}
```

### Step 3: Implement Public/Private Lock Pattern

Refactor the class to follow the recommended pattern:

```cpp
class MyClass {
public:
    // Public API - acquires locks
    ReturnType publicMethod(params) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return publicMethod_unlocked(params);
    }
    
private:
    // Private implementation - assumes locks held
    ReturnType publicMethod_unlocked(params) {
        // Actual implementation
        // Can safely call other _unlocked methods
    }
    
    mutable std::mutex m_mutex;
};
```

### Step 4: Create Validation Tests

Test the refactored implementation:

```cpp
void testRefactoredImplementation() {
    MyClass obj;
    
    // Test that would have caused deadlock before
    auto deadlock_test = ThreadingTest::DeadlockDetectionTest<MyClass>(
        &obj,
        [](MyClass* o, int tid) {
            // This should now work without deadlock
            o->publicMethodThatCallsOtherMethods();
        }
    );
    
    auto results = deadlock_test.run();
    assert(results.success);
}
```

### Step 5: Performance Validation

Ensure refactoring doesn't significantly impact performance:

```cpp
void benchmarkPerformance() {
    MyClass obj;
    
    {
        ThreadingTest::PerformanceBenchmark bench("MyClass::publicMethod");
        for (int i = 0; i < 10000; ++i) {
            obj.publicMethod();
        }
    }
    
    // Compare with baseline measurements
}
```

## Test Configuration

Customize test behavior with `TestConfig`:

```cpp
ThreadingTest::TestConfig config;
config.num_threads = 16;                    // More threads for stress testing
config.operations_per_thread = 5000;       // More operations per thread
config.timeout = std::chrono::seconds(30);  // Longer timeout for complex tests
config.enable_stress_testing = true;       // Enable additional stress scenarios

auto test = ThreadingTest::ConcurrentAccessTest<MyClass>(&obj, operation, config);
```

## Integration with Build System

### Makefile Integration

Add threading safety tests to your Makefile:

```makefile
# Threading safety tests
test_threading_safety_baseline: tests/test_threading_safety_baseline.cpp tests/test_framework_threading.h
	$(CXX) $(CXXFLAGS) -pthread -I. -Iinclude $< -o $@

# Run threading analysis
analyze_threading:
	python3 scripts/analyze_threading_safety.py src include

# Run all threading tests
test_threading: test_threading_safety_baseline
	./test_threading_safety_baseline

.PHONY: analyze_threading test_threading
```

### Continuous Integration

Add to your CI pipeline:

```bash
# Static analysis
python3 scripts/analyze_threading_safety.py src include

# Compile and run threading tests
make test_threading

# Check for new threading issues
if grep -q "⚠️" threading_safety_analysis.md; then
    echo "New threading issues detected!"
    exit 1
fi
```

## Best Practices

### 1. Test Early and Often
- Create baseline tests before refactoring
- Test each class individually before integration
- Run tests on different hardware configurations

### 2. Use Appropriate Test Types
- **ConcurrentAccessTest**: Basic thread safety validation
- **DeadlockDetectionTest**: Verify deadlock prevention
- **StressTest**: High-load scenarios
- **Performance benchmarks**: Ensure acceptable overhead

### 3. Document Lock Ordering
Always document the lock acquisition order in your classes:

```cpp
class MyClass {
    // Lock acquisition order:
    // 1. s_global_mutex (if needed)
    // 2. m_primary_mutex
    // 3. m_secondary_mutex
private:
    static std::mutex s_global_mutex;
    mutable std::mutex m_primary_mutex;
    mutable std::mutex m_secondary_mutex;
};
```

### 4. Validate with Real Workloads
Create tests that simulate actual usage patterns:

```cpp
void testRealWorldScenario() {
    // Simulate actual audio playback scenario
    Audio audio;
    IOHandler io;
    
    auto test = ThreadingTest::ConcurrentAccessTest<Audio>(
        &audio,
        [&io](Audio* a, int tid) {
            // Simulate real operations
            a->setStream(io.createStream());
            while (!a->isFinished()) {
                a->getBufferLevel();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    );
    
    auto results = test.run();
    assert(results.success);
}
```

## Troubleshooting

### Test Hangs
If tests hang, it usually indicates a deadlock:
1. Check lock acquisition order
2. Verify all locks are properly released
3. Look for missing unlocked method calls

### High Error Rates
If tests show many errors:
1. Check for race conditions in shared data
2. Verify atomic operations are used correctly
3. Ensure proper exception safety

### Performance Issues
If refactoring causes performance problems:
1. Profile lock contention
2. Consider lock-free alternatives for hot paths
3. Optimize critical sections to be as short as possible

## Example: Complete Class Refactoring

See `tests/test_threading_safety_baseline.cpp` for complete examples of:
- Mock classes demonstrating current issues
- Baseline testing before refactoring
- Performance benchmarking
- Integration with the testing framework

This provides a template for refactoring real classes in the PsyMP3 codebase.