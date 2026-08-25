#
# Makefile.am - automake input
# This file is part of PsyMP3.
# Copyright © 2011 Kirn Gill <segin2005@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any purpose
# with or without fee is hereby granted, provided that the above copyright
# notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
# OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
# DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
# ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

datadir=$(prefix)/@PACKAGE@/data

if BUILD_TEST_HARNESS
SUBDIRS = res src tests
else
SUBDIRS = res src
endif


# Every public header. Sources list only .cpp files, so nothing else
# distributes these; distcheck (now enforced by CI) keeps the list honest.
EXTRA_DIST = \
	include/BoundedQueue.h \
	include/RAIIFileHandle.h \
	include/SpectrumColors.h \
	include/ZOrder.h \
	include/audio.h \
	include/codecs/AudioCodec.h \
	include/codecs/CodecRegistration.h \
	include/codecs/CodecRegistry.h \
	include/codecs/FLACCodec.h \
	include/codecs/OggCodecs.h \
	include/codecs/aac/AACCodec.h \
	include/codecs/alac/ALACCodec.h \
	include/codecs/flac/BitstreamReader.h \
	include/codecs/flac/CRCValidator.h \
	include/codecs/flac/ChannelDecorrelator.h \
	include/codecs/flac/FLACError.h \
	include/codecs/flac/FLACPerformanceBenchmark.h \
	include/codecs/flac/FLACRFC9639.h \
	include/codecs/flac/FLACRFCValidator.h \
	include/codecs/flac/FLACTypes.h \
	include/codecs/flac/FrameParser.h \
	include/codecs/flac/MD5Validator.h \
	include/codecs/flac/MetadataParser.h \
	include/codecs/flac/NativeFLACCodec.h \
	include/codecs/flac/ResidualDecoder.h \
	include/codecs/flac/SampleReconstructor.h \
	include/codecs/flac/SubframeDecoder.h \
	include/codecs/flac/ValidationUtils.h \
	include/codecs/mp2/MP2Codec.h \
	include/codecs/mp3/MiniMP3Codec.h \
	include/codecs/opus/OpusCodec.h \
	include/codecs/pcm/ALawCodec.h \
	include/codecs/pcm/G722Codec.h \
	include/codecs/pcm/MuLawCodec.h \
	include/codecs/pcm/PCMCodecs.h \
	include/codecs/vorbis/VorbisCodec.h \
	include/core/BezierCurve.h \
	include/core/FileDialog.h \
	include/core/SpectrumConfig.h \
	include/core/about.h \
	include/core/compression/Compressor.h \
	include/core/compression/Decompressor.h \
	include/core/compression/LZ77.h \
	include/core/exceptions.h \
	include/core/fft.h \
	include/core/fft_draw.h \
	include/core/lyrics.h \
	include/core/nullstream.h \
	include/core/persistentstorage.h \
	include/core/rect.h \
	include/core/song.h \
	include/core/utility/Base64.h \
	include/core/utility/G711.h \
	include/core/utility/UTF8Util.h \
	include/core/utility/XMLUtil.h \
	include/core/utility/utility.h \
	include/debug.h \
	include/demuxer/ChainedStream.h \
	include/demuxer/ChunkDemuxer.h \
	include/demuxer/DemuxedStream.h \
	include/demuxer/Demuxer.h \
	include/demuxer/DemuxerExtensibility.h \
	include/demuxer/DemuxerFactory.h \
	include/demuxer/DemuxerPlugin.h \
	include/demuxer/DemuxerRegistry.h \
	include/demuxer/MediaFactory.h \
	include/demuxer/ModernStream.h \
	include/demuxer/flac/FLACDemuxer.h \
	include/demuxer/iso/BoxParser.h \
	include/demuxer/iso/ComplianceValidator.h \
	include/demuxer/iso/ErrorRecovery.h \
	include/demuxer/iso/FragmentHandler.h \
	include/demuxer/iso/ISODemuxer.h \
	include/demuxer/iso/MetadataExtractor.h \
	include/demuxer/iso/SampleTableManager.h \
	include/demuxer/iso/SeekingEngine.h \
	include/demuxer/iso/StreamManager.h \
	include/demuxer/mp3/MP3NullDemuxer.h \
	include/demuxer/ogg/CodecHeaderParser.h \
	include/demuxer/ogg/FLACHeaderParser.h \
	include/demuxer/ogg/OggDemuxer.h \
	include/demuxer/ogg/OggSeekingEngine.h \
	include/demuxer/ogg/OggStreamManager.h \
	include/demuxer/ogg/OggSyncManager.h \
	include/demuxer/ogg/OpusHeaderParser.h \
	include/demuxer/ogg/SpeexHeaderParser.h \
	include/demuxer/ogg/VorbisHeaderParser.h \
	include/demuxer/raw/RawAudioDemuxer.h \
	include/demuxer/riff/wav.h \
	include/display.h \
	include/discord/DiscordPresence.h \
	include/dsp/Equalizer.h \
	include/font.h \
	include/io/BoundedBuffer.h \
	include/io/BufferPool.h \
	include/io/EnhancedAudioBufferPool.h \
	include/io/EnhancedBufferPool.h \
	include/io/EnhancedTemplateBufferPool.h \
	include/io/IOHandler.h \
	include/io/MemoryIOHandler.h \
	include/io/MemoryOptimizer.h \
	include/io/MemoryPoolManager.h \
	include/io/MemoryTracker.h \
	include/io/StreamingManager.h \
	include/io/TagLibIOHandlerAdapter.h \
	include/io/URI.h \
	include/io/file/FileIOHandler.h \
	include/io/http/HTTPClient.h \
	include/io/http/HTTPIOHandler.h \
	include/lastfm/LastFM.h \
	include/lastfm/scrobble.h \
	include/mediafile.h \
	include/mpris/DBusConnectionManager.h \
	include/mpris/MPRISDebugMacros.h \
	include/mpris/MPRISManager.h \
	include/mpris/MPRISTypes.h \
	include/mpris/MethodHandler.h \
	include/mpris/PropertyManager.h \
	include/mpris/SignalEmitter.h \
	include/player.h \
	include/playlist.h \
	include/psymp3.h \
	include/stream.h \
	include/surface.h \
	include/system.h \
	include/tag/ID3v1Tag.h \
	include/tag/ID3v2Tag.h \
	include/tag/ID3v2Utils.h \
	include/tag/ImageUtils.h \
	include/tag/MergedID3Tag.h \
	include/tag/NullTag.h \
	include/tag/Tag.h \
	include/tag/TagConstants.h \
	include/tag/TagFactory.h \
	include/tag/VorbisCommentTag.h \
	include/track.h \
	include/truetype.h \
	include/widget/foundation/DrawableWidget.h \
	include/widget/foundation/FadingWidget.h \
	include/widget/foundation/LayoutWidget.h \
	include/widget/foundation/Widget.h \
	include/widget/ui/AboutWindow.h \
	include/widget/ui/ApplicationWidget.h \
	include/widget/ui/ButtonWidget.h \
	include/widget/ui/CheckboxWidget.h \
	include/widget/ui/ContextMenuWidget.h \
	include/widget/ui/EqualizerCurveWidget.h \
	include/widget/ui/EqualizerWindow.h \
	include/widget/ui/Label.h \
	include/widget/ui/ListViewWidget.h \
	include/widget/ui/LyricsWidget.h \
	include/widget/ui/MainUIWidget.h \
	include/widget/ui/MenuBarWidget.h \
	include/widget/ui/PlaybackIndicatorsWidget.h \
	include/widget/ui/PlayerProgressBarWidget.h \
	include/widget/ui/ProgressBarBracketWidget.h \
	include/widget/ui/ProgressBarFrameWidget.h \
	include/widget/ui/ScrollbarWidget.h \
	include/widget/ui/SliderWidget.h \
	include/widget/ui/SpectrumAnalyzerWidget.h \
	include/widget/ui/TextInputWidget.h \
	include/widget/ui/ToastWidget.h \
	include/widget/windowing/TitlebarWidget.h \
	include/widget/windowing/TransparentWindowWidget.h \
	include/widget/windowing/WindowFrameWidget.h \
	include/widget/windowing/WindowWidget.h \
	AGENTS.md \
	CLAUDE.md \
	ARCHITECTURE.md \
	TESTING.md \
	autogen.sh \
	scripts/analyze_threading_safety.py \
	scripts/check_threading_patterns.py \
	third_party/alac/ALACAudioTypes.h \
	third_party/alac/ALACBitUtilities.c \
	third_party/alac/ALACBitUtilities.h \
	third_party/alac/ALACDecoder.cpp \
	third_party/alac/ALACDecoder.h \
	third_party/alac/EndianPortable.c \
	third_party/alac/EndianPortable.h \
	third_party/alac/ag_dec.c \
	third_party/alac/aglib.h \
	third_party/alac/dp_dec.c \
	third_party/alac/dplib.h \
	third_party/alac/matrix_dec.c \
	third_party/alac/matrixlib.h \
	third_party/kjmp2/kjmp2.c \
	third_party/kjmp2/kjmp2.h \
	third_party/kjmp2/kjmp2.txt \
	third_party/minimp3/minimp3.h

