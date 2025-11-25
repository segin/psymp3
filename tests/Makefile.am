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

# Define optional codec/demuxer libraries (same as main binary)
CODEC_LIBS =
DEMUXER_LIBS =

if HAVE_FLAC
DEMUXER_LIBS += $(top_builddir)/src/demuxer/flac/libflacdemuxer.a
if HAVE_NATIVE_FLAC
CODEC_LIBS += $(top_builddir)/src/codecs/flac/libnativeflac.a
else
CODEC_LIBS += $(top_builddir)/src/codecs/flac/libflaccodec.a
endif
endif

if HAVE_MP3
CODEC_LIBS += $(top_builddir)/src/codecs/mp3/libmp3codec.a
endif

if HAVE_VORBIS
CODEC_LIBS += $(top_builddir)/src/codecs/vorbis/libvorbiscodec.a
endif

if HAVE_OPUS
CODEC_LIBS += $(top_builddir)/src/codecs/opus/libopuscodec.a
endif

if HAVE_OGGDEMUXER
DEMUXER_LIBS += $(top_builddir)/src/demuxer/ogg/liboggdemuxer.a
endif

# Common library set for tests (mirrors main binary linking)
# Order: high-level -> mid-level -> low-level -> system libs
COMMON_TEST_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/widget/ui/libwidgetui.a \
	$(top_builddir)/src/widget/windowing/libwidgetwindowing.a \
	$(top_builddir)/src/widget/foundation/libwidgetfoundation.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/http/libhttpio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/lastfm/liblastfm.a \
	$(top_builddir)/src/mpris/libmpris.a \
	$(top_builddir)/src/codecs/libcodecs.a \
	$(CODEC_LIBS) \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libisodemuxer.a \
	$(top_builddir)/src/demuxer/riff/libriffdemuxer.a \
	$(top_builddir)/src/demuxer/raw/librawdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/codecs/pcm/libpcmcodecs.a \
	$(top_builddir)/src/codecs/libcodecs.a

# Minimal library set for simple tests (I/O, demuxer core, codecs)
MINIMAL_TEST_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/codecs/libcodecs.a \
	$(CODEC_LIBS) \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpcmcodecs.a

# Legacy variable for backward compatibility (deprecated - use COMMON_TEST_LIBS)
COMMON_CODEC_DEPS = \
	$(top_builddir)/src/exceptions.o

# Common library links for codec tests
COMMON_CODEC_LIBS = -logg -lvorbis -lvorbisenc -lmpg123
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS)
AM_LDFLAGS = $(SDL_LIBS) $(TAGLIB_LIBS) $(FREETYPE_LIBS) $(DBUS_LIBS) $(OPUS_LIBS)

# Common test utilities library
noinst_LIBRARIES = libtest_utilities.a
libtest_utilities_a_SOURCES = test_framework.cpp test_rect_utilities.cpp test_framework_threading.cpp
if HAVE_DBUS
libtest_utilities_a_SOURCES += mock_dbus_connection.cpp mock_player.cpp mpris_test_fixtures.cpp
endif
libtest_utilities_a_CPPFLAGS = $(AM_CPPFLAGS)

# Test harness executable and debug programs
if BUILD_TEST_HARNESS
noinst_PROGRAMS = test-harness debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_seek_deadlock test_flac_audioframe_creation test_codec_detection test_error_handling test_mulaw_codec_properties test_alaw_codec_properties test_mulaw_alaw_error_state_consistency
test_harness_SOURCES = test_harness.cpp test_discovery.cpp test_executor.cpp test_reporter.cpp
test_harness_CPPFLAGS = $(AM_CPPFLAGS)
test_harness_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)
else
noinst_PROGRAMS = debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_seek_deadlock test_flac_frame_indexing test_frame_size_estimation_consistency test_flac_audioframe_creation test_codec_detection test_error_handling test_flac_streamable_subset_real_files test_mulaw_codec_properties test_alaw_codec_properties test_mulaw_alaw_error_state_consistency
endif

debug_dual_flac_SOURCES = debug_dual_flac.cpp
debug_dual_flac_CPPFLAGS = $(AM_CPPFLAGS)
debug_dual_flac_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_seek_deadlock_SOURCES = test_seek_deadlock.cpp
test_seek_deadlock_CPPFLAGS = $(AM_CPPFLAGS)
test_seek_deadlock_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
check_PROGRAMS += test_page_extraction test_bisection_search test_granule_arithmetic test_codec_detection test_duration_calculation test_time_conversion test_data_streaming test_error_handling test_memory_safety test_seeking_integration test_ogg_demuxer_integration test_oggdemuxer_performance test_oggdemuxer_comprehensive test_opus_codec_core_decoding test_opus_codec_error_handling test_opus_integration test_opus_compatibility_performance
endif

# A-law codec tests
check_PROGRAMS += test_alaw_table_verification test_alaw_sample_conversion test_alaw_conversion_accuracy test_alaw_codec_properties

# μ-law codec tests
check_PROGRAMS += test_mulaw_conversion_accuracy

# Codec selection and validation tests
check_PROGRAMS += test_codec_selection_validation_simple test_mulaw_alaw_codec_selection test_mulaw_alaw_error_handling

