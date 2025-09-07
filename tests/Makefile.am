#
# tests/Makefile.am - automake input for test directory
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

SUBDIRS = .

# Common flags for all test programs
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS)
AM_LDFLAGS = $(SDL_LIBS) $(TAGLIB_LIBS) $(FREETYPE_LIBS) $(DBUS_LIBS) $(OPUS_LIBS)

# Common test utilities library
noinst_LIBRARIES = libtest_utilities.a
libtest_utilities_a_SOURCES = test_framework.cpp test_rect_utilities.cpp
libtest_utilities_a_CPPFLAGS = $(AM_CPPFLAGS)

# Test harness executable and debug programs
if BUILD_TEST_HARNESS
noinst_PROGRAMS = test-harness debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_flac_audioframe_creation test_codec_detection test_error_handling
test_harness_SOURCES = test_harness.cpp test_discovery.cpp test_executor.cpp test_reporter.cpp
test_harness_CPPFLAGS = $(AM_CPPFLAGS)
test_harness_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)
else
noinst_PROGRAMS = debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_flac_frame_indexing test_frame_size_estimation_consistency test_flac_audioframe_creation test_codec_detection test_error_handling
endif

debug_dual_flac_SOURCES = debug_dual_flac.cpp
debug_dual_flac_CPPFLAGS = $(AM_CPPFLAGS)
debug_dual_flac_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

debug_bit_extraction_SOURCES = debug_bit_extraction.cpp
debug_bit_extraction_CPPFLAGS = $(AM_CPPFLAGS)
debug_bit_extraction_LDADD = $(AM_LDFLAGS)

debug_flac_seeking_SOURCES = debug_flac_seeking.cpp
debug_flac_seeking_CPPFLAGS = $(AM_CPPFLAGS)
debug_flac_seeking_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iohandler_seek_comprehensive_SOURCES = test_iohandler_seek_comprehensive.cpp
test_iohandler_seek_comprehensive_CPPFLAGS = $(AM_CPPFLAGS)
test_iohandler_seek_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iohandler_buffer_analysis_SOURCES = test_iohandler_buffer_analysis.cpp
test_iohandler_buffer_analysis_CPPFLAGS = $(AM_CPPFLAGS)
test_iohandler_buffer_analysis_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_debug_iohandler_position_SOURCES = test_debug_iohandler_position.cpp
test_debug_iohandler_position_CPPFLAGS = $(AM_CPPFLAGS)
test_debug_iohandler_position_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_flac_frame_indexing_SOURCES = test_flac_frame_indexing.cpp
test_flac_frame_indexing_CPPFLAGS = $(AM_CPPFLAGS)
test_flac_frame_indexing_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

# Individual test executables
check_PROGRAMS = \
	test_rect_area_validation \
	test_rect_containment \
	test_rect_intersection \
	test_rect_union \
	test_rect_centering \
	test_rect_centering_overflow \
	test_rect_expansion \
	test_rect_transformation \
	test_rect_normalization \
	test_rect_modern_cpp \
	test_rect_backward_compatibility \
	test_rect_performance \
	test_widget_rect_integration \
	test_harness_integration \
	test_iso_fragment_handler \
	test_iso_fragmented_mp4 \
	test_iohandler_integration_simple \
	test_iohandler_minimal \
	test_iohandler_basic \
	test_integration_verification \
	test_iohandler_basic \
	test_iohandler_demuxer_integration \
	test_iohandler_demuxer_integration_simple \
	test_psymp3_system_integration \
	test_api_consistency \
	test_demuxer_unit \
	test_demuxer_factory_unit \
	test_media_factory_unit \
	test_demuxed_stream_unit \
	test_demuxer_integration \
	test_media_factory_integration \
	test_demuxer_performance \
	test_iohandler_legacy_compatibility \
	test_iohandler_performance_validation

# OggDemuxer tests (conditional on HAVE_OGGDEMUXER)
if HAVE_OGGDEMUXER
check_PROGRAMS += test_page_extraction test_bisection_search test_granule_arithmetic test_codec_detection test_duration_calculation test_time_conversion test_data_streaming test_error_handling test_memory_safety test_seeking_integration test_ogg_demuxer_integration test_oggdemuxer_performance test_oggdemuxer_comprehensive
endif

