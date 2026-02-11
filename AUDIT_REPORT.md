# Codebase Audit Report: PsyMP3

**Date:** 2024-10-16
**Repository:** PsyMP3
**Commit SHA:** (Current HEAD)
**Auditor:** Jules (AI Agent)

## 1. Executive Summary

A comprehensive audit was performed on the PsyMP3 codebase (C++17, Autotools). The application is a media player with strict threading requirements and modular codec architecture.

**Overall Health Score:** 65/100

**Top 5 Risks:**
1.  **Critical Correctness Failure**: Undefined Behavior (UB) in FLAC `SampleReconstructor.cpp` due to invalid bitwise shifts, potentially causing crashes or audio corruption.
2.  **Logic Errors**: "Dead" loops in `audio.cpp` where loop conditions are impossible to satisfy, indicating broken SIMD optimization logic.
3.  **Security Hardening**: The build system (`configure.ac`) lacks standard binary security flags (Stack Protector, PIE, RELRO), making the binary vulnerable to exploitation if memory corruption occurs.
4.  **Operational Risk**: Build environment is fragile; standard dependencies (SDL 1.2, TagLib) are required but not easily provisioned in modern CI/sandbox environments, and the build fails gracefully.
5.  **Data Protection**: Last.fm credentials (password) are stored in heap memory (std::string) without scrubbing/locking, posing a risk in case of core dumps.

---

## 2. Full Findings

### [ID-001] Critical Undefined Behavior in FLAC Decoder
*   **Severity:** Critical
*   **Confidence:** High
*   **File:** `src/codecs/flac/SampleReconstructor.cpp:116`
*   **Description:** The code performs `1 << (shift_amount - 1)` where `shift_amount` can be derived from untrusted input. If `shift_amount` is 0 or >= 32, this invokes Undefined Behavior (UB). Static analysis confirms a path where shifting by 4294967295 bits is possible.
*   **Impact:** Application crash, denial of service, or potentially arbitrary code execution depending on compiler optimization.
*   **Remediation:** Validate `shift_amount` before operation.

### [ID-002] Logical Dead Code / Broken Loops
*   **Severity:** High
*   **Confidence:** High
*   **File:** `src/audio.cpp:506`, `src/audio.cpp:549`
*   **Description:** Loops defined as `for (int x = simd_end; x < 512; x++)` are unreachable because `simd_end` is calculated to be 512. The fallback scalar processing for the last few samples is never executed.
*   **Impact:** Audio glitches or missing samples at the end of buffer blocks.
*   **Remediation:** Correct the `simd_end` calculation or loop logic.

### [ID-003] Missing Compiler Security Flags
*   **Severity:** High
*   **Confidence:** High
*   **File:** `configure.ac`
*   **Description:** The build configuration does not enable `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-fPIE`, or `-Wl,-z,relro,-z,now`.
*   **Impact:** Increased exploitability of potential buffer overflows.
*   **Remediation:** Add `AX_CHECK_COMPILE_FLAG` macros to enable these flags in `configure.ac`.

### [ID-004] Memory Safety in Audio Processing
*   **Severity:** Medium
*   **Confidence:** Medium
*   **File:** `src/audio.cpp:394`
*   **Description:** Unsafe C-style pointer cast `(int16_t *)buf` combined with potential buffer size mismatches.
*   **Impact:** Potential heap buffer overflow.
*   **Remediation:** Use `reinterpret_cast` or `std::span` (C++20) / bounds checking.

### [ID-005] Insecure Credential Handling
*   **Severity:** Medium
*   **Confidence:** Medium
*   **File:** `include/lastfm/LastFM.h`
*   **Description:** `m_password` is stored as a `std::string`.
*   **Impact:** Passwords remain in memory and may be swapped to disk or included in crash dumps.
*   **Remediation:** Use `mlock` or a secure string class that zeros memory on destruction; store only the hash if possible (code suggests hash is also stored).

---

## 3. Metrics

*   **Languages:** C++17 (Primary), Shell (Build)
*   **LOC:** ~76,500 (C++ Headers + Source)
*   **Test Coverage:** 0% (Runtime execution failed due to environment)
*   **Cyclomatic Complexity Hotspots:**
    *   `MemoryPoolManager::getOptimalBufferSize`
    *   `BufferPool` logic
*   **Secrets Found:** 0 hardcoded keys (clean).

## 4. Operational Readiness
*   **Build System:** Autotools (Fragile). Requires `autoconf-archive` which is often missing.
*   **Dependencies:** Legacy SDL 1.2 is a major deprecation risk.
*   **Testing:** Unit tests exist (`tests/`) but require the full build environment.