# Performance and thread safety tests
check_PROGRAMS += test_codec_performance test_mulaw_alaw_performance test_codec_thread_safety test_codec_concurrent_instances test_codec_performance_simple test_codec_thread_safety_simple test_threading_safety_baseline test_audio_thread_safety test_audio_threading_pattern test_iohandler_thread_safety_comprehensive test_iohandler_memory_deadlock_prevention test_memory_pool_manager_integration test_memory_pool_manager_thread_safety_comprehensive test_memory_pool_manager_basic_threading test_surface_thread_safety test_surface_performance_regression test_system_wide_threading_integration test_threading_performance_regression

# SimplePCMCodec integration tests
check_PROGRAMS += test_simplepcmcodec_integration_minimal test_simplepcmcodec_real_integration test_simplepcmcodec_comprehensive_integration test_mulaw_alaw_integration

# Conditional compilation tests
check_PROGRAMS += test_conditional_codec_compilation

# MPRIS tests (conditional on HAVE_DBUS)
if HAVE_DBUS
check_PROGRAMS += test_mpris_types test_dbus_connection_manager test_property_manager test_method_handler test_signal_emitter test_mpris_manager_integration test_mpris_manager_simple test_mpris_manager_minimal test_mpris_manager_basic_integration test_mpris_error_handling test_mpris_error_integration test_mpris_mock_framework test_mpris_stress_testing test_comprehensive_mock_framework test_mock_framework_standalone test_dbus_connection_manager_comprehensive test_property_manager_comprehensive test_method_handler_comprehensive test_method_handler_simple test_signal_emitter_comprehensive test_mpris_manager_comprehensive test_mpris_logger_basic test_mpris_logger_dbus_tracing test_mpris_logger_performance test_mpris_spec_compliance test_mpris_concurrent_clients test_mpris_reconnection_behavior test_mpris_memory_validation test_mpris_performance_profiler test_mpris_regression_validation
endif

# FLAC demuxer compatibility tests (conditional on HAVE_FLAC)
if HAVE_FLAC
check_PROGRAMS += test_flac_demuxer_simple test_flac_real_file test_flac_diagnostic test_iohandler_deadlock test_flac_demuxer_compatibility test_flac_demuxer_performance test_flac_compatibility_integration test_flac_demuxer_thread_safety test_flac_demuxer_threading test_flac_demuxer_unit_comprehensive test_flac_demuxer_integration_comprehensive test_flac_demuxer_unit_fixed test_flac_index_based_seeking test_flac_thread_safety_validation test_flac_binary_search_limitations test_flac_variable_block_size test_flac_codec_minimal_container_agnostic test_flac_codec_container_agnostic test_flac_codec_streaminfo_only test_flac_codec_compatibility test_flac_codec_integration test_flac_codec_threading_safety test_flac_quality_validation test_flac_quality_simple test_flac_codec_unit_minimal test_flac_codec_unit_comprehensive test_flac_codec_error_handling test_flac_codec_multithreaded test_flac_codec_performance_comprehensive test_flac_codec_demuxer_integration test_flac_performance_benchmarking test_flac_performance_optimization test_flac_performance_with_real_files test_flac_conditional_compilation test_flac_comprehensive_validation test_flac_rfc_compliance test_flac_rfc_subframe_validation test_flac_rfc_entropy_coding_validation test_entropy_coding_compilation test_flac_test_data_validation test_flac_crc_validation test_flac_rfc_bit_depth_validation test_flac_bit_depth_validation_simple test_flac_sample_format_rfc9639_compliance test_flac_rfc9639_compliance_validator test_flac_audio_output test_flac_seeking_crash test_flac_audio_output_debug test_flac_codec_simple_debug test_streaminfo_real_files test_streaminfo_recovery test_flac_streamable_subset_real_files
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
	$(top_builddir)/src/demuxer/iso/libisodemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/iso/libisodemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(AM_LDFLAGS)

test_api_consistency_SOURCES = test_api_consistency.cpp
test_api_consistency_LDADD = libtest_utilities.a $(AM_LDFLAGS)

# Demuxer architecture unit tests
test_demuxer_unit_SOURCES = test_demuxer_unit.cpp
test_demuxer_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
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
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/RawAudioDemuxer.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/FLACRFCValidator.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/ChunkDemuxer.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

test_demuxed_stream_unit_SOURCES = test_demuxed_stream_unit.cpp
test_demuxed_stream_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/OpusCodec.o \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_media_factory_integration_SOURCES = test_media_factory_integration.cpp
test_media_factory_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/file/libfileio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/RawAudioDemuxer.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/FLACRFCValidator.o \
	$(top_builddir)/src/ChunkDemuxer.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

