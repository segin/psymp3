# Implementation Plan

- [x] 1. Phase 1: Add namespaces to Codec subsystem files
  - Add proper `PsyMP3::Codec::<Component>` namespaces to files already in codec subdirectories
  - Verify master header inclusion pattern
  - Ensure files end with newline
  - _Requirements: 1.1, 2.1-2.5, 3.1-3.4, 6.1-6.7_

- [x] 1.1 Add namespace to PCM codec files
  - Add `PsyMP3::Codec::PCM` namespace to `src/codecs/pcm/PCMCodec.cpp`
  - Add `PsyMP3::Codec::PCM` namespace to `src/codecs/pcm/ALawCodec.cpp`
  - Add `PsyMP3::Codec::PCM` namespace to `src/codecs/pcm/MuLawCodec.cpp`
  - Verify each file includes only `psymp3.h` as master header
  - Ensure proper namespace closure with comments
  - _Requirements: 1.1, 2.4, 3.1, 6.1-6.7_

- [x] 1.2 Add namespace to Opus codec files
  - Add `PsyMP3::Codec::Opus` namespace to `src/codecs/opus/OpusCodec.cpp`
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 1.1, 2.2, 3.1, 6.1-6.7_

- [x] 1.3 Add namespace to Vorbis codec files
  - Add `PsyMP3::Codec::Vorbis` namespace to `src/codecs/vorbis/VorbisCodec.cpp`
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 1.1, 2.3, 3.1, 6.1-6.7_

- [x] 1.4 Add namespace to MP3 codec files
  - Add `PsyMP3::Codec::MP3` namespace to `src/codecs/mp3/MP3Codec.cpp`
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 1.1, 2.5, 3.1, 6.1-6.7_

- [x] 1.5 Verify Codec subsystem builds cleanly
  - Run `make -C src/codecs clean && make -C src/codecs -j$(nproc)`
  - Verify no compilation errors or warnings
  - _Requirements: 7.1-7.5_

- [-] 2. Phase 1: Add namespaces to IO subsystem files
  - Add proper `PsyMP3::IO::HTTP` namespace to HTTP handler
  - Verify master header inclusion pattern
  - Ensure files end with newline
  - _Requirements: 1.3, 2.12, 3.1-3.4, 6.1-6.7_

- [x] 2.1 Add namespace to HTTP IO handler
  - Add `PsyMP3::IO::HTTP` namespace to `src/io/http/HTTPIOHandler.cpp`
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 1.3, 2.12, 3.1, 6.1-6.7_

- [-] 2.2 Verify IO subsystem builds cleanly
  - Run `make -C src/io clean && make -C src/io -j$(nproc)`
  - Verify no compilation errors or warnings
  - _Requirements: 7.1-7.5_

- [-] 3. Phase 1: Add namespaces to Demuxer subsystem files
  - Add proper `PsyMP3::Demuxer::<Component>` namespaces to demuxer subdirectories
  - Verify master header inclusion pattern
  - Ensure files end with newline
  - _Requirements: 1.2, 2.6-2.8, 3.1-3.4, 6.1-6.7_

