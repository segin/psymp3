# Threading Safety Maintenance Guide

This document provides guidance for maintaining threading safety as the PsyMP3 codebase evolves, ensuring that the threading safety patterns established during the refactoring continue to be followed.

## Overview

The PsyMP3 codebase has been refactored to follow a consistent **Public/Private Lock Pattern** to prevent deadlocks and ensure thread safety. This guide explains how to maintain these patterns and validate threading safety in ongoing development.

## Core Threading Safety Principles

### 1. Public/Private Lock Pattern

Every class that uses synchronization primitives MUST follow this pattern:

- **Public methods**: Handle lock acquisition and call private `_unlocked` implementations
- **Private methods**: Append `_unlocked` suffix and perform work without acquiring locks  
- **Internal calls**: Always use `_unlocked` versions when calling methods within the same class

### 2. Lock Acquisition Order

When multiple locks are involved, always acquire them in the documented order:

1. Global/Static locks (e.g., `IOHandler::s_memory_mutex`)
2. Manager-level locks (e.g., `MemoryPoolManager::m_mutex`)
3. Instance-level locks (e.g., `Audio::m_stream_mutex`, `Audio::m_buffer_mutex`)
4. System locks (e.g., SDL surface locks)

### 3. Exception Safety

Always use RAII lock guards (`std::lock_guard`, `std::unique_lock`) to ensure locks are released even when exceptions occur.

## Development Workflow

### Before Making Changes

1. **Review Threading Guidelines**: Read `.kiro/steering/threading-safety-guidelines.md`
2. **Check Existing Patterns**: Examine similar classes to understand the established patterns
3. **Plan Lock Strategy**: If adding new locks, document the acquisition order

### During Development

1. **Follow Naming Conventions**: Use `_unlocked` suffix for private methods
2. **Document Lock Order**: Add comments documenting lock acquisition order
3. **Use RAII Guards**: Never use manual lock/unlock
4. **Avoid Callbacks Under Lock**: Never invoke callbacks while holding internal locks

### After Making Changes

1. **Run Static Analysis**: Use the threading safety analysis tools
2. **Run Threading Tests**: Execute the threading safety test suite
3. **Performance Check**: Verify no significant performance regression
4. **Code Review**: Have changes reviewed by someone familiar with threading patterns

## Validation Tools and Scripts

### Static Analysis Tools

#### Threading Pattern Checker
```bash
# Check specific file
python3 scripts/check_threading_patterns.py --fix-suggestions src/MyClass.cpp

# Check entire directory
python3 scripts/check_threading_patterns.py --strict src/

# Generate report
python3 scripts/check_threading_patterns.py --output threading_issues.txt src/
```

#### Threading Safety Analyzer
```bash
# Analyze codebase
python3 scripts/analyze_threading_safety.py src include

# This generates threading_safety_analysis.md with detailed findings
```

### Build System Integration

#### Make Targets
```bash
# Basic threading safety check
make threading-check

# Strict checks (warnings as errors)
make threading-check-strict

# Full analysis with reports
make threading-analysis

# Run threading safety tests
make threading-check-tests

# Comprehensive validation
make threading-validation
```

#### CI Integration
```bash
# Quick CI check
scripts/ci_threading_safety_check.sh

# Comprehensive validation
scripts/run_comprehensive_threading_validation.sh
```

### Test Suite

#### Unit Tests
- `test_threading_safety_baseline` - Basic threading patterns
- `test_audio_thread_safety` - Audio component threading
- `test_iohandler_thread_safety_comprehensive` - I/O handler threading
- `test_memory_pool_manager_thread_safety_comprehensive` - Memory management threading
- `test_surface_thread_safety` - Surface/graphics threading

#### Integration Tests
- `test_system_wide_threading_integration` - Multi-component threading scenarios

#### Performance Tests
- `test_threading_performance_regression` - Performance impact validation

## Common Scenarios

### Adding a New Threaded Class

1. **Design the Lock Strategy**:
   ```cpp
   class MyThreadedClass {
   private:
       mutable std::mutex m_mutex;  // Use mutable for const methods
   };
   ```

2. **Implement Public/Private Pattern**:
   ```cpp
   public:
       ReturnType publicMethod(params) {
           std::lock_guard<std::mutex> lock(m_mutex);
           return publicMethod_unlocked(params);
       }
   
   private:
       ReturnType publicMethod_unlocked(params) {
           // Implementation here
           // Can safely call other _unlocked methods
       }
   ```

3. **Document Lock Order**:
   ```cpp
   // Lock acquisition order:
   // 1. MyThreadedClass::m_mutex
   ```

4. **Add Tests**:
   - Create unit tests for concurrent access
   - Add deadlock prevention tests
   - Include performance benchmarks

### Modifying Existing Threaded Classes

1. **Identify Current Pattern**: Check if class already follows public/private pattern
2. **Maintain Consistency**: Follow the existing lock acquisition order
3. **Update Tests**: Modify existing tests to cover new functionality
4. **Validate Changes**: Run full threading safety validation

