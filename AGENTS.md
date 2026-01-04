# AGENTS.md

> **Purpose**: This file provides context and directives for AI agents working on the PsyMP3 codebase.
> **Humans**: Please refer to [README.md](README.md) for build instructions and usage, and [ARCHITECTURE.md](ARCHITECTURE.md) for system design.

## 1. Project Identity
**PsyMP3** is a cross-platform (Linux/Windows) C++17 audio player with FFT visualization (v2.x rewrite).
- **License**: ISC
- **Maintainer**: Kirn Gill II <segin2005@gmail.com>

## 2. Agent Directives

### Documentation & Architecture
*   **Source of Truth**:
    *   [README.md](README.md): Build instructions, dependencies, usage, and end-user info.
    *   [ARCHITECTURE.md](ARCHITECTURE.md): System design, component layout, and logic flow.
    *   [TESTING.md](TESTING.md): comprehensive testing guide.
*   **Update Rule**: You **MUST** read and update `ARCHITECTURE.md` whenever you make structural changes, add components, or modify the data pipeline. Keep it synchronized with the code.

### Build System
*   **Tool**: ALWAYS use `make`. Never use direct `gcc`/`g++` or `touch`.
*   **Cleanliness**: Prefer `make clean` over manual `rm` for object files.
*   **Output**: **NEVER** pipe build output to pagers (`less`, `tail`). Always capture full output.
*   **Standard Build**: `./autogen.sh && ./configure && make -j$(nproc)`
*   **Testing Build**: `./configure --enable-rapidcheck && make check`

### Version Control
*   **Atomic Commits**: ONE test unit or logical change per commit. Even for cross-cutting concerns (e.g., copyright updates, header cleanups), split commits by module (e.g., "Codecs: ...", "Core: ...") rather than bulk application.
*   **Workflow**: `git commit -m "Topic: Description" && git push` after each distinct unit of work.
*   **Renaming**: ALWAYS use `git mv`.

### Code Quality Rules
*   **Threading**: Follow **Public/Private Lock Pattern**.
    *   Public: Acquire lock -> call `_unlocked`.
    *   Private: `_unlocked` suffix, assume lock held.
*   **Error Handling**: Throw exceptions with context. Do not use error codes.
*   **Resource Management**: RAII everywhere.

## 3. Tooling & Configuration
*   **Sanitizers**: Use `./configure --enable-asan` (Address), `--enable-ubsan` (UndefinedBehavior), or `--enable-tsan` (Thread) for verification.
*   **Fuzzing**: Property-based tests use RapidCheck (`tests/*.cpp`).

## 4. Context Pointers
*   **Architecture**: See [ARCHITECTURE.md](ARCHITECTURE.md).
*   **Detailed Testing**: See [TESTING.md](TESTING.md).
*   **Build Options**: See [README.md](README.md).