# Performance and regression tests
test_demuxer_performance_SOURCES = test_demuxer_performance.cpp
test_demuxer_performance_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# IOHandler backward compatibility and performance validation tests
test_iohandler_legacy_compatibility_SOURCES = test_iohandler_legacy_compatibility.cpp
test_iohandler_legacy_compatibility_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/HTTPIOHandler.o \
	$(top_builddir)/src/HTTPClient.o \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/HTTPIOHandler.o \
	$(top_builddir)/src/HTTPClient.o \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/codecs/libcodecs.a \
	$(top_builddir)/src/codecs/flac/libflaccodec.a \
	$(top_builddir)/src/codecs/mp3/libmp3codec.a \
	$(top_builddir)/src/codecs/vorbis/libvorbiscodec.a \
	$(top_builddir)/src/codecs/opus/libopuscodec.a \
	$(top_builddir)/src/codecs/pcm/libpcmcodecs.a \
	$(top_builddir)/src/codecs/libcodecs.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	-lFLAC++ -lFLAC -lmpg123 -lvorbis -lopus -logg \
	$(AM_LDFLAGS)

# A-law codec sample conversion test
test_alaw_sample_conversion_SOURCES = test_alaw_sample_conversion.cpp
test_alaw_sample_conversion_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_ALAW_CODEC
test_alaw_sample_conversion_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# A-law codec conversion accuracy test
test_alaw_conversion_accuracy_SOURCES = test_alaw_conversion_accuracy.cpp
test_alaw_conversion_accuracy_CPPFLAGS = $(AM_CPPFLAGS)
test_alaw_conversion_accuracy_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# μ-law codec conversion accuracy test
test_mulaw_conversion_accuracy_SOURCES = test_mulaw_conversion_accuracy.cpp
test_mulaw_conversion_accuracy_CPPFLAGS = $(AM_CPPFLAGS)
test_mulaw_conversion_accuracy_LDADD = $(AM_LDFLAGS)

# Codec selection and validation test (simple)
test_codec_selection_validation_simple_SOURCES = test_codec_selection_validation_simple.cpp
test_codec_selection_validation_simple_CPPFLAGS = $(AM_CPPFLAGS)
test_codec_selection_validation_simple_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# Codec performance test
test_codec_performance_SOURCES = test_codec_performance.cpp
test_codec_performance_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_performance_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# μ-law/A-law codec performance test
test_mulaw_alaw_performance_SOURCES = test_mulaw_alaw_performance.cpp
test_mulaw_alaw_performance_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_mulaw_alaw_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/libcodecs.a \
	$(top_builddir)/src/codecs/flac/libflaccodec.a \
	$(top_builddir)/src/codecs/mp3/libmp3codec.a \
	$(top_builddir)/src/codecs/vorbis/libvorbiscodec.a \
	$(top_builddir)/src/codecs/opus/libopuscodec.a \
	$(top_builddir)/src/codecs/pcm/libpcmcodecs.a \
	$(top_builddir)/src/codecs/libcodecs.a \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	-lFLAC++ -lFLAC -lmpg123 -lvorbis -lopus -logg \
	$(AM_LDFLAGS)

# Codec thread safety test
test_codec_thread_safety_SOURCES = test_codec_thread_safety.cpp
test_codec_thread_safety_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_thread_safety_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# Codec concurrent instances test
test_codec_concurrent_instances_SOURCES = test_codec_concurrent_instances.cpp
test_codec_concurrent_instances_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_concurrent_instances_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

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

test_property_manager_SOURCES = test_property_manager.cpp
test_property_manager_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_property_manager_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_method_handler_SOURCES = test_method_handler.cpp
test_method_handler_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_method_handler_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_signal_emitter_SOURCES = test_signal_emitter.cpp
test_signal_emitter_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_signal_emitter_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_manager_integration_SOURCES = test_mpris_manager_integration.cpp
test_mpris_manager_integration_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_manager_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_manager_simple_SOURCES = test_mpris_manager_simple.cpp
test_mpris_manager_simple_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_manager_simple_LDADD = \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_manager_minimal_SOURCES = test_mpris_manager_minimal.cpp
test_mpris_manager_minimal_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_manager_minimal_LDADD = \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_manager_basic_integration_SOURCES = test_mpris_manager_basic_integration.cpp player_stub.cpp
test_mpris_manager_basic_integration_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_manager_basic_integration_LDADD = \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_error_handling_SOURCES = test_mpris_error_handling.cpp
test_mpris_error_handling_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_error_handling_LDADD = \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_error_integration_SOURCES = test_mpris_error_integration.cpp
test_mpris_error_integration_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_error_integration_LDADD = \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/io/libio.a \
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

# μ-law/A-law codec integration tests with SimplePCMCodec
test_mulaw_alaw_integration_SOURCES = test_mulaw_alaw_integration.cpp
test_mulaw_alaw_integration_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_mulaw_alaw_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# μ-law codec property-based tests (standalone, minimal dependencies)
test_mulaw_codec_properties_SOURCES = test_mulaw_codec_properties.cpp
test_mulaw_codec_properties_CPPFLAGS = $(AM_CPPFLAGS)
test_mulaw_codec_properties_LDADD = $(AM_LDFLAGS)

# A-law codec property-based tests (standalone, minimal dependencies)
test_alaw_codec_properties_SOURCES = test_alaw_codec_properties.cpp
test_alaw_codec_properties_CPPFLAGS = $(AM_CPPFLAGS)
test_alaw_codec_properties_LDADD = $(AM_LDFLAGS)