- [x] 3.1 Add namespace to ISO demuxer files
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxer.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerBoxParser.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerComplianceValidator.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerErrorRecovery.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerFragmentHandler.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerMetadataExtractor.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerSampleTableManager.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerSeekingEngine.cpp`
  - Add `PsyMP3::Demuxer::ISO` namespace to `src/demuxer/iso/ISODemuxerStreamManager.cpp`
  - Verify master header inclusion for all files
  - Ensure proper namespace closure with comments
  - _Requirements: 1.2, 2.6, 3.1, 6.1-6.7_

- [x] 3.2 Add namespace to RIFF demuxer files
  - Add `PsyMP3::Demuxer::RIFF` namespace to `src/demuxer/riff/wav.cpp`
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 1.2, 2.7, 3.1, 6.1-6.7_

- [x] 3.3 Add namespace to Raw demuxer files
  - Add `PsyMP3::Demuxer::Raw` namespace to `src/demuxer/raw/RawAudioDemuxer.cpp`
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 1.2, 2.8, 3.1, 6.1-6.7_

- [x] 3.4 Verify Demuxer subsystem builds cleanly
  - Run `make -C src/demuxer clean && make -C src/demuxer -j$(nproc)`
  - Verify no compilation errors or warnings
  - _Requirements: 7.1-7.5_

- [x] 4. Phase 1: Add namespaces to Widget subsystem files
  - Add proper `PsyMP3::Widget::<Component>` namespaces to widget subdirectories
  - Verify master header inclusion pattern
  - Ensure files end with newline
  - _Requirements: 1.4, 2.13-2.15, 3.1-3.4, 6.1-6.7_

- [x] 4.1 Add namespace to Widget Foundation files
  - Add `PsyMP3::Widget::Foundation` namespace to `src/widget/foundation/Widget.cpp`
  - Add `PsyMP3::Widget::Foundation` namespace to `src/widget/foundation/DrawableWidget.cpp`
  - Add `PsyMP3::Widget::Foundation` namespace to `src/widget/foundation/LayoutWidget.cpp`
  - Add `PsyMP3::Widget::Foundation` namespace to `src/widget/foundation/FadingWidget.cpp`
  - Add `PsyMP3::Widget::Foundation` namespace to `src/widget/foundation/WidgetManager.cpp`
  - Verify master header inclusion for all files
  - Ensure proper namespace closure with comments
  - _Requirements: 1.4, 2.13, 3.1, 6.1-6.7_

- [x] 4.2 Add namespace to Widget Windowing files
  - Add `PsyMP3::Widget::Windowing` namespace to `src/widget/windowing/TitlebarWidget.cpp`
  - Add `PsyMP3::Widget::Windowing` namespace to `src/widget/windowing/WindowFrameWidget.cpp`
  - Add `PsyMP3::Widget::Windowing` namespace to `src/widget/windowing/WindowWidget.cpp`
  - Add `PsyMP3::Widget::Windowing` namespace to `src/widget/windowing/TransparentWindowWidget.cpp`
  - Add `PsyMP3::Widget::Windowing` namespace to `src/widget/windowing/WindowManager.cpp`
  - Verify master header inclusion for all files
  - Ensure proper namespace closure with comments
  - _Requirements: 1.4, 2.14, 3.1, 6.1-6.7_

- [x] 4.3 Add namespace to Widget UI files
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/ButtonWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/SpectrumAnalyzerWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/PlayerProgressBarWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/ToastWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/ToastNotification.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/VolumeWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/PlaylistWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/LyricsWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/VisualizerWidget.cpp`
  - Add `PsyMP3::Widget::UI` namespace to `src/widget/ui/EqualizerWidget.cpp`
  - Verify master header inclusion for all files
  - Ensure proper namespace closure with comments
  - _Requirements: 1.4, 2.15, 3.1, 6.1-6.7_

- [x] 4.4 Verify Widget subsystem builds cleanly
  - Run `make -C src/widget clean && make -C src/widget -j$(nproc)`
  - Verify no compilation errors or warnings
  - _Requirements: 7.1-7.5_

- [x] 5. Checkpoint: Verify Phase 1 completion
  - Ensure all tests pass, ask the user if questions arise.

- [x] 6. Phase 2: Fold demuxers/ directory into demuxer/
  - Move FLAC and Ogg demuxers from src/demuxers/ to src/demuxer/
  - Update build system to reflect new structure
  - Remove old demuxers/ directory
  - _Requirements: 11.1-11.6_

- [x] 6.1 Move FLAC demuxer to demuxer/ directory
  - Use `git mv src/demuxers/flac src/demuxer/flac` to preserve history
  - Verify namespace is already `PsyMP3::Demuxer::FLAC`
  - _Requirements: 11.1, 11.3_

- [x] 6.2 Move Ogg demuxer to demuxer/ directory
  - Use `git mv src/demuxers/ogg src/demuxer/ogg` to preserve history
  - Verify namespace is already `PsyMP3::Demuxer::Ogg`
  - _Requirements: 11.2, 11.3_

