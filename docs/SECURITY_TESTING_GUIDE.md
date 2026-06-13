# FLAC Decoder Security Testing Guide

## Overview

This guide provides comprehensive instructions for security testing the native FLAC decoder implementation. It covers sanitizer builds, fuzz testing, and malformed input testing.

## Quick Start

### 1. AddressSanitizer Testing (Memory Errors)

```bash
# Configure with AddressSanitizer
./configure --enable-asan --enable-native-flac

# Build
make -j$(nproc)

# Run tests
make -C tests check

# Run specific test
./tests/test_flac_codec_unit_comprehensive
```

**What it detects:**
- Buffer overflows
- Use-after-free
- Memory leaks
- Double-free
- Invalid memory access

### 2. UndefinedBehaviorSanitizer Testing (Undefined Behavior)

```bash
# Configure with UBSanitizer
./configure --enable-ubsan --enable-native-flac

# Build
make -j$(nproc)

# Run tests
make -C tests check
```

**What it detects:**
- Integer overflow
- Division by zero
- Shift errors
- Null pointer dereference
- Out-of-bounds array access
- Invalid enum values

### 3. ThreadSanitizer Testing (Thread Safety)

```bash
# Configure with ThreadSanitizer
./configure --enable-tsan --enable-native-flac

# Build
make -j$(nproc)

# Run tests
make -C tests check
```

**What it detects:**
- Data races
- Deadlocks
- Use-after-free in multithreaded context
- Mutex errors

### 4. Fuzz Testing

```bash
# Build fuzz target
./tests/run_security_tests.sh fuzz

# Run fuzzer (60 second timeout)
./build-security/fuzz_flac_decoder corpus/ -max_len=1000000

# Run with AFL++ (if installed)
afl-fuzz -i corpus -o findings ./build-security/fuzz_flac_decoder
```

## Detailed Testing Procedures

### AddressSanitizer (ASAN) Testing

#### Setup
```bash
./configure --enable-asan --enable-native-flac
make clean
make -j$(nproc)
```

#### Running Tests
```bash
# Run all tests
make -C tests check

# Run specific test
./tests/test_flac_codec_unit_comprehensive

# Run with verbose output
ASAN_OPTIONS=verbosity=2 ./tests/test_flac_codec_unit_comprehensive
```

#### Interpreting Results
```
=================================================================
==12345==ERROR: AddressSanitizer: buffer-overflow on unknown address
    #0 0x... in BitstreamReader::readBits(...)
    #1 0x... in FrameParser::parseFrameHeader(...)
    ...
```

**Common Issues:**
- `buffer-overflow`: Array access out of bounds
- `use-after-free`: Accessing freed memory
- `heap-use-after-free`: Using heap memory after free
- `SEGV on unknown address`: Null pointer or invalid memory

#### Fixing Issues
1. Identify the function and line number
2. Check array bounds and pointer validity
3. Add bounds checking if needed
4. Re-run test to verify fix

### UndefinedBehaviorSanitizer (UBSAN) Testing

#### Setup
```bash
./configure --enable-ubsan --enable-native-flac
make clean
make -j$(nproc)
```

#### Running Tests
```bash
# Run all tests
make -C tests check

# Run specific test
./tests/test_flac_codec_unit_comprehensive

# Run with verbose output
UBSAN_OPTIONS=verbosity=2 ./tests/test_flac_codec_unit_comprehensive
```

#### Interpreting Results
```
runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
    at BitstreamReader.cpp:123:45
```

**Common Issues:**
- `signed integer overflow`: Integer overflow in arithmetic
- `division by zero`: Division or modulo by zero
- `shift exponent`: Shift amount out of range
- `index out of bounds`: Array index out of range

#### Fixing Issues
1. Identify the operation and line number
2. Check for overflow conditions
3. Use safe arithmetic (e.g., check before adding)
4. Re-run test to verify fix

### ThreadSanitizer (TSAN) Testing

#### Setup
```bash
./configure --enable-tsan --enable-native-flac
make clean
make -j$(nproc)
```

