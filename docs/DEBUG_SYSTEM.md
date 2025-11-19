# PsyMP3 Debug System

## Overview

The PsyMP3 debug system provides flexible, channel-based logging with support for sub-channels and optional function/line information.

## Basic Usage

### Command Line

```bash
# Enable specific channels
./src/psymp3 --debug=flac,audio --logfile=debug.log "file.flac"

# Enable all channels
./src/psymp3 --debug=all --logfile=debug.log "file.flac"

# Enable multiple channels
./src/psymp3 --debug=flac,demux,audio --logfile=debug.log "file.flac"
```

## Sub-Channels

Sub-channels allow fine-grained control over debug output using colon notation:

```bash
# Enable only FLAC frame parsing
./src/psymp3 --debug=flac:frame --logfile=debug.log "file.flac"

# Enable multiple sub-channels
./src/psymp3 --debug=flac:frame,flac:metadata,audio:buffer --logfile=debug.log "file.flac"

# Enable parent channel (enables all sub-channels)
./src/psymp3 --debug=flac --logfile=debug.log "file.flac"
```

### Sub-Channel Behavior

- **Parent channel enabled**: Shows all messages from parent and all sub-channels
  - `--debug=flac` shows `flac`, `flac:frame`, `flac:metadata`, etc.

- **Specific sub-channel enabled**: Shows only that sub-channel
  - `--debug=flac:frame` shows only `flac:frame` messages, not `flac` or `flac:metadata`

- **Multiple sub-channels**: Shows only the specified sub-channels
  - `--debug=flac:frame,flac:metadata` shows only those two sub-channels

## Code Usage

### Basic Logging

```cpp
// Simple message
Debug::log("flac", "Parsing FLAC file");

// With values
Debug::log("flac", "Frame size: ", frame_size, " bytes");
```

### Logging with Function and Line Information

```cpp
// Using the DEBUG_LOG macro (recommended)
DEBUG_LOG("flac", "Parsing frame header");
DEBUG_LOG("flac", "Block size: ", block_size);

// Manual (if you need custom location info)
Debug::log("flac", __FUNCTION__, __LINE__, "Custom message");
```

### Sub-Channel Usage

```cpp
// Use sub-channels to categorize messages
Debug::log("flac:frame", "Parsing frame header");
Debug::log("flac:metadata", "Reading STREAMINFO block");
Debug::log("flac:seek", "Building seek table");

// With location info
DEBUG_LOG("flac:frame", "Frame CRC validation passed");
DEBUG_LOG("flac:metadata", "Found VORBIS_COMMENT block");
```

## Output Format

### Without Location Info
```
01:20:08.566486 [flac]: Parsing FLAC file
01:20:08.566588 [flac:frame]: Frame size: 4096 bytes
```

### With Location Info
```
01:20:08.566600 [flac] [parseFrame:123]: Parsing frame header
01:20:08.566610 [flac:frame] [validateCRC:456]: CRC validation passed
```

Format: `HH:MM:SS.microseconds [channel] [function:line]: message`

## Recommended Sub-Channel Conventions

### FLAC
- `flac:frame` - Frame parsing and processing
- `flac:metadata` - Metadata block parsing
- `flac:seek` - Seek table and seeking operations
- `flac:crc` - CRC validation
- `flac:error` - Error conditions

### Audio
- `audio:buffer` - Buffer management
- `audio:playback` - Playback operations
- `audio:decode` - Decoding operations
- `audio:sync` - Synchronization

### Demuxer
- `demux:init` - Initialization
- `demux:parse` - Container parsing
- `demux:seek` - Seeking operations
- `demux:error` - Error conditions

## Best Practices

1. **Use sub-channels** for different subsystems within a component
2. **Use DEBUG_LOG macro** when debugging complex issues that need location info
3. **Use basic Debug::log** for general informational messages
4. **Keep messages concise** but informative
5. **Use appropriate channels** - don't log everything to a single channel
6. **Test with specific channels** enabled to reduce noise

## Examples

### Debugging FLAC Frame Parsing

```bash
# See only frame-related messages
./src/psymp3 --debug=flac:frame --logfile=debug.log "file.flac"

# See frame and CRC messages
./src/psymp3 --debug=flac:frame,flac:crc --logfile=debug.log "file.flac"

# See all FLAC messages
./src/psymp3 --debug=flac --logfile=debug.log "file.flac"
```

### Debugging Audio Playback

```bash
# See only buffer operations
./src/psymp3 --debug=audio:buffer --logfile=debug.log "file.flac"

# See all audio operations
./src/psymp3 --debug=audio --logfile=debug.log "file.flac"

# See audio and FLAC together
./src/psymp3 --debug=audio,flac --logfile=debug.log "file.flac"
```

### Finding Specific Issues

```bash
# Debug seeking issues
./src/psymp3 --debug=flac:seek,demux:seek --logfile=debug.log "file.flac"

# Debug error conditions
./src/psymp3 --debug=flac:error,audio:error,demux:error --logfile=debug.log "file.flac"

# Full diagnostic (verbose!)
./src/psymp3 --debug=all --logfile=debug.log "file.flac"
```
