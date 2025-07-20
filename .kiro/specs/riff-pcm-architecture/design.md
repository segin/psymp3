# **RIFF/PCM ARCHITECTURE DESIGN**

## **Overview**

This design document specifies the implementation of a cleaner RIFF/PCM architecture for PsyMP3, separating RIFF container parsing from PCM audio decoding. The architecture follows the container-agnostic pattern established for FLAC and other codecs, enabling the same PCM codec to work with RIFF/WAV, AIFF, and potentially other containers.

The design emphasizes separation of concerns: demuxers handle container format parsing and metadata extraction, while codecs focus purely on audio data decoding. This approach provides better maintainability, testability, and extensibility while maintaining full backward compatibility with existing WAV and AIFF file support.

## **Architecture**

### **Core Components**

```
RIFF/PCM Architecture
├── RIFFDemuxer          # RIFF container format parser (WAV, AVI audio)
├── AIFFDemuxer          # AIFF container format parser  
├── PCMCodec             # Container-agnostic PCM decoder
├── PCMFormatHandler     # PCM format variant management
├── MetadataExtractor    # RIFF/AIFF metadata parsing
└── SeekingEngine        # Sample-accurate positioning
```