### Adding New Dependencies Between Threaded Classes

1. **Review Lock Hierarchy**: Ensure new dependencies don't create circular lock dependencies
2. **Update Documentation**: Document any changes to lock acquisition order
3. **Test Interactions**: Create integration tests for the new dependencies
4. **Performance Impact**: Measure performance impact of additional locking

## Troubleshooting

### Common Threading Issues

#### Deadlock Detection
- **Symptoms**: Application hangs, threads waiting indefinitely
- **Diagnosis**: Use `gdb` with `thread apply all bt` to see thread stack traces
- **Prevention**: Follow lock acquisition order, use timeouts for debugging

#### Race Conditions
- **Symptoms**: Intermittent crashes, data corruption, inconsistent state
- **Diagnosis**: Use thread sanitizer (`-fsanitize=thread`)
- **Prevention**: Ensure all shared data access is protected by appropriate locks

#### Performance Issues
- **Symptoms**: Slower performance after threading changes
- **Diagnosis**: Run performance regression tests, profile with `perf`
- **Solutions**: Reduce lock contention, use lock-free alternatives where appropriate

### Debugging Tools

#### Thread Sanitizer
```bash
# Build with thread sanitizer
CXXFLAGS="-fsanitize=thread -g" LDFLAGS="-fsanitize=thread" ./configure
make clean && make

# Run tests
./tests/test_audio_thread_safety
```

#### Helgrind (Valgrind)
```bash
# Check for race conditions
valgrind --tool=helgrind ./tests/test_audio_thread_safety
```

#### GDB Threading Commands
```bash
# Attach to running process
gdb -p <pid>

# Show all threads
info threads

# Show all thread backtraces
thread apply all bt

# Switch to specific thread
thread <number>
```

## Continuous Integration

### GitHub Actions Integration

The threading safety validation is integrated into the CI pipeline via `.github/workflows/threading-safety.yml`:

- **Pull Requests**: Run critical threading safety checks
- **Main Branch**: Run comprehensive validation
- **Performance**: Check for performance regressions

### Pre-commit Hooks

Add to `.git/hooks/pre-commit`:
```bash
#!/bin/bash
# Run threading safety checks before commit
python3 scripts/check_threading_patterns.py --strict
```

### Code Review Checklist

When reviewing code changes that involve threading:

- [ ] Follows public/private lock pattern
- [ ] Uses RAII lock guards
- [ ] Documents lock acquisition order
- [ ] No callbacks invoked under locks
- [ ] Includes appropriate tests
- [ ] No performance regression

## Performance Considerations

### Lock Overhead Minimization

1. **Keep Critical Sections Small**: Minimize time spent holding locks
2. **Use Appropriate Lock Types**: Consider `shared_mutex` for read-heavy workloads
3. **Lock-Free Alternatives**: Use atomics for simple operations
4. **Batch Operations**: Group multiple operations under a single lock acquisition

### Performance Monitoring

1. **Baseline Measurements**: Establish performance baselines before changes
2. **Regular Benchmarking**: Run performance tests regularly
3. **Profile Critical Paths**: Use profiling tools to identify bottlenecks
4. **Monitor in Production**: Track performance metrics in production environments

## Future Considerations

### Scalability

As the codebase grows, consider:

1. **Lock Granularity**: Fine-grained locking for better concurrency
2. **Lock-Free Data Structures**: Where appropriate for high-performance paths
3. **Thread Pool Patterns**: For managing concurrent operations
4. **Async/Await Patterns**: For I/O-bound operations

### Maintenance Automation

1. **Static Analysis Integration**: Integrate threading checks into IDE/editor
2. **Automated Refactoring**: Tools to help maintain threading patterns
3. **Documentation Generation**: Auto-generate threading documentation from code
4. **Performance Regression Detection**: Automated performance monitoring

## Resources

### Documentation
- [Threading Safety Guidelines](.kiro/steering/threading-safety-guidelines.md)
- [Development Standards](.kiro/steering/development-standards.md)
- [Threading Safety Analysis Tools](threading-safety-analysis-tools.md)

### Tools and Scripts
- `scripts/check_threading_patterns.py` - Pattern validation
- `scripts/analyze_threading_safety.py` - Static analysis
- `scripts/ci_threading_safety_check.sh` - CI integration
- `scripts/run_comprehensive_threading_validation.sh` - Full validation

### Tests
- `tests/test_*_thread_safety.cpp` - Component-specific tests
- `tests/test_system_wide_threading_integration.cpp` - Integration tests
- `tests/test_threading_performance_regression.cpp` - Performance tests

## Contact and Support

For questions about threading safety patterns or issues with the validation tools:

1. Review this documentation and the threading safety guidelines
2. Check existing tests for examples
3. Run the static analysis tools for guidance
4. Consult with team members familiar with the threading patterns

Remember: When in doubt, err on the side of caution and follow the established patterns. The threading safety validation tools are there to help catch issues early in the development process.