# μ-law/A-law parameter handling property-based tests (standalone, minimal dependencies)
test_mulaw_alaw_parameter_handling_SOURCES = test_mulaw_alaw_parameter_handling.cpp
test_mulaw_alaw_parameter_handling_CPPFLAGS = $(AM_CPPFLAGS)
test_mulaw_alaw_parameter_handling_LDADD = $(AM_LDFLAGS)

# μ-law/A-law error state consistency property-based tests (standalone, minimal dependencies)
test_mulaw_alaw_error_state_consistency_SOURCES = test_mulaw_alaw_error_state_consistency.cpp
test_mulaw_alaw_error_state_consistency_CPPFLAGS = $(AM_CPPFLAGS)
test_mulaw_alaw_error_state_consistency_LDADD = $(AM_LDFLAGS)

# μ-law/A-law codec selection unit tests (standalone, minimal dependencies)
test_mulaw_alaw_codec_selection_SOURCES = test_mulaw_alaw_codec_selection.cpp
test_mulaw_alaw_codec_selection_CPPFLAGS = $(AM_CPPFLAGS)
test_mulaw_alaw_codec_selection_LDADD = $(AM_LDFLAGS)

# μ-law/A-law error handling unit tests (standalone, minimal dependencies)
test_mulaw_alaw_error_handling_SOURCES = test_mulaw_alaw_error_handling.cpp
test_mulaw_alaw_error_handling_CPPFLAGS = $(AM_CPPFLAGS)
test_mulaw_alaw_error_handling_LDADD = $(AM_LDFLAGS)

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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# FLAC STREAMINFO real files test
test_streaminfo_real_files_SOURCES = test_streaminfo_real_files.cpp
test_streaminfo_real_files_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_streaminfo_real_files_LDADD = \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# FLAC STREAMINFO recovery test
test_streaminfo_recovery_SOURCES = test_streaminfo_recovery.cpp
test_streaminfo_recovery_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_streaminfo_recovery_LDADD = \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/io/libio.a \
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
$(top_builddir)/src/demuxer/flac/libflacdemuxer.a:
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