# A-law codec tests
check_PROGRAMS += test_alaw_table_verification test_alaw_sample_conversion test_alaw_conversion_accuracy

# μ-law codec tests
check_PROGRAMS += test_mulaw_conversion_accuracy

# Codec selection and validation tests
check_PROGRAMS += test_codec_selection_validation_simple

# Performance and thread safety tests
check_PROGRAMS += test_codec_performance test_codec_thread_safety test_codec_concurrent_instances test_codec_performance_simple test_codec_thread_safety_simple test_threading_safety_baseline test_audio_thread_safety test_audio_threading_pattern test_iohandler_thread_safety_comprehensive test_iohandler_memory_deadlock_prevention test_memory_pool_manager_integration test_memory_pool_manager_thread_safety_comprehensive test_memory_pool_manager_basic_threading test_surface_thread_safety test_surface_performance_regression test_system_wide_threading_integration test_threading_performance_regression

# SimplePCMCodec integration tests
check_PROGRAMS += test_simplepcmcodec_integration_minimal test_simplepcmcodec_real_integration test_simplepcmcodec_comprehensive_integration

# Conditional compilation tests
check_PROGRAMS += test_conditional_codec_compilation

# MPRIS tests (conditional on HAVE_DBUS)
if HAVE_DBUS
check_PROGRAMS += test_mpris_types test_dbus_connection_manager
endif

# FLAC demuxer compatibility tests (conditional on HAVE_FLAC)
if HAVE_FLAC
check_PROGRAMS += test_flac_demuxer_simple test_flac_real_file test_flac_diagnostic test_iohandler_deadlock test_flac_demuxer_compatibility test_flac_demuxer_performance test_flac_compatibility_integration test_flac_demuxer_thread_safety test_flac_demuxer_threading test_flac_demuxer_unit_comprehensive test_flac_demuxer_integration_comprehensive test_flac_demuxer_unit_fixed test_flac_index_based_seeking test_flac_binary_search_limitations test_flac_variable_block_size test_flac_codec_minimal_container_agnostic test_flac_codec_container_agnostic test_flac_codec_streaminfo_only test_flac_codec_compatibility test_flac_codec_integration test_flac_codec_threading_safety test_flac_quality_validation test_flac_quality_simple test_flac_codec_unit_minimal test_flac_codec_unit_comprehensive test_flac_codec_error_handling test_flac_codec_multithreaded test_flac_codec_performance_comprehensive test_flac_codec_demuxer_integration test_flac_performance_benchmarking test_flac_performance_optimization test_flac_performance_with_real_files test_flac_conditional_compilation test_flac_comprehensive_validation test_flac_rfc_compliance test_flac_test_data_validation
endif

# Test program sources and dependencies
test_rect_area_validation_SOURCES = test_rect_area_validation.cpp
test_rect_area_validation_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_containment_SOURCES = test_rect_containment.cpp
test_rect_containment_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_intersection_SOURCES = test_rect_intersection.cpp
test_rect_intersection_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_union_SOURCES = test_rect_union.cpp
test_rect_union_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_centering_SOURCES = test_rect_centering.cpp
test_rect_centering_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_centering_overflow_SOURCES = test_rect_centering_overflow.cpp
test_rect_centering_overflow_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_expansion_SOURCES = test_rect_expansion.cpp
test_rect_expansion_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_transformation_SOURCES = test_rect_transformation.cpp
test_rect_transformation_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_normalization_SOURCES = test_rect_normalization.cpp
test_rect_normalization_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_modern_cpp_SOURCES = test_rect_modern_cpp.cpp
test_rect_modern_cpp_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_backward_compatibility_SOURCES = test_rect_backward_compatibility.cpp
test_rect_backward_compatibility_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_performance_SOURCES = test_rect_performance.cpp
test_rect_performance_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_widget_rect_integration_SOURCES = test_widget_rect_integration.cpp
test_widget_rect_integration_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_harness_integration_SOURCES = test_harness_integration.cpp
test_harness_integration_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_iso_fragment_handler_SOURCES = test_iso_fragment_handler.cpp
test_iso_fragment_handler_LDADD = \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iso_fragmented_mp4_SOURCES = test_iso_fragmented_mp4.cpp
test_iso_fragmented_mp4_LDADD = \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iohandler_integration_simple_SOURCES = test_iohandler_integration_simple.cpp
test_iohandler_integration_simple_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iohandler_minimal_SOURCES = test_iohandler_minimal.cpp
test_iohandler_minimal_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iohandler_basic_SOURCES = test_iohandler_basic.cpp
test_iohandler_basic_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_integration_verification_SOURCES = test_integration_verification.cpp
test_integration_verification_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iohandler_demuxer_integration_SOURCES = test_iohandler_demuxer_integration.cpp
test_iohandler_demuxer_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

