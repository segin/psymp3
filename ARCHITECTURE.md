# PsyMP3 Architecture

> **Note**: This document provides a high-level overview of the PsyMP3 system architecture. For build instructions and usage, see [README.md](README.md).

## 1. Core Concept: VLC-Style Audio Pipeline

PsyMP3 utilizes an on-demand decoding pipeline, avoiding large decoded buffers in favor of memory efficiency.

**Data Flow:**
```mermaid
graph LR
    File[File Input] --> IO[IOHandler]
    IO --> Demuxer[Demuxer]
    Demuxer --> Chunk[MediaChunk (Compressed)]
    Chunk --> Codec[AudioCodec]
    Codec --> Frame[AudioFrame (PCM)]
```

*Decoded audio is NOT buffered. Compressed chunks are buffered and decoded purely on-demand.*

## 2. Subsystem Layout

The source code (`src/`) is organized by functional component:

```text
src/
├── codecs/         # Audio codec implementations
│   ├── flac/       # Native FLAC decoder (RFC 9639 compliant, no libFLAC)
│   ├── mp3/        # MP3 codec (libmpg123 wrapper)
│   ├── opus/       # Opus codec (libopus wrapper)
│   ├── vorbis/     # Vorbis codec (libvorbis wrapper)
│   └── pcm/        # PCM codecs: Raw PCM, G.711 (μ-law, A-law)
├── demuxer/        # Container demuxer implementations
│   ├── ogg/        # Ogg container (Vorbis, Opus, FLAC)
│   ├── iso/        # ISO Base Media File Format (MP4/M4A)
│   ├── riff/       # RIFF/WAV container
│   ├── flac/       # Native FLAC container
│   └── raw/        # Raw/headerless streams
├── io/             # I/O abstraction layer
│   ├── file/       # Local file I/O
│   └── http/       # HTTP streaming
├── widget/         # UI widget system
├── lastfm/         # Last.fm scrobbling
└── mpris/          # MPRIS media player interface (D-Bus)
```

## 3. Component Registration (Static Initializers)

PsyMP3 uses a self-registration pattern for codecs and demuxers to allow modular extension.

*   **Demuxers**: Registered via `DemuxerFactory::registerDemuxer()` in `src/demuxer/DemuxerRegistry.cpp`.
*   **Codecs**: Registered via `AudioCodecFactory::registerCodec()` in `src/codecs/CodecRegistration.cpp`.

## 4. Key Base Classes

| Class | Location | Purpose |
|-------|----------|---------|
| `AudioCodec` | `include/codecs/AudioCodec.h` | Abstract base for all audio decoders. |
| `SimplePCMCodec` | `include/codecs/AudioCodec.h` | Helper base for PCM-type codecs (WAV, G.711). |
| `Demuxer` | `include/demuxer/Demuxer.h` | Abstract base for container parsers. |
| `IOHandler` | `include/io/IOHandler.h` | Abstract interface for data sources (File, HTTP). |
| `StreamInfo` | `include/demuxer/Demuxer.h` | Metadata structure for stream properties. |
| `MediaChunk` | `include/demuxer/Demuxer.h` | Encapsulates a unit of compressed data. |
| `AudioFrame` | `include/codecs/AudioCodec.h` | Encapsulates decoded PCM samples. |
| `CodecHeaderParser` | `include/demuxer/ogg/CodecHeaderParser.h` | Validator for codec-specific Ogg headers. |

## 5. Ogg Demuxer Architecture

The Ogg demuxer follows a layered design to ensure RFC 3533 compliance and handle complex multiplexed streams.

| Layer | Class | Responsibility |
|-------|-------|----------------|
| **Sync** | `OggSyncManager` | Handles `OggS` capture, page sync, and CRC validation. Manages a circular buffer of raw data. |
| **Stream** | `OggStreamManager` | Manages a single logical bitstream (`ogg_stream_state`) and reconstructs packets from pages. |
| **Seek** | `OggSeekingEngine` | Implements precise bisection seeking and duration calculation using granule positions. |
| **Demux** | `OggDemuxer` | Orchestrates components, selects primary streams, and provides the standard `Demuxer` interface. |
| **Codec Header** | `CodecHeaderParser` | Validates codec-specific BOS headers (Vorbis, Opus, FLAC, Speex) to extract sample rates and pre-skip. |

