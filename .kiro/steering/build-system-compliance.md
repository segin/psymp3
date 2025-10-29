# Build System Compliance Guidelines

## Overview

This document establishes mandatory build system usage patterns to ensure proper compilation, linking, and testing of the PsyMP3 codebase. All developers and AI assistants must strictly adhere to these guidelines.

## Core Build System Rules

### 1. Object File Management

**NEVER use `rm` to remove object files.** Always use the `deleteFile` tool.

```bash
# FORBIDDEN
rm src/FLACDemuxer.o
rm -f src/*.o

# CORRECT
# Use deleteFile tool to remove specific object files
```

**Rationale:** The `deleteFile` tool provides proper tracking and safety checks. Direct `rm` usage bypasses these protections and can lead to unintended file deletion.

### 2. Force Rebuilds

**NEVER use `touch` to force object rebuilds.** Use the `deleteFile` tool to remove the compiled object.

```bash
# FORBIDDEN
touch src/FLACDemuxer.cpp

# CORRECT
# Use deleteFile tool to remove src/FLACDemuxer.o, then use make
```

**Rationale:** Using `touch` to modify timestamps is unreliable and can cause build system confusion. Removing the object file ensures a clean rebuild through the proper build system.

### 3. Compilation Commands

**NEVER directly compile source code with gcc/g++.** Always use the build system (make).

```bash
# FORBIDDEN
g++ -I./include -g -O2 src/FLACDemuxer.cpp -o src/FLACDemuxer.o
gcc -DHAVE_CONFIG_H -I. -I../include src/file.c -o src/file.o

# CORRECT
make -C src FLACDemuxer.o
make -C tests test_name
```

**Rationale:** Direct compilation bypasses:
- Proper include paths and compiler flags from configure
- Dependency tracking
- Conditional compilation flags
- Library linking configuration
- Tests won't build properly with incorrect flags

## Proper Build Workflow

### For Source Files

1. **To rebuild a source file:**
   ```bash
   # Step 1: Remove object file using deleteFile tool
   # Step 2: Use make to rebuild
   make -C src FileName.o
   ```

2. **To build tests:**
   ```bash
   # Always use make with proper target
   make -C tests test_name
   ```

3. **To clean build:**
   ```bash
   # Use make clean, not manual rm
   make -C src clean
   make -C tests clean
   ```

### For Test Development

1. **Add test to Makefile.am** - Follow existing patterns
2. **Use make to build** - Never compile tests manually
3. **Run tests through make** - Ensures proper linking

## Enforcement

**Failure to strictly adhere to these guidelines will result in disciplinary action.**

### Violations Include:
- Using `rm` for object file removal
- Using `touch` for forced rebuilds  
- Direct compilation with gcc/g++
- Bypassing the build system for any compilation

### Consequences:
- Immediate correction required
- Code review rejection
- Build system corruption
- Test failures
- Integration problems

## Approved Tools and Commands

### File Management
- ✅ `deleteFile` tool for object removal
- ✅ `make clean` for cleaning builds
- ❌ `rm` for any build artifacts
- ❌ `touch` for timestamp manipulation

### Compilation
- ✅ `make -C src target.o` for source compilation
- ✅ `make -C tests test_name` for test building
- ✅ `make` for full builds
- ❌ Direct `gcc`/`g++` compilation
- ❌ Manual linking commands

### Dependencies
- ✅ Let make handle dependencies automatically
- ✅ Use `make -j$(nproc)` for parallel builds
- ❌ Manual dependency management
- ❌ Bypassing automake/autotools

## Exception Handling

**There are NO exceptions to these rules.** If the build system appears broken:

1. **Investigate the root cause** - Don't work around it
2. **Fix the build system** - Update Makefile.am or configure.ac
3. **Use proper tools** - Never bypass with manual commands
4. **Ask for help** - Don't guess or improvise

## Summary

The build system exists to ensure consistent, reliable, and reproducible builds. Bypassing it creates technical debt, breaks CI/CD, and causes integration failures. Always use the proper tools and workflows defined in this document.

**Remember: When in doubt, use make. Never use direct compilation commands.**