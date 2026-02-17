# File Analysis for Namespace Migration

## Files Already Migrated (Have Proper Namespaces)

### IO Subsystem
- ✅ `src/io/IOHandler.cpp` - `PsyMP3::IO`
- ✅ `src/io/StreamingManager.cpp` - `PsyMP3::IO`
- ✅ `src/io/URI.cpp` - `PsyMP3::IO`
- ✅ `src/io/TagLibIOHandlerAdapter.cpp` - `PsyMP3::IO`
- ✅ `src/io/file/FileIOHandler.cpp` - `PsyMP3::IO::File`
- ✅ `src/io/http/HTTPClient.cpp` - `PsyMP3::IO::HTTP` ✓ verified

### LastFM Subsystem
- ✅ `src/lastfm/LastFM.cpp` - `PsyMP3::LastFM`
- ✅ `src/lastfm/scrobble.cpp` - `PsyMP3::LastFM`

### Demuxer Subsystem
- ✅ `src/demuxer/Demuxer.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/DemuxerFactory.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/DemuxerRegistry.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/MediaFactory.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/DemuxedStream.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/ChunkDemuxer.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/ModernStream.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/DemuxerExtensibility.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/DemuxerPlugin.cpp` - `PsyMP3::Demuxer`
- ✅ `src/demuxer/ChainedStream.cpp` - `PsyMP3::Demuxer` ✓ verified
- ✅ `src/demuxers/flac/FLACDemuxer.cpp` - `PsyMP3::Demuxer::FLAC` ✓ verified (will move to src/demuxer/flac/)
- ✅ `src/demuxers/ogg/OggDemuxer.cpp` - `PsyMP3::Demuxer::Ogg` ✓ verified (will move to src/demuxer/ogg/)

### Codec Subsystem
- ✅ `src/codecs/AudioCodec.cpp` - `PsyMP3::Codec`
- ✅ `src/codecs/CodecRegistry.cpp` - `PsyMP3::Codec`
- ✅ `src/codecs/CodecRegistration.cpp` - `PsyMP3::Codec` ✓ verified
- ✅ `src/codecs/flac.cpp` - `PsyMP3::Codec::FLAC`
- ✅ `src/codecs/OggCodecs.cpp` - `PsyMP3::Codec`
- ✅ `src/codecs/flac/NativeFLACCodec.cpp` - `PsyMP3::Codec::FLAC` ✓ verified

### Widget Subsystem
- ✅ `src/widget/windowing/WindowWidget.cpp` - `PsyMP3::Widget::Windowing`
- ✅ `src/widget/windowing/WindowFrameWidget.cpp` - `PsyMP3::Widget::Windowing` ✓ verified
- ✅ `src/widget/windowing/TitlebarWidget.cpp` - `PsyMP3::Widget::Windowing` ✓ verified
- ✅ `src/widget/windowing/TransparentWindowWidget.cpp` - `PsyMP3::Widget::Windowing` ✓ verified
- ✅ `src/widget/ui/ButtonWidget.cpp` - `PsyMP3::Widget::UI`

## Files Needing Namespace Migration

### Codec Subsystem Files
- ❌ `src/codecs/FLACCodec.cpp` - Needs `PsyMP3::Codec::FLAC`
- ❌ `src/codecs/flac/BitstreamReader.cpp` - Needs `PsyMP3::Codec::FLAC`
- ❌ `src/codecs/flac/FrameParser.cpp` - Needs `PsyMP3::Codec::FLAC`
- ❌ `src/codecs/flac/SubframeDecoder.cpp` - Needs `PsyMP3::Codec::FLAC`
- ❌ `src/codecs/flac/ResidualDecoder.cpp` - Needs `PsyMP3::Codec::FLAC`
- ❌ `src/codecs/flac/CRCValidator.cpp` - Needs `PsyMP3::Codec::FLAC`
- ❌ `src/codecs/opus/OpusCodec.cpp` - Needs `PsyMP3::Codec::Opus`
- ❌ `src/codecs/vorbis/VorbisCodec.cpp` - Needs `PsyMP3::Codec::Vorbis`
- ❌ `src/codecs/pcm/PCMCodecs.cpp` - Needs `PsyMP3::Codec::PCM`
- ❌ `src/codecs/pcm/ALawCodec.cpp` - Needs `PsyMP3::Codec::PCM`
- ❌ `src/codecs/pcm/MuLawCodec.cpp` - Needs `PsyMP3::Codec::PCM`
- ❌ `src/codecs/mp3/libmpg123.cpp` - Needs `PsyMP3::Codec::MP3`