test_iohandler_demuxer_integration_simple_SOURCES = test_iohandler_demuxer_integration_simple.cpp
test_iohandler_demuxer_integration_simple_LDADD = libtest_utilities.a $(AM_LDFLAGS)

test_psymp3_system_integration_SOURCES = test_psymp3_system_integration.cpp
test_psymp3_system_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/URI.o \
	$(AM_LDFLAGS)

test_api_consistency_SOURCES = test_api_consistency.cpp
test_api_consistency_LDADD = libtest_utilities.a $(AM_LDFLAGS)

# Demuxer architecture unit tests
test_demuxer_unit_SOURCES = test_demuxer_unit.cpp
test_demuxer_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_demuxer_factory_unit_SOURCES = test_demuxer_factory_unit.cpp
test_demuxer_factory_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_media_factory_unit_SOURCES = test_media_factory_unit.cpp
test_media_factory_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/CodecRegistration.o \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/HTTPIOHandler.o \
	$(top_builddir)/src/HTTPClient.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/ModernStream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/flac.o \
	$(top_builddir)/src/libmpg123.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/RawAudioDemuxer.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ChunkDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

test_demuxed_stream_unit_SOURCES = test_demuxed_stream_unit.cpp
test_demuxed_stream_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	-logg -lvorbis -lvorbisenc -lopus \
	$(AM_LDFLAGS)

# Integration tests
test_demuxer_integration_SOURCES = test_demuxer_integration.cpp
test_demuxer_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_media_factory_integration_SOURCES = test_media_factory_integration.cpp
test_media_factory_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# Performance and regression tests
test_demuxer_performance_SOURCES = test_demuxer_performance.cpp
test_demuxer_performance_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# IOHandler backward compatibility and performance validation tests
test_iohandler_legacy_compatibility_SOURCES = test_iohandler_legacy_compatibility.cpp
test_iohandler_legacy_compatibility_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/HTTPIOHandler.o \
	$(top_builddir)/src/HTTPClient.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	-lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

test_iohandler_performance_validation_SOURCES = test_iohandler_performance_validation.cpp
test_iohandler_performance_validation_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/HTTPIOHandler.o \
	$(top_builddir)/src/HTTPClient.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	-lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