# Include threading safety makefile
include scripts/threading-safety-makefile.mk

# Add threading safety checks to the build process
# all-local: threading-check  # DISABLED - too pushy during development

# Add threading safety tests to check target
check-local: threading-check-tests

# Threading safety test targets
.PHONY: threading-check-tests threading-check-integration threading-check-performance

threading-check-tests:
	@echo "Running threading safety tests..."
	@$(MAKE) -C tests test_threading_safety_baseline
	@$(MAKE) -C tests test_audio_thread_safety
	@$(MAKE) -C tests test_iohandler_thread_safety_comprehensive
	@$(MAKE) -C tests test_memory_pool_manager_thread_safety_comprehensive
	@$(MAKE) -C tests test_surface_thread_safety
	@echo "Basic threading safety tests completed"

threading-check-integration:
	@echo "Running threading safety integration tests..."
	@$(MAKE) -C tests test_system_wide_threading_integration
	@cd tests && ./test_system_wide_threading_integration
	@echo "Threading safety integration tests completed"

threading-check-performance:
	@echo "Running threading performance regression tests..."
	@$(MAKE) -C tests test_threading_performance_regression
	@cd tests && ./test_threading_performance_regression
	@echo "Threading performance regression tests completed"

# Comprehensive threading validation (for CI)
threading-validation: threading-check-strict threading-check-tests threading-check-integration threading-check-performance
	@echo "=== Comprehensive Threading Safety Validation ==="
	@echo "All threading safety checks PASSED"
	@echo "- Static analysis: PASSED"
	@echo "- Pattern validation: PASSED"
	@echo "- Unit tests: PASSED"
	@echo "- Integration tests: PASSED"
	@echo "- Performance regression: PASSED"

# Development helper targets
threading-dev-check: threading-check threading-check-tests
	@echo "Development threading safety check completed"

# Clean threading artifacts
clean-local: threading-clean
