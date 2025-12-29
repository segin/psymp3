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