# FLAC thread safety validation test
test_flac_thread_safety_validation_SOURCES = test_flac_thread_safety_validation.cpp
test_flac_thread_safety_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_thread_safety_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(COMMON_TEST_LIBS) \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(OPENSSL_LIBS) \
	$(CURL_LIBS) \
	$(FLAC_LIBS) \
	$(LIBMPG123_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(OGG_LIBS) \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

# Note: FLACCodec.o and EnhancedAudioBufferPool.o build rules already defined above

# FLAC codec compatibility test
test_flac_codec_compatibility_SOURCES = test_flac_codec_compatibility.cpp
test_flac_codec_compatibility_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_codec_compatibility_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# FLAC RFC 9639 subframe type validation test
test_flac_rfc_subframe_validation_SOURCES = test_flac_rfc_subframe_validation.cpp
test_flac_rfc_subframe_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_rfc_subframe_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# FLAC RFC 9639 entropy coding validation test
test_flac_rfc_entropy_coding_validation_SOURCES = test_flac_rfc_entropy_coding_validation.cpp
test_flac_rfc_entropy_coding_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_rfc_entropy_coding_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# Simple entropy coding compilation test
test_entropy_coding_compilation_SOURCES = test_entropy_coding_compilation.cpp
test_entropy_coding_compilation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_entropy_coding_compilation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
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

# FLAC CRC validation test (RFC 9639 compliance)
test_flac_crc_validation_SOURCES = test_flac_crc_validation.cpp
test_flac_crc_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_crc_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# RFC 9639 bit depth and sample format compliance validation test
test_flac_rfc_bit_depth_validation_SOURCES = test_flac_rfc_bit_depth_validation.cpp
test_flac_rfc_bit_depth_validation_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_rfc_bit_depth_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lopus \
	$(AM_LDFLAGS)

# Simple RFC 9639 bit depth validation test (minimal dependencies)
test_flac_bit_depth_validation_simple_SOURCES = test_flac_bit_depth_validation_simple.cpp
test_flac_bit_depth_validation_simple_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_flac_bit_depth_validation_simple_LDADD = $(AM_LDFLAGS)

# RFC 9639 sample format and bit depth compliance test
test_flac_sample_format_rfc9639_compliance_SOURCES = test_flac_sample_format_rfc9639_compliance.cpp
test_flac_sample_format_rfc9639_compliance_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_flac_sample_format_rfc9639_compliance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/flac.o \
	$(top_builddir)/src/libmpg123.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/ModernStream.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/HTTPIOHandler.o \
	$(top_builddir)/src/HTTPClient.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/CodecRegistration.o \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/RawAudioDemuxer.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/ChunkDemuxer.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lopus -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

# OggDemuxer performance test
test_oggdemuxer_performance_SOURCES = test_oggdemuxer_performance.cpp
test_oggdemuxer_performance_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(OPUS_CFLAGS) $(FLAC_CFLAGS) -std=c++17 -pthread
test_oggdemuxer_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# Opus Codec Tests
test_opus_codec_core_decoding_SOURCES = test_opus_codec_core_decoding.cpp
test_opus_codec_core_decoding_CPPFLAGS = $(AM_CPPFLAGS) $(OPUS_CFLAGS)
test_opus_codec_core_decoding_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OpusCodec.o \
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
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(AM_LDFLAGS)

test_opus_codec_error_handling_SOURCES = test_opus_codec_error_handling.cpp
test_opus_codec_error_handling_CPPFLAGS = $(AM_CPPFLAGS) $(OPUS_CFLAGS) -pthread
test_opus_codec_error_handling_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OpusCodec.o \
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
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(AM_LDFLAGS)

test_opus_integration_SOURCES = test_opus_integration.cpp
test_opus_integration_CPPFLAGS = $(AM_CPPFLAGS) $(OPUS_CFLAGS)
test_opus_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(AM_LDFLAGS)

test_opus_compatibility_performance_SOURCES = test_opus_compatibility_performance.cpp
test_opus_compatibility_performance_CPPFLAGS = $(AM_CPPFLAGS) $(OPUS_CFLAGS)
test_opus_compatibility_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/demuxer/ogg/liboggdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/BoundedBuffer.o \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(AM_LDFLAGS)

# MPRIS Mock Framework Tests
if HAVE_DBUS
test_mpris_mock_framework_SOURCES = test_mpris_mock_framework.cpp
test_mpris_mock_framework_CPPFLAGS = $(AM_CPPFLAGS)
test_mpris_mock_framework_LDADD = libtest_utilities.a $(AM_LDFLAGS)

test_mpris_stress_testing_SOURCES = test_mpris_stress_testing.cpp
test_mpris_stress_testing_CPPFLAGS = $(AM_CPPFLAGS)
test_mpris_stress_testing_LDADD = libtest_utilities.a $(AM_LDFLAGS)

test_comprehensive_mock_framework_SOURCES = test_comprehensive_mock_framework.cpp
test_comprehensive_mock_framework_CPPFLAGS = $(AM_CPPFLAGS)
test_comprehensive_mock_framework_LDADD = libtest_utilities.a $(AM_LDFLAGS)

test_mock_framework_standalone_SOURCES = test_mock_framework_standalone.cpp
test_mock_framework_standalone_CPPFLAGS = $(AM_CPPFLAGS)
test_mock_framework_standalone_LDADD = libtest_utilities.a $(AM_LDFLAGS)

# Comprehensive MPRIS component tests
test_dbus_connection_manager_comprehensive_SOURCES = test_dbus_connection_manager_comprehensive.cpp
test_dbus_connection_manager_comprehensive_CPPFLAGS = $(AM_CPPFLAGS)
test_dbus_connection_manager_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_property_manager_comprehensive_SOURCES = test_property_manager_comprehensive.cpp
test_property_manager_comprehensive_CPPFLAGS = $(AM_CPPFLAGS)
test_property_manager_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_method_handler_comprehensive_SOURCES = test_method_handler_comprehensive.cpp
test_method_handler_comprehensive_CPPFLAGS = $(AM_CPPFLAGS) -DTESTING
test_method_handler_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_method_handler_simple_SOURCES = test_method_handler_simple.cpp
test_method_handler_simple_CPPFLAGS = $(AM_CPPFLAGS) -DTESTING
test_method_handler_simple_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_signal_emitter_comprehensive_SOURCES = test_signal_emitter_comprehensive.cpp
test_signal_emitter_comprehensive_CPPFLAGS = $(AM_CPPFLAGS)
test_signal_emitter_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_mpris_manager_comprehensive_SOURCES = test_mpris_manager_comprehensive.cpp
test_mpris_manager_comprehensive_CPPFLAGS = $(AM_CPPFLAGS)
test_mpris_manager_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_mpris_logger_basic_SOURCES = test_mpris_logger_basic.cpp
test_mpris_logger_basic_CPPFLAGS = $(AM_CPPFLAGS)
test_mpris_logger_basic_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_mpris_logger_dbus_tracing_SOURCES = test_mpris_logger_dbus_tracing.cpp
test_mpris_logger_dbus_tracing_CPPFLAGS = $(AM_CPPFLAGS)
test_mpris_logger_dbus_tracing_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)

test_mpris_logger_performance_SOURCES = test_mpris_logger_performance.cpp
test_mpris_logger_performance_CPPFLAGS = $(AM_CPPFLAGS)
test_mpris_logger_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/rect.o \
	$(AM_LDFLAGS)
endif

# MPRIS Integration Tests
test_mpris_spec_compliance_SOURCES = test_mpris_spec_compliance.cpp
test_mpris_spec_compliance_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_spec_compliance_LDADD = \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/rect.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_concurrent_clients_SOURCES = test_mpris_concurrent_clients.cpp
test_mpris_concurrent_clients_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_concurrent_clients_LDADD = \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/rect.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_reconnection_behavior_SOURCES = test_mpris_reconnection_behavior.cpp
test_mpris_reconnection_behavior_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_reconnection_behavior_LDADD = \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/rect.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_memory_validation_SOURCES = test_mpris_memory_validation.cpp
test_mpris_memory_validation_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS)
test_mpris_memory_validation_LDADD = \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/rect.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

# Test scripts for running comprehensive test suites
EXTRA_DIST = \
	run_mpris_mock_tests.sh \
	run_mpris_stress_tests.sh \
	validate_mock_framework.sh \
	run_mpris_logging_tests.sh \
	run_mpris_integration_tests.sh