- [x] 6.3 Update demuxer build system
  - Update `src/demuxer/Makefile.am` to add `flac` and `ogg` to SUBDIRS
  - Update `src/Makefile.am` to link `demuxer/flac/libflacdemuxer.a` and `demuxer/ogg/liboggdemuxer.a`
  - _Requirements: 11.5_

- [x] 6.4 Remove old demuxers/ directory
  - Remove `src/demuxers/Makefile.am`
  - Remove `src/demuxers/` directory (should be empty)
  - Update `configure.ac` to remove references to `src/demuxers/`
  - Update parent Makefiles to remove demuxers references
  - _Requirements: 11.4, 11.6_

- [x] 6.5 Regenerate build system and verify
  - Run `./generate-configure.sh`
  - Run `./configure`
  - Run `make -j$(nproc)` to verify full build succeeds
  - _Requirements: 7.3-7.5_

- [ ] 7. Phase 3: Create MPRIS subsystem
  - Create new MPRIS subsystem directory structure
  - Move MPRIS-related files from src/ root
  - Add proper namespaces
  - Update build system
  - _Requirements: 8.1-8.5_

- [ ] 7.1 Create MPRIS subsystem structure
  - Create `src/mpris/` directory
  - Create `src/mpris/Makefile.am` for building `libmpris.a`
  - Add source files list: MPRISManager.cpp, DBusConnectionManager.cpp, MPRISTypes.cpp, MethodHandler.cpp, PropertyManager.cpp, SignalEmitter.cpp
  - _Requirements: 8.1_

- [ ] 7.2 Move MPRIS files to new subsystem
  - Use `git mv src/MPRISManager.cpp src/mpris/MPRISManager.cpp`
  - Use `git mv src/DBusConnectionManager.cpp src/mpris/DBusConnectionManager.cpp`
  - Use `git mv src/MPRISTypes.cpp src/mpris/MPRISTypes.cpp`
  - Use `git mv src/MethodHandler.cpp src/mpris/MethodHandler.cpp`
  - Use `git mv src/PropertyManager.cpp src/mpris/PropertyManager.cpp`
  - Use `git mv src/SignalEmitter.cpp src/mpris/SignalEmitter.cpp`
  - _Requirements: 8.2_

- [ ] 7.3 Add namespaces to MPRIS files
  - Add `PsyMP3::MPRIS` namespace to all moved MPRIS files
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - Ensure files end with newline
  - _Requirements: 8.3, 3.1, 6.1-6.7_

- [ ] 7.4 Update build system for MPRIS
  - Add `src/mpris/Makefile` to `AC_CONFIG_FILES` in `configure.ac`
  - Add `mpris` to SUBDIRS in `src/Makefile.am`
  - Add `mpris/libmpris.a` to `psymp3_LDADD` in `src/Makefile.am`
  - _Requirements: 8.4_

- [ ] 7.5 Verify MPRIS subsystem builds
  - Run `./generate-configure.sh && ./configure`
  - Run `make -C src/mpris -j$(nproc)`
  - Verify no compilation errors
  - _Requirements: 8.5, 7.1-7.5_

- [ ] 8. Phase 3: Create Util subsystem
  - Create new Util subsystem directory structure
  - Move utility files from src/ root
  - Add proper namespaces
  - Update build system
  - _Requirements: 8.1, 8.4-8.5_

- [ ] 8.1 Create Util subsystem structure
  - Create `src/util/` directory
  - Create `src/util/Makefile.am` for building `libutil.a`
  - Add source files list: XMLUtil.cpp
  - _Requirements: 8.4_

- [ ] 8.2 Move utility files to new subsystem
  - Use `git mv src/XMLUtil.cpp src/util/XMLUtil.cpp`
  - _Requirements: 8.2_

- [ ] 8.3 Add namespaces to Util files
  - Add `PsyMP3::Util` namespace to XMLUtil.cpp
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - Ensure file ends with newline
  - _Requirements: 8.5, 3.1, 6.1-6.7_

- [ ] 8.4 Update build system for Util
  - Add `src/util/Makefile` to `AC_CONFIG_FILES` in `configure.ac`
  - Add `util` to SUBDIRS in `src/Makefile.am`
  - Add `util/libutil.a` to `psymp3_LDADD` in `src/Makefile.am`
  - _Requirements: 8.4_