#### Running Tests
```bash
# Run threading tests
./tests/test_flac_codec_threading_safety

# Run with verbose output
TSAN_OPTIONS=verbosity=2 ./tests/test_flac_codec_threading_safety
```

#### Interpreting Results
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T1:
    #0 0x... in NativeFLACCodec::decode_unlocked(...)
  Previous read of size 4 at 0x... by thread T2:
    #0 0x... in NativeFLACCodec::canDecode_unlocked(...)
```

**Common Issues:**
- `data race`: Concurrent access to shared variable
- `deadlock`: Threads waiting for each other
- `use-after-free`: Thread accessing freed memory

#### Fixing Issues
1. Identify the shared variable and threads
2. Add synchronization (mutex, lock)
3. Ensure consistent lock ordering
4. Re-run test to verify fix

### Fuzz Testing with libFuzzer

#### Building Fuzz Target
```bash
# Automatic build
./tests/run_security_tests.sh fuzz

# Manual build
clang++ -fsanitize=fuzzer,address -fno-omit-frame-pointer \
    -I./include -I./src -std=c++17 \
    tests/fuzz_flac_decoder.cpp \
    src/codecs/flac/*.cpp \
    -o fuzz_flac_decoder
```

#### Running Fuzzer
```bash
# Create corpus directory
mkdir -p corpus
cp tests/data/*.flac corpus/

# Run fuzzer
./fuzz_flac_decoder corpus/ -max_len=1000000

# Run with specific options
./fuzz_flac_decoder corpus/ \
    -max_len=1000000 \
    -timeout=5 \
    -artifact_prefix=crashes/ \
    -max_total_time=3600
```

#### Interpreting Results
```
#123456 NEW    cov: 1234 ft: 5678 corp: 42/1.2MB lim: 1000000 len: 12345 exp: 0
#123457 REDUCE cov: 1234 ft: 5678 corp: 42/1.2MB lim: 1000000 len: 12340 exp: 0
#123458 CRASH  cov: 1234 ft: 5678 corp: 42/1.2MB lim: 1000000 len: 12345 exp: 0
```

**Output Meanings:**
- `NEW`: Found new code coverage
- `REDUCE`: Found smaller input with same coverage
- `CRASH`: Found input that crashes decoder
- `LEAK`: Found memory leak
- `TIMEOUT`: Input causes excessive CPU usage

#### Handling Crashes
```bash
# Reproduce crash
./fuzz_flac_decoder crash-file

# Debug crash
gdb ./fuzz_flac_decoder
(gdb) run crash-file

# Minimize crash
./fuzz_flac_decoder crash-file -minimize_crash=1
```

### Fuzz Testing with AFL++

#### Installation
```bash
# Ubuntu/Debian
sudo apt-get install afl++

# macOS
brew install afl-plus-plus

# From source
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make
sudo make install
```

#### Building with AFL++
```bash
# Build with AFL++
afl-clang++ -fsanitize=address -fno-omit-frame-pointer \
    -I./include -I./src -std=c++17 \
    tests/fuzz_flac_decoder.cpp \
    src/codecs/flac/*.cpp \
    -o fuzz_flac_decoder_afl
```

#### Running AFL++
```bash
# Create corpus
mkdir -p corpus findings
cp tests/data/*.flac corpus/

# Run fuzzer
afl-fuzz -i corpus -o findings ./fuzz_flac_decoder_afl

# Run with multiple cores
afl-fuzz -i corpus -o findings -M master ./fuzz_flac_decoder_afl
afl-fuzz -i corpus -o findings -S slave1 ./fuzz_flac_decoder_afl
afl-fuzz -i corpus -o findings -S slave2 ./fuzz_flac_decoder_afl
```

#### Analyzing Results
```bash
# View crash files
ls -la findings/crashes/

# Reproduce crash
./fuzz_flac_decoder_afl < findings/crashes/id:000000,sig:11

# Minimize crash
afl-tmin -i findings/crashes/id:000000,sig:11 \
    -o crash_min ./fuzz_flac_decoder_afl
```

## Malformed Input Testing

### Creating Test Cases

```bash
# Invalid sync pattern
printf '\x00\x00\x00\x00' > invalid_sync.flac

# Truncated frame
printf '\xff\xf8\x00\x00' > truncated.flac

# Invalid block size (0)
printf '\xff\xf8\x00\x00\x00\x00' > invalid_blocksize.flac

# Invalid sample rate (0xF)
printf '\xff\xf8\x0f\x00' > invalid_samplerate.flac

# Invalid channel count (0)
printf '\xff\xf8\x00\x00' > invalid_channels.flac

# Invalid bit depth (0)
printf '\xff\xf8\x00\x00' > invalid_bitdepth.flac
```

### Testing with Malformed Files

```bash
# Test with AddressSanitizer
ASAN_OPTIONS=halt_on_error=1 ./tests/test_flac_codec_unit_comprehensive

# Test with UBSanitizer
UBSAN_OPTIONS=halt_on_error=1 ./tests/test_flac_codec_unit_comprehensive

# Test with custom malformed file
./fuzz_flac_decoder invalid_sync.flac
```

## Continuous Integration

### GitHub Actions Example

```yaml
name: Security Tests

on: [push, pull_request]

jobs:
  security:
    runs-on: ubuntu-latest
    
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y clang llvm
      
      - name: AddressSanitizer
        run: |
          ./configure --enable-asan --enable-native-flac
          make -j$(nproc)
          make -C tests check
      
      - name: UBSanitizer
        run: |
          ./configure --enable-ubsan --enable-native-flac
          make -j$(nproc)
          make -C tests check
      
      - name: Fuzz Testing
        run: |
          ./tests/run_security_tests.sh fuzz
```

## Performance Considerations

### Sanitizer Overhead

| Sanitizer | CPU Overhead | Memory Overhead | Recommended Use |
|-----------|-------------|-----------------|-----------------|
| ASAN | 2-3x | 2-3x | Development, CI |
| UBSAN | 1-2x | 1x | Development, CI |
| TSAN | 5-10x | 5-10x | Specific testing |
| None | 1x | 1x | Production |

### Fuzz Testing Duration

- **Quick test**: 1 minute (basic coverage)
- **Standard test**: 1 hour (good coverage)
- **Thorough test**: 24+ hours (comprehensive coverage)
- **Continuous**: Run continuously in background

## Troubleshooting

### Build Failures

**Problem**: Sanitizer flags not recognized
```bash
# Solution: Update compiler
sudo apt-get install -y clang-14
export CC=clang-14
export CXX=clang++-14
./configure --enable-asan
```

**Problem**: Linking errors with sanitizers
```bash
# Solution: Ensure all files compiled with same flags
make clean
./configure --enable-asan
make -j$(nproc)
```

### Test Failures

**Problem**: False positives from sanitizers
```bash
# Solution: Suppress known false positives
ASAN_OPTIONS=suppressions=asan.supp ./tests/test_name
```

**Problem**: Fuzz testing timeout
```bash
# Solution: Increase timeout
./fuzz_flac_decoder corpus/ -timeout=10
```

## Best Practices

1. **Run sanitizers regularly**
   - Enable in CI/CD pipeline
   - Run before each commit
   - Fix issues immediately

2. **Fuzz continuously**
   - Run 24/7 in background
   - Monitor for crashes
   - Minimize and analyze crashes

3. **Test with real data**
   - Use actual FLAC files
   - Test various encoders
   - Test edge cases

4. **Document findings**
   - Record all crashes
   - Track fixes
   - Update security audit

5. **Keep tools updated**
   - Update sanitizers
   - Update fuzzer
   - Update test corpus

## References

- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [UndefinedBehaviorSanitizer Documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [ThreadSanitizer Documentation](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer/)
- [AFL++ Documentation](https://aflplusplus.github.io/)
- [RFC 9639 - FLAC Specification](https://tools.ietf.org/html/rfc9639)

## Support

For issues or questions:
1. Check the security audit document
2. Review test output carefully
3. Consult RFC 9639 for format details
4. File bug reports with minimal test case

</content>
