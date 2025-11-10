---
inclusion: always
---

# Diagnostic Logging Requirements

## CRITICAL RULE: File-Based Logging Only

**YOU ARE FORBIDDEN TO USE STANDARD OUTPUT OR STANDARD ERROR TO COLLECT DIAGNOSTICS.**

PsyMP3 has a file-based error logging facility. **YOU MUST USE IT.**

## Correct Usage

When running PsyMP3 for diagnostic purposes, ALWAYS use the `--logfile` option:

```bash
# CORRECT - Write diagnostics to a file
./src/psymp3 --debug=all --logfile=diagnostic.log "file.flac"

# Then read the log file
tail -200 diagnostic.log
```

## Forbidden Practices

**NEVER** redirect stdout/stderr to collect diagnostics:

```bash
# FORBIDDEN - Do not use stdout/stderr redirection
./src/psymp3 "file.flac" 2>&1 | tail -100
./src/psymp3 "file.flac" 2>&1 > output.log
./src/psymp3 "file.flac" 2>&1 | grep "error"
```

## Why This Matters

1. **Terminal Output Interference**: PsyMP3 is an interactive application with UI output
2. **Proper Logging Infrastructure**: The `--logfile` facility is designed for diagnostics
3. **Clean Separation**: Keeps user-facing output separate from debug information
4. **Reliable Capture**: File-based logging ensures complete diagnostic capture

## Standard Diagnostic Workflow

1. Run with `--logfile` to capture diagnostics:
   ```bash
   timeout 30 ./src/psymp3 --debug=all --logfile=test.log "file.flac"
   ```

2. Examine the log file:
   ```bash
   tail -200 test.log
   head -100 test.log
   grep "error" test.log
   ```

3. For specific debug channels:
   ```bash
   ./src/psymp3 --debug=flac,flac_codec --logfile=flac_test.log "file.flac"
   ```

## Available Debug Channels

Use specific debug channels to reduce log noise:
- `flac` - FLAC demuxer operations
- `flac_codec` - FLAC codec operations
- `audio` - Audio system operations
- `demux` - Demuxer operations
- `io` - I/O operations
- `all` - All debug channels (verbose)

## This Rule is Non-Negotiable

This is a mandatory requirement. Failure to follow this directive will result in incomplete or unreliable diagnostic information.