### 5.1 Ogg Layered Detail

The Ogg demuxer is implemented as a pipeline of specialized managers:

1.  **Sync Layer (`OggSyncManager`)**: The foundation. It reads raw bytes from `IOHandler` into an `ogg_sync_state` buffer. It performs "capture pattern" synchronization to find the Ogg page markers (`OggS`) and validates checksums.
2.  **Stream Layer (`OggStreamManager`)**: Manages logical bitstreams. Once a page is synchronized, it is routed to the appropriate `OggStreamManager` based on its serial number. This layer handles the reassembly of Ogg packets from one or more Ogg pages, including handling spanning packets.
3.  **Seek Layer (`OggSeekingEngine`)**: Handles bisection seeking. It uses the `OggSyncManager` to scan the file for pages near a target timestamp. It converts between milliseconds and Ogg granule positions using codec-specific math provided by header parsers.
    *   **Serial Number Validation**: Crucially, the bisection search validates page serial numbers against the target stream. This ensures correct seeking in multiplexed files (e.g., Ogg Theora+Vorbis) by ignoring pages that belong to other concurrent streams, preventing invalid granule interpretation.
4.  **Demux Layer (`OggDemuxer`)**: The top-level orchestrator. It manages the lifecycle of all internal managers. It implements the `Demuxer` interface used by the rest of PsyMP3. It also handles "chained" Ogg streams (multiplexed serial numbers).
5.  **Codec Header Parser (`CodecHeaderParser`)**: A pluggable validation layer. It inspects the first packet of each stream to identify the codec (Vorbis, Opus, FLAC, Speex, or unknown) and extracts critical metadata like sample rate and channel count without initializing a full decoder.

## 6. Core Mechanisms

### 6.1 Deferred Widget Deletion
To prevent Heap-Use-After-Free errors where a widget destroys itself within its own event callback (e.g., closing a window), PsyMP3 uses a deferred deletion pattern.

-   **Mechanism**: The `Player` class maintains a `m_deferred_widgets` queue.
-   **Usage**: Instead of `delete this` or `.reset()`, widgets transfer ownership to `Player::deferWidgetDeletion()`.
-   **Execution**: At the end of each event loop iteration, `Player::processDeferredDeletions()` clears the queue, safely destroying widgets when they are no longer on the call stack.

### 6.2 Ogg Duration Caching
Determining the duration of an Ogg file requires scanning the last page. To avoid performance penalties:

-   **Caching**: `OggSeekingEngine` calculates the duration once (via bisection search for the last granule) and caches it in `m_duration_cached`.
-   **Optimization**: Subsequent calls to `getDuration()` return the cached value instantly, preventing repeated file I/O during playback or seeking operations.

## 7. Last.fm Integration

PsyMP3 includes a background scrobbler for the Last.fm service, using the legacy Audioscrobbler 1.2 API.

### 7.1 Secure Credential Storage
To protect user credentials, PsyMP3 implements several security measures:
- **One-Way Hashing**: Passwords are never stored in plain text. Instead, an MD5 hash of the password is saved, which is the format required by the Last.fm 1.2 authentication protocol.
- **Restricted File Permissions**: The Last.fm configuration file (`lastfm.conf`) is created with restricted filesystem permissions (0600 on Unix-like systems), ensuring it is only readable by the current user.
- **Legacy Migration**: Upon startup, any existing plain-text password in the configuration is automatically hashed and the original plain-text entry is removed from the file on the next write.

### 7.2 Background Submission
Scrobbling operations (handshake, now-playing updates, and scrobble submissions) are performed in a dedicated background thread (`lastfm-submission`) to prevent network latency from affecting UI responsiveness. Failed scrobbles are cached in an XML file and retried with exponential backoff.