### Demuxer Subsystem Files
- ❌ `src/demuxer/iso/ISODemuxer.cpp` - Needs `PsyMP3::Demuxer::ISO` (keep class name `ISODemuxer`)
- ❌ `src/demuxer/iso/ISODemuxerBoxParser.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `BoxParser`
- ❌ `src/demuxer/iso/ISODemuxerComplianceValidator.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `ComplianceValidator`
- ❌ `src/demuxer/iso/ISODemuxerErrorRecovery.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `ErrorRecovery`
- ❌ `src/demuxer/iso/ISODemuxerFragmentHandler.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `FragmentHandler`
- ❌ `src/demuxer/iso/ISODemuxerMetadataExtractor.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `MetadataExtractor`
- ❌ `src/demuxer/iso/ISODemuxerSampleTableManager.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `SampleTableManager`
- ❌ `src/demuxer/iso/ISODemuxerSeekingEngine.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `SeekingEngine`
- ❌ `src/demuxer/iso/ISODemuxerStreamManager.cpp` - Needs `PsyMP3::Demuxer::ISO` + rename class to `StreamManager`
- ❌ `src/demuxer/riff/wav.cpp` - Needs `PsyMP3::Demuxer::RIFF`
- ❌ `src/demuxer/raw/RawAudioDemuxer.cpp` - Needs `PsyMP3::Demuxer::Raw`
- ✅ `src/demuxers/flac/FLACDemuxer.cpp` - Already has `PsyMP3::Demuxer::FLAC` (will move to src/demuxer/flac/)
- ✅ `src/demuxers/ogg/OggDemuxer.cpp` - Already has `PsyMP3::Demuxer::Ogg` (will move to src/demuxer/ogg/)

### IO Subsystem Files
- ❌ `src/io/http/HTTPIOHandler.cpp` - Needs `PsyMP3::IO::HTTP`

### Widget Subsystem Files
- ❌ `src/widget/foundation/*.cpp` - All files need `PsyMP3::Widget::Foundation`
- ❌ `src/widget/ui/*.cpp` - Remaining files need `PsyMP3::Widget::UI`
- ❌ `src/widget/windowing/*.cpp` - Remaining files need `PsyMP3::Widget::Windowing`

## Files That Should Be Moved to Subsystems

### IO-Related Files (Should move to src/io/)
- ❌ `src/RAIIFileHandle.cpp` - Should move to `src/io/RAIIFileHandle.cpp` with `PsyMP3::IO`
- ❌ `src/MemoryPoolManager.cpp` - Should move to `src/io/MemoryPoolManager.cpp` with `PsyMP3::IO`
- ❌ `src/MemoryOptimizer.cpp` - Should move to `src/io/MemoryOptimizer.cpp` with `PsyMP3::IO`
- ❌ `src/MemoryTracker.cpp` - Should move to `src/io/MemoryTracker.cpp` with `PsyMP3::IO`
- ❌ `src/BufferPool.cpp` - Should move to `src/io/BufferPool.cpp` with `PsyMP3::IO`
- ❌ `src/EnhancedBufferPool.cpp` - Should move to `src/io/EnhancedBufferPool.cpp` with `PsyMP3::IO`
- ❌ `src/EnhancedAudioBufferPool.cpp` - Should move to `src/io/EnhancedAudioBufferPool.cpp` with `PsyMP3::IO`
- ❌ `src/BoundedBuffer.cpp` - Should move to `src/io/BoundedBuffer.cpp` with `PsyMP3::IO`

### MPRIS-Related Files (Should create src/mpris/ subsystem)
- ❌ `src/MPRISManager.cpp` - Should move to `src/mpris/MPRISManager.cpp` with `PsyMP3::MPRIS`
- ❌ `src/DBusConnectionManager.cpp` - Should move to `src/mpris/DBusConnectionManager.cpp` with `PsyMP3::MPRIS`
- ❌ `src/MPRISTypes.cpp` - Should move to `src/mpris/MPRISTypes.cpp` with `PsyMP3::MPRIS`
- ❌ `src/MethodHandler.cpp` - Should move to `src/mpris/MethodHandler.cpp` with `PsyMP3::MPRIS`
- ❌ `src/PropertyManager.cpp` - Should move to `src/mpris/PropertyManager.cpp` with `PsyMP3::MPRIS`
- ❌ `src/SignalEmitter.cpp` - Should move to `src/mpris/SignalEmitter.cpp` with `PsyMP3::MPRIS`

### Widget-Related Files (Should move to src/widget/)
- ❌ `src/Label.cpp` - Should move to `src/widget/ui/Label.cpp` with `PsyMP3::Widget::UI`

### Utility Files (Should create src/util/ subsystem)
- ❌ `src/XMLUtil.cpp` - Should move to `src/util/XMLUtil.cpp` with `PsyMP3::Util`

## Files That Should Stay in src/ Root (Core Application Files)

These files are core application logic and should remain in src/ root without subsystem namespaces:

- ✅ `src/main.cpp` - Application entry point
- ✅ `src/player.cpp` - Core player class
- ✅ `src/audio.cpp` - Core audio system
- ✅ `src/playlist.cpp` - Core playlist management
- ✅ `src/song.cpp` - Core song class
- ✅ `src/track.cpp` - Core track class
- ✅ `src/stream.cpp` - Core stream base class
- ✅ `src/mediafile.cpp` - Core media file handling
- ✅ `src/display.cpp` - Core display management
- ✅ `src/surface.cpp` - Core surface wrapper
- ✅ `src/font.cpp` - Core font handling
- ✅ `src/fft.cpp` - Core FFT processing
- ✅ `src/fft_draw.cpp` - Core FFT rendering
- ✅ `src/lyrics.cpp` - Core lyrics support
- ✅ `src/about.cpp` - Core about dialog
- ✅ `src/system.cpp` - Core system interfaces
- ✅ `src/utility.cpp` - Core utility functions
- ✅ `src/debug.cpp` - Core debug system
- ✅ `src/exceptions.cpp` - Core exception classes
- ✅ `src/persistentstorage.cpp` - Core storage wrapper
- ✅ `src/rect.cpp` - Core geometry class

## Summary

### Migration Strategy

1. **Phase 1: Namespace Existing Subsystem Files** (No file moves)
   - Add namespaces to all files already in subsystem directories
   - Verify builds after each subsystem is complete

2. **Phase 2: Create New Subsystems and Move Files**
   - Create `src/mpris/` subsystem and move MPRIS files
   - Create `src/util/` subsystem and move utility files
   - Move IO-related files from src/ to src/io/
   - Move widget-related files from src/ to src/widget/ui/

3. **Phase 3: Update Build System**
   - Update Makefile.am files for new subsystems
   - Update configure.ac for new subsystem Makefiles
   - Update src/Makefile.am to link new subsystem libraries

4. **Phase 4: Verification**
   - Full project build
   - Run existing tests
   - Verify backward compatibility

### File Counts

- **Already Migrated**: ~30 files
- **Need Namespace Only**: ~40 files
- **Need Move + Namespace**: ~15 files
- **Stay in Root**: ~20 files
- **Total Files**: ~105 files