- [ ] 8.5 Verify Util subsystem builds
  - Run `./generate-configure.sh && ./configure`
  - Run `make -C src/util -j$(nproc)`
  - Verify no compilation errors
  - _Requirements: 8.5, 7.1-7.5_

- [ ] 9. Checkpoint: Verify Phase 3 completion
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 10. Phase 4: Move memory management files to IO subsystem
  - Move memory-related files from src/ root to src/io/
  - Add proper namespaces
  - Update build system
  - _Requirements: 9.1-9.5_

- [ ] 10.1 Move memory management files
  - Use `git mv src/MemoryPoolManager.cpp src/io/MemoryPoolManager.cpp`
  - Use `git mv src/MemoryOptimizer.cpp src/io/MemoryOptimizer.cpp`
  - Use `git mv src/MemoryTracker.cpp src/io/MemoryTracker.cpp`
  - _Requirements: 9.1, 9.4_

- [ ] 10.2 Add namespaces to memory management files
  - Add `PsyMP3::IO` namespace to MemoryPoolManager.cpp
  - Add `PsyMP3::IO` namespace to MemoryOptimizer.cpp
  - Add `PsyMP3::IO` namespace to MemoryTracker.cpp
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 9.1, 3.1, 6.1-6.7_

- [ ] 10.3 Update IO build system for memory files
  - Add memory management files to `libio_a_SOURCES` in `src/io/Makefile.am`
  - Remove references from `src/Makefile.am`
  - _Requirements: 9.5_

- [ ] 10.4 Verify memory management files build
  - Run `make -C src/io -j$(nproc)`
  - Verify no compilation errors
  - _Requirements: 7.1-7.5_

- [ ] 11. Phase 4: Move buffer pool files to IO subsystem
  - Move buffer-related files from src/ root to src/io/
  - Add proper namespaces
  - Update build system
  - _Requirements: 9.2-9.5_

- [ ] 11.1 Move buffer pool files
  - Use `git mv src/BufferPool.cpp src/io/BufferPool.cpp`
  - Use `git mv src/EnhancedBufferPool.cpp src/io/EnhancedBufferPool.cpp`
  - Use `git mv src/EnhancedAudioBufferPool.cpp src/io/EnhancedAudioBufferPool.cpp`
  - Use `git mv src/BoundedBuffer.cpp src/io/BoundedBuffer.cpp`
  - _Requirements: 9.2, 9.4_

- [ ] 11.2 Add namespaces to buffer pool files
  - Add `PsyMP3::IO` namespace to BufferPool.cpp
  - Add `PsyMP3::IO` namespace to EnhancedBufferPool.cpp
  - Add `PsyMP3::IO` namespace to EnhancedAudioBufferPool.cpp
  - Add `PsyMP3::IO` namespace to BoundedBuffer.cpp
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 9.2, 3.1, 6.1-6.7_

- [ ] 11.3 Update IO build system for buffer files
  - Add buffer pool files to `libio_a_SOURCES` in `src/io/Makefile.am`
  - Remove references from `src/Makefile.am`
  - _Requirements: 9.5_

- [ ] 11.4 Verify buffer pool files build
  - Run `make -C src/io -j$(nproc)`
  - Verify no compilation errors
  - _Requirements: 7.1-7.5_

- [ ] 12. Phase 4: Move RAII file handle to IO subsystem
  - Move RAII wrapper from src/ root to src/io/
  - Add proper namespace
  - Update build system
  - _Requirements: 9.3-9.5_

- [ ] 12.1 Move RAII file handle
  - Use `git mv src/RAIIFileHandle.cpp src/io/RAIIFileHandle.cpp`
  - _Requirements: 9.3, 9.4_

- [ ] 12.2 Add namespace to RAII file handle
  - Add `PsyMP3::IO` namespace to RAIIFileHandle.cpp
  - Verify master header inclusion
  - Ensure proper namespace closure with comments
  - _Requirements: 9.3, 3.1, 6.1-6.7_