# Test data files for mock framework validation
EXTRA_DIST += \
	mock_test_data.txt \
	stress_test_config.json

# MPRIS Final Validation Tests (Task 15)
if HAVE_DBUS
test_mpris_performance_profiler_SOURCES = test_mpris_performance_profiler.cpp
test_mpris_performance_profiler_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_mpris_performance_profiler_LDADD = \
	$(AM_LDFLAGS)

test_mpris_regression_validation_SOURCES = test_mpris_regression_validation.cpp fake_player.h
test_mpris_regression_validation_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS) -std=c++17 -pthread
test_mpris_regression_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/MPRISManager.o \
	$(top_builddir)/src/PropertyManager.o \
	$(top_builddir)/src/MethodHandler.o \
	$(top_builddir)/src/SignalEmitter.o \
	$(top_builddir)/src/DBusConnectionManager.o \
	$(top_builddir)/src/MPRISTypes.o \
	$(top_builddir)/src/MPRISLogger.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)
endif

# Additional test scripts and headers to be distributed
EXTRA_DIST += \
	run_mpris_static_analysis.sh \
	run_mpris_final_validation.sh \
	run_mpris_integration_tests.sh \
	run_basic_integration_tests.sh \
	run_mpris_logging_tests.sh \
	fake_player.h

# Make test scripts executable during build
all-local:
	chmod +x $(srcdir)/run_mpris_static_analysis.sh
	chmod +x $(srcdir)/run_mpris_final_validation.sh
	chmod +x $(srcdir)/run_mpris_integration_tests.sh
	chmod +x $(srcdir)/run_basic_integration_tests.sh
	chmod +x $(srcdir)/run_mpris_logging_tests.sh

# Clean up generated validation reports
clean-local:
	rm -f mpris_performance_profile.csv
	rm -f mpris_performance_report.txt
	rm -f mpris_regression_report.txt
	rm -f static_analysis_report.txt
	rm -f final_validation_report.html
	rm -rf /tmp/mpris_*
	rm -rf validation_results_*

# ISO Demuxer Compliance Validation Tests
check_PROGRAMS += test_iso_compliance_comprehensive test_iso_compliance_edge_cases test_iso_codec_compliance test_iso_compliance_minimal test_iso_compliance_validation

# ISO Demuxer Comprehensive Integration Tests (Task 21)
check_PROGRAMS += test_iso_comprehensive_integration_simple test_iso_encoder_compatibility test_iso_fragmented_streaming test_iso_seeking_accuracy test_iso_comprehensive_integration

# ISO Demuxer FLAC Integration Tests
check_PROGRAMS += test_iso_flac_integration test_iso_flac_support

# ISO Demuxer Performance and Optimization Tests
check_PROGRAMS += test_iso_performance_optimization test_iso_sample_table_optimization test_iso_sample_table_simple

# ISO Demuxer Additional Tests
check_PROGRAMS += test_iso_constants test_iso_metadata_extraction test_iso_telephony_codecs

test_iso_compliance_comprehensive_SOURCES = test_iso_compliance_comprehensive.cpp
test_iso_compliance_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(AM_LDFLAGS)

test_iso_compliance_edge_cases_SOURCES = test_iso_compliance_edge_cases.cpp
test_iso_compliance_edge_cases_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(AM_LDFLAGS)

test_iso_codec_compliance_SOURCES = test_iso_codec_compliance.cpp
test_iso_codec_compliance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(AM_LDFLAGS)

# ISO Demuxer Comprehensive Integration Tests (Task 21)
test_iso_comprehensive_integration_simple_SOURCES = test_iso_comprehensive_integration_simple.cpp
test_iso_comprehensive_integration_simple_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_comprehensive_integration_simple_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_encoder_compatibility_SOURCES = test_iso_encoder_compatibility.cpp
test_iso_encoder_compatibility_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_encoder_compatibility_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_fragmented_streaming_SOURCES = test_iso_fragmented_streaming.cpp
test_iso_fragmented_streaming_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_fragmented_streaming_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_seeking_accuracy_SOURCES = test_iso_seeking_accuracy.cpp
test_iso_seeking_accuracy_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_seeking_accuracy_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
# Additional ISO Demuxer Compliance Tests
test_iso_compliance_minimal_SOURCES = test_iso_compliance_minimal.cpp
test_iso_compliance_minimal_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_compliance_minimal_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_compliance_validation_SOURCES = test_iso_compliance_validation.cpp
test_iso_compliance_validation_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_compliance_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# Additional ISO Demuxer Integration Tests
test_iso_comprehensive_integration_SOURCES = test_iso_comprehensive_integration.cpp
test_iso_comprehensive_integration_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_comprehensive_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# ISO Demuxer FLAC Integration Tests
test_iso_flac_integration_SOURCES = test_iso_flac_integration.cpp
test_iso_flac_integration_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_flac_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_flac_support_SOURCES = test_iso_flac_support.cpp
test_iso_flac_support_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_flac_support_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# ISO Demuxer Performance and Optimization Tests
test_iso_performance_optimization_SOURCES = test_iso_performance_optimization.cpp
test_iso_performance_optimization_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_performance_optimization_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_sample_table_optimization_SOURCES = test_iso_sample_table_optimization.cpp
test_iso_sample_table_optimization_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_sample_table_optimization_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_sample_table_simple_SOURCES = test_iso_sample_table_simple.cpp
test_iso_sample_table_simple_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_sample_table_simple_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# ISO Demuxer Additional Tests
test_iso_constants_SOURCES = test_iso_constants.cpp
test_iso_constants_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_constants_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_iso_metadata_extraction_SOURCES = test_iso_metadata_extraction.cpp
test_iso_metadata_extraction_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_metadata_extraction_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

