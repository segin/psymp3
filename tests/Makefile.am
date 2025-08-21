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

# Test harness executable
if BUILD_TEST_HARNESS
noinst_PROGRAMS = test-harness
test_harness_SOURCES = test_harness.cpp test_discovery.cpp test_executor.cpp test_reporter.cpp
test_harness_CPPFLAGS = $(AM_CPPFLAGS)
test_harness_LDADD = libtest_utilities.a $(top_builddir)/src/rect.o $(AM_LDFLAGS)
endif

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
	test_demuxer_performance

# OggDemuxer tests (conditional on HAVE_OGGDEMUXER)
if HAVE_OGGDEMUXER
check_PROGRAMS += test_page_extraction
endif

# A-law codec tests
check_PROGRAMS += test_alaw_table_verification test_alaw_sample_conversion test_alaw_conversion_accuracy

# μ-law codec tests
check_PROGRAMS += test_mulaw_conversion_accuracy

# Codec selection and validation tests
check_PROGRAMS += test_codec_selection_validation_simple

# Performance and thread safety tests
check_PROGRAMS += test_codec_performance test_codec_thread_safety test_codec_concurrent_instances test_codec_performance_simple test_codec_thread_safety_simple

# SimplePCMCodec integration tests
check_PROGRAMS += test_simplepcmcodec_integration_minimal test_simplepcmcodec_real_integration test_simplepcmcodec_comprehensive_integration

# Conditional compilation tests
check_PROGRAMS += test_conditional_codec_compilation

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
test_iso_fragment_handler_LDADD = $(top_builddir)/src/ISODemuxerFragmentHandler.o $(top_builddir)/src/ISODemuxer.o $(AM_LDFLAGS)

test_iso_fragmented_mp4_SOURCES = test_iso_fragmented_mp4.cpp
test_iso_fragmented_mp4_LDADD = $(top_builddir)/src/ISODemuxerFragmentHandler.o $(top_builddir)/src/ISODemuxer.o $(AM_LDFLAGS)

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
	$(top_builddir)/src/FileIOHandler.o \
	$(top_builddir)/src/URI.o \
	$(top_builddir)/src/exceptions.o \
	$(top_builddir)/src/RAIIFileHandle.o \
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
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_demuxer_factory_unit_SOURCES = test_demuxer_factory_unit.cpp
test_demuxer_factory_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/DemuxerFactory.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_media_factory_unit_SOURCES = test_media_factory_unit.cpp
test_media_factory_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/MediaFactory.o \
	$(top_builddir)/src/Stream.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
	$(AM_LDFLAGS)

test_demuxed_stream_unit_SOURCES = test_demuxed_stream_unit.cpp
test_demuxed_stream_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/IOHandler.o \
	$(top_builddir)/src/Demuxer.o \
	$(top_builddir)/src/DemuxedStream.o \
	$(top_builddir)/src/AudioCodec.o \
	$(top_builddir)/src/Stream.o \
	$(top_builddir)/src/BufferPool.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/exceptions.o \
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
	$(top_builddir)/src/Stream.o \
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

# OggDemuxer page extraction test
if HAVE_OGGDEMUXER
test_page_extraction_SOURCES = test_page_extraction_simple.cpp
test_page_extraction_CPPFLAGS = $(AM_CPPFLAGS) $(OGG_CFLAGS)
test_page_extraction_LDADD = \
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
	$(OGG_LIBS) \
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

# Simple codec performance test (minimal dependencies)
test_codec_performance_simple_SOURCES = test_codec_performance_simple.cpp
test_codec_performance_simple_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_performance_simple_LDADD = $(AM_LDFLAGS)

# Simple codec thread safety test (minimal dependencies)
test_codec_thread_safety_simple_SOURCES = test_codec_thread_safety_simple.cpp
test_codec_thread_safety_simple_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_MULAW_CODEC -DENABLE_ALAW_CODEC
test_codec_thread_safety_simple_LDADD = $(AM_LDFLAGS)

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