# OggDemuxer page extraction test
if HAVE_OGGDEMUXER
test_page_extraction_SOURCES = test_page_extraction.cpp
test_page_extraction_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS)
test_page_extraction_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer bisection search test
test_bisection_search_SOURCES = test_bisection_search.cpp
test_bisection_search_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS)
test_bisection_search_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer granule arithmetic test
test_granule_arithmetic_SOURCES = test_granule_arithmetic.cpp
test_granule_arithmetic_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS)
test_granule_arithmetic_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer codec detection test
test_codec_detection_SOURCES = test_codec_detection.cpp
test_codec_detection_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(OPUS_CFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_codec_detection_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) $(VORBIS_LIBS) $(OPUS_LIBS) $(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer duration calculation test
test_duration_calculation_SOURCES = test_duration_calculation.cpp
test_duration_calculation_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) -std=c++17 -pthread
test_duration_calculation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer time conversion test
test_time_conversion_SOURCES = test_time_conversion.cpp
test_time_conversion_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) -std=c++17 -pthread
test_time_conversion_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer data streaming test
test_data_streaming_SOURCES = test_data_streaming.cpp
test_data_streaming_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) -std=c++17 -pthread
test_data_streaming_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer memory safety test
test_memory_safety_SOURCES = test_memory_safety.cpp
test_memory_safety_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) -std=c++17 -pthread
test_memory_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer error handling test
test_error_handling_SOURCES = test_error_handling.cpp
test_error_handling_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(OPUS_CFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_error_handling_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer seeking integration test
test_seeking_integration_SOURCES = test_seeking_integration.cpp
test_seeking_integration_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(OPUS_CFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_seeking_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer integration test
test_ogg_demuxer_integration_SOURCES = test_ogg_demuxer_integration.cpp
test_ogg_demuxer_integration_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(OPUS_CFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_ogg_demuxer_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
endif

# A-law codec table verification test
test_alaw_table_verification_SOURCES = test_alaw_table_verification.cpp
test_alaw_table_verification_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_ALAW_CODEC
test_alaw_table_verification_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# A-law codec sample conversion test
test_alaw_sample_conversion_SOURCES = test_alaw_sample_conversion.cpp
test_alaw_sample_conversion_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_ALAW_CODEC
test_alaw_sample_conversion_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# A-law codec conversion accuracy test
test_alaw_conversion_accuracy_SOURCES = test_alaw_conversion_accuracy.cpp
test_alaw_conversion_accuracy_CPPFLAGS = $(AM_CPPFLAGS)
test_alaw_conversion_accuracy_LDADD = $(AM_LDFLAGS)

# μ-law codec conversion accuracy test
test_mulaw_conversion_accuracy_SOURCES = test_mulaw_conversion_accuracy.cpp
test_mulaw_conversion_accuracy_CPPFLAGS = $(AM_CPPFLAGS)
test_mulaw_conversion_accuracy_LDADD = $(AM_LDFLAGS)

# Codec selection and validation test (simple)
test_codec_selection_validation_simple_SOURCES = test_codec_selection_validation_simple.cpp
test_codec_selection_validation_simple_CPPFLAGS = $(AM_CPPFLAGS)
test_codec_selection_validation_simple_LDADD = $(AM_LDFLAGS)

# Codec performance test
test_codec_performance_SOURCES = test_codec_performance.cpp
test_codec_performance_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# Codec thread safety test
test_codec_thread_safety_SOURCES = test_codec_thread_safety.cpp
test_codec_thread_safety_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# Codec concurrent instances test
test_codec_concurrent_instances_SOURCES = test_codec_concurrent_instances.cpp
test_codec_concurrent_instances_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_concurrent_instances_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# MPRIS types test (conditional on HAVE_DBUS)
if HAVE_DBUS
test_mpris_types_SOURCES = test_mpris_types.cpp
test_mpris_types_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_types_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_dbus_connection_manager_SOURCES = test_dbus_connection_manager.cpp
test_dbus_connection_manager_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_dbus_connection_manager_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)
endif




# Test execution target
check-local: $(check_PROGRAMS)
if BUILD_TEST_HARNESS
	./test-harness --verbose
else
	@echo "Running individual tests..."
	@for test in $(check_PROGRAMS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
endif

# Clean up test executables
CLEANFILES = $(check_PROGRAMS)
if BUILD_TEST_HARNESS
CLEANFILES += test-harness
endif

# Ensure required object files are built before tests
$(check_PROGRAMS): $(top_builddir)/src/rect.o

$(top_builddir)/src/rect.o:
	$(MAKE) -C $(top_builddir)/src rect.o

# Build rules for OggDemuxer test dependencies
if HAVE_OGGDEMUXER
$(top_builddir)/src/OggDemuxer.o:
	$(MAKE) -C $(top_builddir)/src OggDemuxer.o

$(top_builddir)/src/Demuxer.o:
	$(MAKE) -C $(top_builddir)/src Demuxer.o

$(top_builddir)/src/IOHandler.o:
	$(MAKE) -C $(top_builddir)/src IOHandler.o

$(top_builddir)/src/FileIOHandler.o:
	$(MAKE) -C $(top_builddir)/src FileIOHandler.o

$(top_builddir)/src/URI.o:
	$(MAKE) -C $(top_builddir)/src URI.o

$(top_builddir)/src/debug.o:
	$(MAKE) -C $(top_builddir)/src debug.o

$(top_builddir)/src/exceptions.o:
	$(MAKE) -C $(top_builddir)/src exceptions.o

$(top_builddir)/src/MemoryPoolManager.o:
	$(MAKE) -C $(top_builddir)/src MemoryPoolManager.o

$(top_builddir)/src/MemoryTracker.o:
	$(MAKE) -C $(top_builddir)/src MemoryTracker.o

$(top_builddir)/src/BufferPool.o:
	$(MAKE) -C $(top_builddir)/src BufferPool.o

$(top_builddir)/src/EnhancedBufferPool.o:
	$(MAKE) -C $(top_builddir)/src EnhancedBufferPool.o

$(top_builddir)/src/BoundedBuffer.o:
	$(MAKE) -C $(top_builddir)/src BoundedBuffer.o

$(top_builddir)/src/RAIIFileHandle.o:
	$(MAKE) -C $(top_builddir)/src RAIIFileHandle.o

$(top_builddir)/src/MemoryOptimizer.o:
	$(MAKE) -C $(top_builddir)/src MemoryOptimizer.o

$(top_builddir)/src/HTTPIOHandler.o:
	$(MAKE) -C $(top_builddir)/src HTTPIOHandler.o

$(top_builddir)/src/HTTPClient.o:
	$(MAKE) -C $(top_builddir)/src HTTPClient.o
endif

# Build rules for A-law codec test dependencies
$(top_builddir)/src/ALawCodec.o:
	$(MAKE) -C $(top_builddir)/src ALawCodec.o

$(top_builddir)/src/AudioCodec.o:
	$(MAKE) -C $(top_builddir)/src AudioCodec.o

# Build rules for μ-law codec test dependencies
$(top_builddir)/src/MuLawCodec.o:
	$(MAKE) -C $(top_builddir)/src MuLawCodec.o

$(top_builddir)/src/PCMCodecs.o:
	$(MAKE) -C $(top_builddir)/src PCMCodecs.o

# Build rules for Audio thread safety test dependencies
$(top_builddir)/src/audio.o:
	$(MAKE) -C $(top_builddir)/src audio.o

$(top_builddir)/src/stream.o:
	$(MAKE) -C $(top_builddir)/src stream.o

$(top_builddir)/src/fft.o:
	$(MAKE) -C $(top_builddir)/src fft.o

$(top_builddir)/src/fft_draw.o:
	$(MAKE) -C $(top_builddir)/src fft_draw.o

$(top_builddir)/src/system.o:
	$(MAKE) -C $(top_builddir)/src system.o

$(top_builddir)/src/track.o:
	$(MAKE) -C $(top_builddir)/src track.o

$(top_builddir)/src/lyrics.o:
	$(MAKE) -C $(top_builddir)/src lyrics.o

$(top_builddir)/src/TagLibIOHandlerAdapter.o:
	$(MAKE) -C $(top_builddir)/src TagLibIOHandlerAdapter.o

# Build rules for Surface thread safety test dependencies
$(top_builddir)/src/surface.o:
	$(MAKE) -C $(top_builddir)/src surface.o

# Build rules for ISO demuxer test dependencies
$(top_builddir)/src/ISODemuxer.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxer.o

$(top_builddir)/src/ISODemuxerFragmentHandler.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxerFragmentHandler.o

$(top_builddir)/src/ISODemuxerBoxParser.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxerBoxParser.o

$(top_builddir)/src/ISODemuxerSampleTableManager.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxerSampleTableManager.o

$(top_builddir)/src/ISODemuxerSeekingEngine.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxerSeekingEngine.o

$(top_builddir)/src/ISODemuxerMetadataExtractor.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxerMetadataExtractor.o

$(top_builddir)/src/ISODemuxerStreamManager.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxerStreamManager.o

$(top_builddir)/src/ISODemuxerErrorRecovery.o:
	$(MAKE) -C $(top_builddir)/src ISODemuxerErrorRecovery.o

# System-wide threading integration test
test_system_wide_threading_integration_SOURCES = test_system_wide_threading_integration.cpp
test_system_wide_threading_integration_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_system_wide_threading_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/fft.o \
	$(top_builddir)/src/fft_draw.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

# Threading performance regression test
test_threading_performance_regression_SOURCES = test_threading_performance_regression.cpp
test_threading_performance_regression_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_threading_performance_regression_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/fft.o \
	$(top_builddir)/src/fft_draw.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

# Simple codec performance test (minimal dependencies)
test_codec_performance_simple_SOURCES = test_codec_performance_simple.cpp
test_codec_performance_simple_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_performance_simple_LDADD = $(AM_LDFLAGS)

# Simple codec thread safety test (minimal dependencies)
test_codec_thread_safety_simple_SOURCES = test_codec_thread_safety_simple.cpp
test_codec_thread_safety_simple_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_thread_safety_simple_LDADD = $(AM_LDFLAGS)

# Threading safety baseline test
test_threading_safety_baseline_SOURCES = test_threading_safety_baseline.cpp
test_threading_safety_baseline_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_threading_safety_baseline_LDADD = $(AM_LDFLAGS)

# Audio thread safety test
test_audio_thread_safety_SOURCES = test_audio_thread_safety.cpp
test_audio_thread_safety_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_audio_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/fft.o \
	$(top_builddir)/src/fft_draw.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

# Audio threading pattern test (simpler test without SDL dependencies)
test_audio_threading_pattern_SOURCES = test_audio_threading_pattern.cpp
test_audio_threading_pattern_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_audio_threading_pattern_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# Surface thread safety test
test_surface_thread_safety_SOURCES = test_surface_thread_safety.cpp
test_surface_thread_safety_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_surface_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

# Surface performance regression test
test_surface_performance_regression_SOURCES = test_surface_performance_regression.cpp
test_surface_performance_regression_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_surface_performance_regression_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

# SimplePCMCodec integration test (minimal version)
test_simplepcmcodec_integration_minimal_SOURCES = test_simplepcmcodec_integration_minimal.cpp
test_simplepcmcodec_integration_minimal_CPPFLAGS = $(AM_CPPFLAGS)
test_simplepcmcodec_integration_minimal_LDADD = $(AM_LDFLAGS)

# SimplePCMCodec real integration test (with MuLaw/ALaw codecs)
test_simplepcmcodec_real_integration_SOURCES = test_simplepcmcodec_real_integration.cpp
test_simplepcmcodec_real_integration_CPPFLAGS = $(AM_CPPFLAGS)
test_simplepcmcodec_real_integration_LDADD = $(AM_LDFLAGS)

# SimplePCMCodec comprehensive integration test (minimal dependencies)
test_simplepcmcodec_comprehensive_integration_SOURCES = test_simplepcmcodec_comprehensive_integration.cpp
test_simplepcmcodec_comprehensive_integration_CPPFLAGS = $(AM_CPPFLAGS)
test_simplepcmcodec_comprehensive_integration_LDADD = $(AM_LDFLAGS)

# Conditional codec compilation test
test_conditional_codec_compilation_SOURCES = test_conditional_codec_compilation.cpp
test_conditional_codec_compilation_CPPFLAGS = $(AM_CPPFLAGS)
test_conditional_codec_compilation_LDADD = libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# Note: This test only checks conditional compilation flags
# No codec object files are needed since we don't instantiate the classes

# FLAC demuxer simple test (minimal dependencies)
if HAVE_FLAC
test_flac_demuxer_simple_SOURCES = test_flac_demuxer_simple.cpp
test_flac_demuxer_simple_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_demuxer_simple_LDADD = \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer real file test
test_flac_real_file_SOURCES = test_flac_real_file.cpp
test_flac_real_file_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_real_file_LDADD = \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer diagnostic test
test_flac_diagnostic_SOURCES = test_flac_diagnostic.cpp
test_flac_diagnostic_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_diagnostic_LDADD = \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# IOHandler deadlock test
test_iohandler_deadlock_SOURCES = test_iohandler_deadlock.cpp
test_iohandler_deadlock_CPPFLAGS = $(AM_CPPFLAGS)
test_iohandler_deadlock_LDADD = \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

# FLAC demuxer compatibility test
test_flac_demuxer_compatibility_SOURCES = test_flac_demuxer_compatibility.cpp
test_flac_demuxer_compatibility_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_demuxer_compatibility_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)



# FLAC demuxer performance test
test_flac_demuxer_performance_SOURCES = test_flac_demuxer_performance.cpp
test_flac_demuxer_performance_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_demuxer_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC compatibility integration test
test_flac_compatibility_integration_SOURCES = test_flac_compatibility_integration.cpp
test_flac_compatibility_integration_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_compatibility_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer thread safety test
test_flac_demuxer_thread_safety_SOURCES = test_flac_demuxer_thread_safety.cpp
test_flac_demuxer_thread_safety_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_demuxer_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer comprehensive unit tests
test_flac_demuxer_unit_comprehensive_SOURCES = test_flac_demuxer_unit_comprehensive.cpp
test_flac_demuxer_unit_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_demuxer_unit_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer comprehensive integration tests
test_flac_demuxer_integration_comprehensive_SOURCES = test_flac_demuxer_integration_comprehensive.cpp
test_flac_demuxer_integration_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_demuxer_integration_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer unit tests (fixed, no external files required)
test_flac_demuxer_unit_fixed_SOURCES = test_flac_demuxer_unit_fixed.cpp
test_flac_demuxer_unit_fixed_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_demuxer_unit_fixed_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC index-based seeking test
test_flac_index_based_seeking_SOURCES = test_flac_index_based_seeking.cpp
test_flac_index_based_seeking_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_index_based_seeking_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec variable block size test
test_flac_variable_block_size_SOURCES = test_flac_variable_block_size.cpp
test_flac_variable_block_size_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_variable_block_size_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec minimal container-agnostic test
test_flac_codec_minimal_container_agnostic_SOURCES = test_flac_codec_minimal_container_agnostic.cpp
test_flac_codec_minimal_container_agnostic_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_minimal_container_agnostic_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec container-agnostic operation test
test_flac_codec_container_agnostic_SOURCES = test_flac_codec_container_agnostic.cpp
test_flac_codec_container_agnostic_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_container_agnostic_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lopus \
	$(AM_LDFLAGS)

# FLAC codec StreamInfo-only initialization test
test_flac_codec_streaminfo_only_SOURCES = test_flac_codec_streaminfo_only.cpp
test_flac_codec_streaminfo_only_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_streaminfo_only_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lopus \
	$(AM_LDFLAGS)

# Build rules for FLAC demuxer test dependencies
$(top_builddir)/src/FLACDemuxer.o:
	$(MAKE) -C $(top_builddir)/src FLACDemuxer.o

$(top_builddir)/src/FLACCodec.o:
	$(MAKE) -C $(top_builddir)/src FLACCodec.o

# Additional build rules for FLAC tests (non-duplicated)
$(top_builddir)/src/EnhancedAudioBufferPool.o:
	$(MAKE) -C $(top_builddir)/src EnhancedAudioBufferPool.o

$(top_builddir)/src/OggCodecs.o:
	$(MAKE) -C $(top_builddir)/src OggCodecs.o

$(top_builddir)/src/vorbis.o:
	$(MAKE) -C $(top_builddir)/src vorbis.o

$(top_builddir)/src/opus.o:
	$(MAKE) -C $(top_builddir)/src opus.o

$(top_builddir)/src/DemuxedStream.o:
	$(MAKE) -C $(top_builddir)/src DemuxedStream.o

$(top_builddir)/src/DemuxerFactory.o:
	$(MAKE) -C $(top_builddir)/src DemuxerFactory.o

$(top_builddir)/src/MediaFactory.o:
	$(MAKE) -C $(top_builddir)/src MediaFactory.o

# Simple FLAC codec quality validation test (minimal dependencies)
test_flac_quality_simple_SOURCES = test_flac_quality_simple.cpp
test_flac_quality_simple_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17
test_flac_quality_simple_LDADD = \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# Minimal FLAC codec unit tests
test_flac_codec_unit_minimal_SOURCES = test_flac_codec_unit_minimal.cpp
test_flac_codec_unit_minimal_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17
test_flac_codec_unit_minimal_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

endif

# IOHandler thread safety comprehensive test
test_iohandler_thread_safety_comprehensive_SOURCES = test_iohandler_thread_safety_comprehensive.cpp
test_iohandler_thread_safety_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_iohandler_thread_safety_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

# IOHandler memory deadlock prevention test
test_iohandler_memory_deadlock_prevention_SOURCES = test_iohandler_memory_deadlock_prevention.cpp
test_iohandler_memory_deadlock_prevention_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_iohandler_memory_deadlock_prevention_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)

# MemoryPoolManager integration test
test_memory_pool_manager_integration_SOURCES = test_memory_pool_manager_integration.cpp
test_memory_pool_manager_integration_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_memory_pool_manager_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# MemoryPoolManager comprehensive thread safety test
test_memory_pool_manager_thread_safety_comprehensive_SOURCES = test_memory_pool_manager_thread_safety_comprehensive.cpp
test_memory_pool_manager_thread_safety_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_memory_pool_manager_thread_safety_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# MemoryPoolManager basic threading test
test_memory_pool_manager_basic_threading_SOURCES = test_memory_pool_manager_basic_threading.cpp
test_memory_pool_manager_basic_threading_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_memory_pool_manager_basic_threading_LDADD = \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_frame_size_estimation_consistency_SOURCES = test_frame_size_estimation_consistency.cpp
test_frame_size_estimation_consistency_CPPFLAGS = $(AM_CPPFLAGS)
test_frame_size_estimation_consistency_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(AM_LDFLAGS)
# FLAC AudioFrame creation test
test_flac_audioframe_creation_SOURCES = test_flac_audioframe_creation.cpp
test_flac_audioframe_creation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_audioframe_creation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/libmpg123.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(FLAC_LIBS) -logg -lvorbis -lvorbisenc -lmpg123 -lopus \
	$(AM_LDFLAGS)

# Note: FLACCodec.o and EnhancedAudioBufferPool.o build rules already defined above

# FLAC codec compatibility test
test_flac_codec_compatibility_SOURCES = test_flac_codec_compatibility.cpp
test_flac_codec_compatibility_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_compatibility_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec integration test
test_flac_codec_integration_SOURCES = test_flac_codec_integration.cpp
test_flac_codec_integration_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec quality validation and accuracy test
test_flac_quality_validation_SOURCES = test_flac_quality_validation.cpp
test_flac_quality_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_quality_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
# Comprehensive FLAC codec unit tests
test_flac_codec_unit_comprehensive_SOURCES = test_flac_codec_unit_comprehensive.cpp
test_flac_codec_unit_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_unit_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec error handling tests
test_flac_codec_error_handling_SOURCES = test_flac_codec_error_handling.cpp
test_flac_codec_error_handling_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_error_handling_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec performance comprehensive tests
test_flac_codec_performance_comprehensive_SOURCES = test_flac_codec_performance_comprehensive.cpp
test_flac_codec_performance_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_performance_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec multi-threaded tests
test_flac_codec_multithreaded_SOURCES = test_flac_codec_multithreaded.cpp
test_flac_codec_multithreaded_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_multithreaded_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec demuxer integration tests
test_flac_codec_demuxer_integration_SOURCES = test_flac_codec_demuxer_integration.cpp
test_flac_codec_demuxer_integration_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) $(OGG_CFLAGS) -std=c++17 -pthread
test_flac_codec_demuxer_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec performance benchmarking tests
test_flac_performance_benchmarking_SOURCES = test_flac_performance_benchmarking.cpp
test_flac_performance_benchmarking_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_performance_benchmarking_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACPerformanceBenchmark.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