test_iso_telephony_codecs_SOURCES = test_iso_telephony_codecs.cpp
test_iso_telephony_codecs_CPPFLAGS = $(AM_CPPFLAGS)
test_iso_telephony_codecs_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/ISODemuxer.o \
	$(top_builddir)/src/ISODemuxerBoxParser.o \
	$(top_builddir)/src/ISODemuxerSampleTableManager.o \
	$(top_builddir)/src/ISODemuxerFragmentHandler.o \
	$(top_builddir)/src/ISODemuxerMetadataExtractor.o \
	$(top_builddir)/src/ISODemuxerStreamManager.o \
	$(top_builddir)/src/ISODemuxerSeekingEngine.o \
	$(top_builddir)/src/ISODemuxerErrorRecovery.o \
	$(top_builddir)/src/ISODemuxerComplianceValidator.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# RFC 9639 compliance validator test
if HAVE_FLAC
test_flac_rfc9639_compliance_validator_SOURCES = test_flac_rfc9639_compliance_validator.cpp FLACRFCComplianceValidator.cpp
test_flac_rfc9639_compliance_validator_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS)
test_flac_rfc9639_compliance_validator_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACRFCValidator.o \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/libmpg123.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/HTTPIOHandler.o \
	$(top_builddir)/src/HTTPClient.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/ModernStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/io/libio.a \
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
	-logg -lvorbis -lvorbisenc -lopus -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

# RFC Compliance Debugging and Validation Tools Test
# Temporarily disabled due to missing class implementations
# noinst_PROGRAMS += test_rfc_compliance_debugging_tools
test_rfc_compliance_debugging_tools_SOURCES = test_rfc_compliance_debugging_tools.cpp FLACRFCComplianceValidator.cpp
test_rfc_compliance_debugging_tools_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/include
test_rfc_compliance_debugging_tools_LDADD = \
        libtest_utilities.a \
        $(top_builddir)/src/FLACRFCValidator.o \
        $(top_builddir)/src/debug.o \
        $(top_builddir)/src/exceptions.o \
        $(FLAC_LIBS) \
        $(AM_LDFLAGS)

# FLAC audio output test (verifies synthetic STREAMINFO fix)
test_flac_audio_output_SOURCES = test_flac_audio_output.cpp
test_flac_audio_output_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17
test_flac_audio_output_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/FLACRFCValidator.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/libmpg123.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lmpg123 -lopus \
	$(AM_LDFLAGS)

# FLAC seeking crash test (reproduces memory corruption during seeking)
test_flac_seeking_crash_SOURCES = test_flac_seeking_crash.cpp
test_flac_seeking_crash_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17
test_flac_seeking_crash_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/FLACRFCValidator.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/PCMCodecs.o \
	$(top_builddir)/src/ALawCodec.o \
	$(top_builddir)/src/MuLawCodec.o \
	$(top_builddir)/src/OggCodecs.o \
	$(top_builddir)/src/vorbis.o \
	$(top_builddir)/src/opus.o \
	$(top_builddir)/src/OpusCodec.o \
	$(top_builddir)/src/libmpg123.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/lyrics.o \
	$(top_builddir)/src/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lmpg123 -lopus \
	$(AM_LDFLAGS)

# FLAC audio output debug test (comprehensive pipeline debugging)
test_flac_audio_output_debug_SOURCES = test_flac_audio_output_debug.cpp
test_flac_audio_output_debug_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17
test_flac_audio_output_debug_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/FLACRFCValidator.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# FLAC codec simple debug test (minimal codec testing)
test_flac_codec_simple_debug_SOURCES = test_flac_codec_simple_debug.cpp
test_flac_codec_simple_debug_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17
test_flac_codec_simple_debug_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/FLACCodec.o \
	$(top_builddir)/src/FLACRFCValidator.o \
	$(top_builddir)/src/AudioCodec.o \
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

# FLAC streamable subset real files test
test_flac_streamable_subset_real_files_SOURCES = test_flac_streamable_subset_real_files.cpp
test_flac_streamable_subset_real_files_CPPFLAGS = $(AM_CPPFLAGS) $(FLAC_CFLAGS) -std=c++17
test_flac_streamable_subset_real_files_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/io/libio.a \
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

# Debug system test
noinst_PROGRAMS += test_debug_system
test_debug_system_SOURCES = test_debug_system.cpp
test_debug_system_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_debug_system_LDADD = \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Native FLAC codec unit tests (conditional on HAVE_NATIVE_FLAC)
if HAVE_NATIVE_FLAC
check_PROGRAMS += test_bitstream_reader_unit test_frame_parser_unit test_subframe_decoder_unit test_residual_decoder_unit test_channel_decorrelator_unit test_sample_reconstructor_unit test_native_flac_rfc_examples test_native_flac_real_files test_native_flac_containers test_native_flac_edge_cases test_native_flac_performance_benchmark test_native_flac_memory_usage test_native_flac_threading