- [ ] 12.3 Update IO build system for RAII file
  - Add RAIIFileHandle.cpp to `libio_a_SOURCES` in `src/io/Makefile.am`
  - Remove reference from `src/Makefile.am`
  - _Requirements: 9.5_

- [ ] 12.4 Verify RAII file builds
  - Run `make -C src/io -j$(nproc)`
  - Verify no compilation errors
  - _Requirements: 7.1-7.5_

- [ ] 13. Phase 4: Verify IO subsystem completion
  - Run full IO subsystem build
  - Verify all moved files compile correctly
  - _Requirements: 7.3-7.5_

- [ ] 13.1 Full IO subsystem build verification
  - Run `make -C src/io clean && make -C src/io -j$(nproc)`
  - Verify no compilation errors or warnings
  - _Requirements: 7.3-7.5_

- [ ] 14. Checkpoint: Verify Phase 4 completion
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 15. Phase 5: Rename ISO demuxer helper classes
  - Remove redundant `ISODemuxer` prefix from helper classes
  - Keep main `ISODemuxer` class name unchanged
  - Update all references
  - _Requirements: 12.1-12.7_

- [ ] 15.1 Rename ISODemuxerBoxParser to BoxParser
  - Update class declaration in `include/demuxer/iso/ISODemuxerBoxParser.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerBoxParser.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.2 Rename ISODemuxerComplianceValidator to ComplianceValidator
  - Update class declaration in `include/demuxer/iso/ISODemuxerComplianceValidator.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerComplianceValidator.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.3 Rename ISODemuxerErrorRecovery to ErrorRecovery
  - Update class declaration in `include/demuxer/iso/ISODemuxerErrorRecovery.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerErrorRecovery.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.4 Rename ISODemuxerFragmentHandler to FragmentHandler
  - Update class declaration in `include/demuxer/iso/ISODemuxerFragmentHandler.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerFragmentHandler.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.5 Rename ISODemuxerMetadataExtractor to MetadataExtractor
  - Update class declaration in `include/demuxer/iso/ISODemuxerMetadataExtractor.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerMetadataExtractor.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.6 Rename ISODemuxerSampleTableManager to SampleTableManager
  - Update class declaration in `include/demuxer/iso/ISODemuxerSampleTableManager.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerSampleTableManager.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.7 Rename ISODemuxerSeekingEngine to SeekingEngine
  - Update class declaration in `include/demuxer/iso/ISODemuxerSeekingEngine.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerSeekingEngine.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.8 Rename ISODemuxerStreamManager to StreamManager
  - Update class declaration in `include/demuxer/iso/ISODemuxerStreamManager.h`
  - Update class implementation in `src/demuxer/iso/ISODemuxerStreamManager.cpp`
  - Update all references in ISO demuxer files
  - Verify build succeeds
  - _Requirements: 12.1, 12.3-12.6_

- [ ] 15.9 Verify ISO demuxer builds with renamed classes
  - Run `make -C src/demuxer/iso clean && make -C src/demuxer/iso -j$(nproc)`
  - Verify no compilation errors or warnings
  - _Requirements: 12.6, 7.1-7.5_

- [ ] 16. Final verification and cleanup
  - Perform full project build
  - Update backward compatibility declarations
  - Verify git history preservation
  - _Requirements: 4.1-4.4, 5.1-5.5, 7.4_

- [ ] 16.1 Add backward compatibility using declarations
  - Update `psymp3.h` with `using` declarations for commonly-used namespaced types
  - Add declarations for IO, Demuxer, Codec, Widget types
  - Document purpose of each using declaration
  - _Requirements: 4.1-4.3_

- [ ] 16.2 Full project build verification
  - Run `make clean`
  - Run `make -j$(nproc)`
  - Verify entire project builds without errors or warnings
  - _Requirements: 5.4, 7.4_

- [ ] 16.3 Verify git history preservation
  - For each moved file, run `git log --follow <file>` to verify history
  - Confirm commits before moves are visible
  - _Requirements: 9.4, 11.1-11.2_

- [ ] 16.4 Final checkpoint
  - Ensure all tests pass, ask the user if questions arise.