# FLAC codec conditional compilation tests
test_flac_conditional_compilation_SOURCES = test_flac_conditional_compilation.cpp
test_flac_conditional_compilation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_conditional_compilation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

# FLAC binary search limitations test
test_flac_binary_search_limitations_SOURCES = test_flac_binary_search_limitations.cpp
test_flac_binary_search_limitations_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_binary_search_limitations_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec threading safety test
test_flac_codec_threading_safety_SOURCES = test_flac_codec_threading_safety.cpp
test_flac_codec_threading_safety_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_threading_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer threading test
test_flac_demuxer_threading_SOURCES = test_flac_demuxer_threading.cpp
test_flac_demuxer_threading_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_demuxer_threading_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC performance optimization test
test_flac_performance_optimization_SOURCES = test_flac_performance_optimization.cpp
test_flac_performance_optimization_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_performance_optimization_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC performance with real files test
test_flac_performance_with_real_files_SOURCES = test_flac_performance_with_real_files.cpp
test_flac_performance_with_real_files_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_performance_with_real_files_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC comprehensive validation test
test_flac_comprehensive_validation_SOURCES = test_flac_comprehensive_validation.cpp
test_flac_comprehensive_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_comprehensive_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC RFC 9639 compliance validation test
test_flac_rfc_compliance_SOURCES = test_flac_rfc_compliance.cpp
test_flac_rfc_compliance_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_rfc_compliance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC test data validation test
test_flac_test_data_validation_SOURCES = test_flac_test_data_validation.cpp
test_flac_test_data_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_test_data_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer performance test
test_oggdemuxer_performance_SOURCES = test_oggdemuxer_performance.cpp
test_oggdemuxer_performance_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(OPUS_CFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_oggdemuxer_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer comprehensive test suite
test_oggdemuxer_comprehensive_SOURCES = test_oggdemuxer_comprehensive.cpp
test_oggdemuxer_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(OPUS_CFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_oggdemuxer_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OggDemuxer.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)