# BitstreamReader unit tests
test_bitstream_reader_unit_SOURCES = test_bitstream_reader_unit.cpp
test_bitstream_reader_unit_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_bitstream_reader_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# FrameParser unit tests
test_frame_parser_unit_SOURCES = test_frame_parser_unit.cpp
test_frame_parser_unit_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_frame_parser_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# SubframeDecoder unit tests
test_subframe_decoder_unit_SOURCES = test_subframe_decoder_unit.cpp
test_subframe_decoder_unit_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_subframe_decoder_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# ResidualDecoder unit tests
test_residual_decoder_unit_SOURCES = test_residual_decoder_unit.cpp
test_residual_decoder_unit_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_residual_decoder_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# ChannelDecorrelator unit tests
test_channel_decorrelator_unit_SOURCES = test_channel_decorrelator_unit.cpp
test_channel_decorrelator_unit_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_channel_decorrelator_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# SampleReconstructor unit tests
test_sample_reconstructor_unit_SOURCES = test_sample_reconstructor_unit.cpp
test_sample_reconstructor_unit_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_sample_reconstructor_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

# Integration test: RFC 9639 example files
test_native_flac_rfc_examples_SOURCES = test_native_flac_rfc_examples.cpp
test_native_flac_rfc_examples_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_native_flac_rfc_examples_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/demuxers/flac/libflacdemuxer.a \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/CodecRegistration.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(AM_LDFLAGS)

# Integration test: Real FLAC files
test_native_flac_real_files_SOURCES = test_native_flac_real_files.cpp
test_native_flac_real_files_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_native_flac_real_files_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/demuxers/flac/libflacdemuxer.a \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/CodecRegistration.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(AM_LDFLAGS)

# Integration test: Container formats
test_native_flac_containers_SOURCES = test_native_flac_containers.cpp
test_native_flac_containers_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_native_flac_containers_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/demuxers/flac/libflacdemuxer.a \
	$(top_builddir)/src/demuxers/ogg/liboggdemuxer.a \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/CodecRegistration.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(AM_LDFLAGS) \
	-logg

# Integration test: Edge cases
test_native_flac_edge_cases_SOURCES = test_native_flac_edge_cases.cpp
test_native_flac_edge_cases_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_native_flac_edge_cases_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/demuxers/flac/libflacdemuxer.a \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/DemuxerRegistry.o \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/CodecRegistration.o \
	$(top_builddir)/src/demuxer/libdemuxer.a \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(AM_LDFLAGS)

# Performance test: Benchmark against libFLAC
test_native_flac_performance_benchmark_SOURCES = test_native_flac_performance_benchmark.cpp
test_native_flac_performance_benchmark_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_native_flac_performance_benchmark_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/codecs/vorbis/libvorbiscodec.a \
	$(top_builddir)/src/codecs/opus/libopuscodec.a \
	$(top_builddir)/src/codecs/mp3/libmp3codec.a \
	$(top_builddir)/src/codecs/pcm/libpcmcodecs.a \
	$(top_builddir)/src/demuxers/ogg/liboggdemuxer.a \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(AM_LDFLAGS)

# Performance test: Memory usage
test_native_flac_memory_usage_SOURCES = test_native_flac_memory_usage.cpp
test_native_flac_memory_usage_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
test_native_flac_memory_usage_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/codecs/vorbis/libvorbiscodec.a \
	$(top_builddir)/src/codecs/opus/libopuscodec.a \
	$(top_builddir)/src/codecs/mp3/libmp3codec.a \
	$(top_builddir)/src/codecs/pcm/libpcmcodecs.a \
	$(top_builddir)/src/demuxers/ogg/liboggdemuxer.a \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(AM_LDFLAGS)

# Performance test: Threading
test_native_flac_threading_SOURCES = test_native_flac_threading.cpp
test_native_flac_threading_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17 -pthread
test_native_flac_threading_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libnativeflac.a \
	$(top_builddir)/src/codecs/vorbis/libvorbiscodec.a \
	$(top_builddir)/src/codecs/opus/libopuscodec.a \
	$(top_builddir)/src/codecs/mp3/libmp3codec.a \
	$(top_builddir)/src/codecs/pcm/libpcmcodecs.a \
	$(top_builddir)/src/demuxers/ogg/liboggdemuxer.a \
	$(top_builddir)/src/CodecRegistry.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/io/file/libfileio.a \
	$(top_builddir)/src/RAIIFileHandle.o \
	$(top_builddir)/src/io/libio.a \
	$(top_builddir)/src/rect.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/MemoryPoolManager.o \
	$(top_builddir)/src/MemoryTracker.o \
	$(top_builddir)/src/MemoryOptimizer.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/EnhancedBufferPool.o \
	$(top_builddir)/src/EnhancedAudioBufferPool.o \
	$(AM_LDFLAGS)

endif

endif
