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
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(OPUS_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(FLAC_CFLAGS) $(LIBMPG123_CFLAGS)

if HAVE_DBUS
AM_CPPFLAGS += $(DBUS_CFLAGS)
endif

# Common flags for all test programs

# Define optional codec/demuxer libraries (same as main binary)
CODEC_LIBS =
DEMUXER_LIBS =


if HAVE_FLAC
DEMUXER_LIBS += $(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a
if HAVE_NATIVE_FLAC
CODEC_LIBS += $(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a
else
CODEC_LIBS += $(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a
endif
endif

if HAVE_MP3
CODEC_LIBS += $(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a
endif

if HAVE_VORBIS
CODEC_LIBS += $(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a
endif

if HAVE_OPUS
CODEC_LIBS += $(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a
endif

if HAVE_OGGDEMUXER
DEMUXER_LIBS += $(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a
endif

# Common library set for tests (mirrors main binary linking)
# Order: high-level -> mid-level -> low-level -> system libs
# Note: Libraries that USE symbols must come BEFORE libraries that DEFINE them
# libpsymp3-codecs.a uses symbols from libpsymp3-codec-pcm.a and CODEC_LIBS, so it comes first
# We list libraries multiple times to handle circular dependencies
COMMON_TEST_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/widget/ui/libpsymp3-widget-ui.a \
	$(top_builddir)/src/widget/windowing/libpsymp3-widget-windowing.a \
	$(top_builddir)/src/widget/foundation/libpsymp3-widget-foundation.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/lastfm/libpsymp3-lastfm.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a

# Minimal library set for simple tests (I/O, demuxer core, codecs)
# Note: Libraries with circular dependencies must be listed multiple times
# Order: high-level -> mid-level -> low-level, then repeat for circular deps
# Note: Tag library is required because demuxers now depend on it for metadata extraction
MINIMAL_TEST_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a

# Common object files needed by tests (not in libraries)
# These are standalone .o files from src/ that tests commonly need
# Note: Memory management files (MemoryPoolManager, MemoryTracker, BufferPool, etc.)
# are now part of libpsymp3-io.a and should be linked via $(top_builddir)/src/io/libpsymp3-io.a
# Note: These object files depend on I/O libraries, so they must be listed
# BEFORE the libraries they depend on in the link order
COMMON_TEST_OBJS = \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a

# Extended object files that depend on I/O libraries
# These must be linked with I/O libraries following them
EXTENDED_TEST_OBJS = \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a

FULL_CODEC_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a

# Legacy variable for backward compatibility (deprecated - use FULL_CODEC_LIBS)
COMMON_CODEC_DEPS = $(FULL_CODEC_LIBS)

# MPRIS testing libraries
MPRIS_TEST_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS)

# Common library links for codec tests
COMMON_CODEC_LIBS = -lFLAC++ -lFLAC -lmpg123 -lvorbis -lopus -logg
AM_LDFLAGS = $(SDL_LIBS) $(TAGLIB_LIBS) $(FREETYPE_LIBS) $(DBUS_LIBS) $(OPUS_LIBS) $(OPENSSL_LIBS) $(CURL_LIBS) $(WINSOCK_LIBS)

# Common test utilities library
noinst_LIBRARIES = libtest_utilities.a
libtest_utilities_a_SOURCES = test_framework.cpp test_rect_utilities.cpp test_framework_threading.cpp
if HAVE_DBUS
libtest_utilities_a_SOURCES += mock_dbus_connection.cpp mock_player.cpp mpris_test_fixtures.cpp
endif

# Test harness executable and debug programs
if BUILD_TEST_HARNESS
noinst_PROGRAMS = test-harness debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_seek_deadlock test_ogg_codec_detection test_error_handling test_mulaw_codec_properties test_alaw_codec_properties test_mulaw_alaw_error_state_consistency test_lastfm_url_encoding_properties test_lastfm_exponential_backoff_properties test_lastfm_lazy_logging_properties test_lastfm_scrobble_cache_roundtrip_properties test_lastfm_config_backward_compatibility test_lastfm_config_roundtrip_properties test_lastfm_thread_safety_properties test_flac_stream_marker_properties test_flac_metadata_block_header_properties test_flac_streaminfo_block_size_properties test_flac_seektable_placeholder_properties test_flac_vorbis_comment_endianness_properties test_flac_cuesheet_track_count_properties test_flac_frame_sync_properties test_flac_block_size_bits_properties test_flac_sample_rate_bits_properties test_flac_crc8_properties test_flac_crc16_properties test_flac_frame_size_estimation_properties test_flac_frame_boundary_search_properties test_flac_duration_calculation_properties test_flac_sync_resync_properties test_flac_thread_safety_lock_pattern_properties test_flac_debug_logging_format_properties
test_harness_SOURCES = test_harness.cpp test_discovery.cpp test_executor.cpp test_reporter.cpp
test_harness_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)
else
noinst_PROGRAMS = debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_seek_deadlock test_flac_frame_indexing test_frame_size_estimation_consistency test_ogg_codec_detection test_error_handling test_flac_streamable_subset_real_files test_mulaw_codec_properties test_alaw_codec_properties test_mulaw_alaw_error_state_consistency test_lastfm_url_encoding_properties test_lastfm_exponential_backoff_properties test_lastfm_lazy_logging_properties test_lastfm_scrobble_cache_roundtrip_properties test_lastfm_config_backward_compatibility test_lastfm_config_roundtrip_properties test_lastfm_thread_safety_properties test_flac_stream_marker_properties test_flac_metadata_block_header_properties test_flac_streaminfo_block_size_properties test_flac_seektable_placeholder_properties test_flac_vorbis_comment_endianness_properties test_flac_cuesheet_track_count_properties test_flac_frame_sync_properties test_flac_block_size_bits_properties test_flac_crc8_properties test_flac_crc16_properties test_flac_frame_size_estimation_properties test_flac_frame_boundary_search_properties test_flac_duration_calculation_properties test_flac_sync_resync_properties test_flac_thread_safety_lock_pattern_properties test_flac_debug_logging_format_properties
endif

# ============================================================================
# opusout - Opus to PCM/WAV decoder utility
# ============================================================================
#
# Usage:
#   opusout [options] input.opus [output.wav|output.pcm]
#
# Options:
#   -r, --raw       Output raw PCM instead of WAVE (default: WAVE)
#   -q, --quiet     Suppress progress output
#   -v, --verbose   Show detailed decoding information
#   -h, --help      Show this help message
#
# ============================================================================

if HAVE_OPUS
noinst_PROGRAMS += opusout

opusout_SOURCES = opusout.cpp
opusout_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OPUS_LIBS) $(OGG_LIBS) \
	-lvorbis -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(FLAC_LIBS) $(TAGLIB_LIBS)
endif

debug_dual_flac_SOURCES = debug_dual_flac.cpp
debug_dual_flac_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

debug_bit_extraction_SOURCES = debug_bit_extraction.cpp
debug_bit_extraction_LDADD = $(AM_LDFLAGS)

debug_flac_seeking_SOURCES = debug_flac_seeking.cpp
debug_flac_seeking_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_iohandler_seek_comprehensive_SOURCES = test_iohandler_seek_comprehensive.cpp
test_iohandler_seek_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_iohandler_buffer_analysis_SOURCES = test_iohandler_buffer_analysis.cpp
test_iohandler_buffer_analysis_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_debug_iohandler_position_SOURCES = test_debug_iohandler_position.cpp
test_debug_iohandler_position_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_seek_deadlock_SOURCES = test_seek_deadlock.cpp
test_seek_deadlock_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_flac_frame_indexing_SOURCES = test_flac_frame_indexing.cpp
test_flac_frame_indexing_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Individual test executables
check_PROGRAMS = test_rect_area_validation test_rect_containment test_rect_intersection test_rect_union opusout \
	test_rect_centering \
	test_rect_centering_overflow \
	test_rect_expansion \
	test_rect_transformation \
	test_rect_normalization \
	test_rect_modern_cpp \
	test_rect_backward_compatibility \
	test_rect_performance \
	test_rect_unit \
	test_rect_integration \
	test_rect_pair_variants \
	test_widget_rect_integration \
	test_harness_integration \
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
	test_iohandler_performance_validation \
	test_http_client_parse_url

# OggDemuxer tests (conditional on HAVE_OGGDEMUXER)
if HAVE_OGGDEMUXER
check_PROGRAMS += test_ogg_page_extraction test_ogg_duration_calculation test_ogg_time_conversion \
	test_ogg_data_streaming test_ogg_memory_safety test_ogg_demuxer_integration \
	test_oggdemuxer_performance test_oggdemuxer_comprehensive test_ogg_comprehensive_integration \
	test_ogg_fuzzing test_ogg_multiplexed_seeking 	test_ogg_seeking_stress \
	test_compression_lz77 repro_ogg_duration_perf

test_compression_lz77_SOURCES = test_compression_lz77.cpp
test_compression_lz77_LDADD = \
	$(top_builddir)/src/core/compression/libpsymp3-core-compression.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(LDADD)

repro_ogg_duration_perf_SOURCES = repro_ogg_duration_perf.cpp
repro_ogg_duration_perf_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
endif

# RapidCheck property-based testing setup (conditional on HAVE_RAPIDCHECK)
if HAVE_RAPIDCHECK
check_PROGRAMS += test_boxparser_vulnerability \
    test_rapidcheck_setup test_opus_codec_property test_fixed_predictor_properties \
    test_rect_properties

test_rapidcheck_setup_SOURCES = test_rapidcheck_setup.cpp
test_rapidcheck_setup_LDADD = $(COMMON_TEST_LIBS) $(RAPIDCHECK_LIBS)

test_boxparser_vulnerability_SOURCES = test_boxparser_vulnerability.cpp
test_boxparser_vulnerability_LDADD = $(COMMON_TEST_LIBS) \
 $(RAPIDCHECK_LIBS)

test_opus_codec_property_SOURCES = test_opus_codec_property.cpp
test_opus_codec_property_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(OPUS_LIBS) \
	$(RAPIDCHECK_LIBS) \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(TAGLIB_LIBS) \
	$(VORBIS_LIBS) \
	$(FLAC_LIBS) \
	$(FLAC_LIBS) \
	-lmpg123 \
	$(AM_LDFLAGS)

test_fixed_predictor_properties_SOURCES = test_fixed_predictor_properties.cpp
test_fixed_predictor_properties_LDADD = \
	$(RAPIDCHECK_LIBS) \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

test_rect_properties_SOURCES = test_rect_properties.cpp
test_rect_properties_LDADD = \
	libtest_utilities.a \
	$(RAPIDCHECK_LIBS) \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)
endif


# Opus codec tests (conditional on HAVE_OPUS)
if HAVE_OPUS
check_PROGRAMS += test_opus_codec_core_decoding test_opus_codec_error_handling \
	test_opus_integration test_opus_compatibility_performance
endif

# G711 codec tests (both A-law and μ-law)
if HAVE_G711
  # A-law tests
  check_PROGRAMS += test_alaw_table_verification test_alaw_sample_conversion test_alaw_conversion_accuracy test_alaw_codec_properties test_wav_g711 test_wav_g711_improved
  # μ-law tests
  check_PROGRAMS += test_mulaw_conversion_accuracy
  # Combined codec selection tests
  check_PROGRAMS += test_mulaw_alaw_codec_selection test_mulaw_alaw_error_handling
  # Performance tests for G711
  check_PROGRAMS += test_mulaw_alaw_performance
  # Integration tests for G711
  check_PROGRAMS += test_mulaw_alaw_integration
endif

# Codec selection and validation tests
check_PROGRAMS += test_codec_selection_validation_simple

# Performance and thread safety tests
check_PROGRAMS += test_codec_performance test_codec_thread_safety test_codec_concurrent_instances test_codec_performance_simple test_codec_thread_safety_simple test_threading_safety_baseline test_audio_thread_safety test_audio_threading_pattern test_iohandler_thread_safety_comprehensive test_iohandler_memory_deadlock_prevention test_memory_pool_manager_integration test_memory_pool_manager_thread_safety_comprehensive test_memory_pool_manager_basic_threading test_memory_pool_allocation_failure test_surface_thread_safety test_surface_performance_regression test_system_wide_threading_integration test_threading_performance_regression

# SimplePCMCodec integration tests
check_PROGRAMS += test_simplepcmcodec_integration_minimal test_simplepcmcodec_real_integration test_simplepcmcodec_comprehensive_integration

# Conditional compilation tests
check_PROGRAMS += test_conditional_codec_compilation

# MPRIS tests (conditional on HAVE_DBUS)
if HAVE_DBUS
check_PROGRAMS += test_mpris_types test_dbus_connection_manager test_property_manager test_method_handler test_signal_emitter test_mpris_manager_integration test_mpris_manager_simple test_mpris_manager_minimal test_mpris_manager_basic_integration test_mpris_error_handling test_mpris_error_integration test_mpris_mock_framework test_mpris_stress_testing test_comprehensive_mock_framework test_mock_framework_standalone test_dbus_connection_manager_comprehensive test_property_manager_comprehensive test_method_handler_comprehensive test_method_handler_simple test_signal_emitter_comprehensive test_mpris_manager_comprehensive test_mpris_logger_basic test_mpris_logger_dbus_tracing test_mpris_logger_performance test_mpris_spec_compliance test_mpris_concurrent_clients test_mpris_reconnection_behavior test_mpris_memory_validation test_mpris_performance_profiler test_mpris_regression_validation
endif

# FLAC demuxer compatibility tests (conditional on HAVE_FLAC)
if HAVE_FLAC
check_PROGRAMS += test_flac_demuxer_simple test_flac_real_file test_streaminfo_real_files \
	test_streaminfo_recovery test_streaminfo_debug test_flac_diagnostic test_iohandler_deadlock \
	test_flac_demuxer_compatibility test_flac_demuxer_performance test_flac_compatibility_integration \
	test_flac_demuxer_thread_safety test_flac_backward_compatibility_validation \
	test_flac_demuxer_real_files test_flac_bisection_real_files test_flac_mini_player \
	test_flac_demuxer_unit_comprehensive test_flac_demuxer_integration_comprehensive \
	test_flac_demuxer_unit_fixed test_flac_index_based_seeking test_flac_variable_block_size \
	test_flac_codec_minimal_container_agnostic test_flac_codec_container_agnostic \
	test_flac_quality_simple test_flac_thread_safety_validation test_flac_codec_unit_minimal \
	test_flac_codec_compatibility test_flac_codec_integration test_flac_codec_unit_comprehensive \
	test_flac_codec_error_handling test_flac_codec_performance_comprehensive \
	test_flac_codec_multithreaded test_flac_codec_demuxer_integration \
	test_flac_performance_benchmarking test_flac_conditional_compilation \
	test_flac_binary_search_limitations test_flac_demuxer_threading \
	test_flac_performance_optimization test_flac_performance_with_real_files \
	test_flac_comprehensive_validation test_flac_rfc_compliance test_flac_rfc_subframe_validation \
	test_flac_rfc_entropy_coding_validation test_entropy_coding_compilation \
	test_flac_test_data_validation test_flac_crc_validation test_flac_rfc_bit_depth_validation \
	test_flac_bit_depth_validation_simple test_flac_security_validation \
	test_flac_sample_format_rfc9639_compliance test_flac_audio_output test_flac_seeking_crash \
	test_flac_audio_output_debug test_flac_codec_simple_debug
endif

# Test program sources and dependencies
test_rect_area_validation_SOURCES = test_rect_area_validation.cpp
test_rect_area_validation_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_containment_SOURCES = test_rect_containment.cpp
test_rect_containment_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_intersection_SOURCES = test_rect_intersection.cpp
test_rect_intersection_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_union_SOURCES = test_rect_union.cpp
test_rect_union_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_centering_SOURCES = test_rect_centering.cpp
test_rect_centering_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_centering_overflow_SOURCES = test_rect_centering_overflow.cpp
test_rect_centering_overflow_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_expansion_SOURCES = test_rect_expansion.cpp
test_rect_expansion_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_transformation_SOURCES = test_rect_transformation.cpp
test_rect_transformation_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_normalization_SOURCES = test_rect_normalization.cpp
test_rect_normalization_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_modern_cpp_SOURCES = test_rect_modern_cpp.cpp
test_rect_modern_cpp_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_backward_compatibility_SOURCES = test_rect_backward_compatibility.cpp
test_rect_backward_compatibility_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_performance_SOURCES = test_rect_performance.cpp
test_rect_performance_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_unit_SOURCES = test_rect_unit.cpp
test_rect_unit_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_integration_SOURCES = test_rect_integration.cpp
test_rect_integration_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_rect_pair_variants_SOURCES = test_rect_pair_variants.cpp
test_rect_pair_variants_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_widget_rect_integration_SOURCES = test_widget_rect_integration.cpp
test_widget_rect_integration_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_harness_integration_SOURCES = test_harness_integration.cpp
test_harness_integration_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_iohandler_integration_simple_SOURCES = test_iohandler_integration_simple.cpp
test_iohandler_integration_simple_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_iohandler_minimal_SOURCES = test_iohandler_minimal.cpp
test_iohandler_minimal_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_iohandler_basic_SOURCES = test_iohandler_basic.cpp
test_iohandler_basic_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_integration_verification_SOURCES = test_integration_verification.cpp
test_integration_verification_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_iohandler_demuxer_integration_SOURCES = test_iohandler_demuxer_integration.cpp
test_iohandler_demuxer_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_iohandler_demuxer_integration_simple_SOURCES = test_iohandler_demuxer_integration_simple.cpp
test_iohandler_demuxer_integration_simple_LDADD = libtest_utilities.a $(AM_LDFLAGS)

test_psymp3_system_integration_SOURCES = test_psymp3_system_integration.cpp
test_psymp3_system_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(AM_LDFLAGS)

test_api_consistency_SOURCES = test_api_consistency.cpp
test_api_consistency_LDADD = libtest_utilities.a $(AM_LDFLAGS)

# Demuxer architecture unit tests
test_demuxer_unit_SOURCES = test_demuxer_unit.cpp
test_demuxer_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_demuxer_factory_unit_SOURCES = test_demuxer_factory_unit.cpp
test_demuxer_factory_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_media_factory_unit_SOURCES = test_media_factory_unit.cpp
test_media_factory_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lmpg123 -lopus -lcurl -lssl -lcrypto \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(AM_LDFLAGS)

test_demuxed_stream_unit_SOURCES = test_demuxed_stream_unit.cpp
test_demuxed_stream_unit_LDADD = libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lopus -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

# Integration tests
test_demuxer_integration_SOURCES = test_demuxer_integration.cpp
test_demuxer_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_media_factory_integration_SOURCES = test_media_factory_integration.cpp
test_media_factory_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lmpg123 -lopus -lcurl -lssl -lcrypto \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(AM_LDFLAGS)

# Performance and regression tests
test_demuxer_performance_SOURCES = test_demuxer_performance.cpp
test_demuxer_performance_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# IOHandler backward compatibility and performance validation tests
test_iohandler_legacy_compatibility_SOURCES = test_iohandler_legacy_compatibility.cpp
test_iohandler_legacy_compatibility_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

test_iohandler_performance_validation_SOURCES = test_iohandler_performance_validation.cpp
test_iohandler_performance_validation_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

test_http_client_parse_url_SOURCES = test_http_client_parse_url.cpp
test_http_client_parse_url_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)

# OggDemuxer page extraction test
if HAVE_OGGDEMUXER
test_ogg_page_extraction_SOURCES = test_ogg_page_extraction.cpp

test_ogg_fuzzing_SOURCES = test_ogg_fuzzing.cpp
test_ogg_fuzzing_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(RAPIDCHECK_LIBS) \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

test_ogg_multiplexed_seeking_SOURCES = test_ogg_multiplexed_seeking.cpp
test_ogg_multiplexed_seeking_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

test_ogg_seeking_stress_SOURCES = test_ogg_seeking_stress.cpp
test_ogg_seeking_stress_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

test_ogg_aggressive_SOURCES = test_ogg_aggressive.cpp
test_ogg_aggressive_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

check_PROGRAMS += test_stream_manager test_ogg_fuzzing test_ogg_aggressive
test_stream_manager_SOURCES = test_stream_manager.cpp
test_stream_manager_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

check_PROGRAMS += test_codec_identification
test_codec_identification_SOURCES = test_codec_identification.cpp
test_codec_identification_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

check_PROGRAMS += test_opus_parser
test_opus_parser_SOURCES = test_opus_parser.cpp
test_opus_parser_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

check_PROGRAMS += test_flac_parser
test_flac_parser_SOURCES = test_flac_parser.cpp
test_flac_parser_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

check_PROGRAMS += test_speex_parser
test_speex_parser_SOURCES = test_speex_parser.cpp
test_speex_parser_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

check_PROGRAMS += test_ogg_seeking_algorithms
test_ogg_seeking_algorithms_SOURCES = test_ogg_seeking_algorithms.cpp
test_ogg_seeking_algorithms_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS) \
	-lpthread

check_PROGRAMS += test_ogg_error_handling
test_ogg_error_handling_SOURCES = test_ogg_error_handling.cpp
test_ogg_error_handling_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

check_PROGRAMS += test_ogg_integration
test_ogg_integration_SOURCES = test_ogg_integration.cpp
test_ogg_integration_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/tag/libpsymp3-tag.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(ZLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS)

test_ogg_page_extraction_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)





# OggDemuxer codec detection test
test_ogg_codec_detection_SOURCES = test_ogg_codec_detection.cpp
test_ogg_codec_detection_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) $(VORBIS_LIBS) $(OPUS_LIBS) $(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer duration calculation test
test_ogg_duration_calculation_SOURCES = test_ogg_duration_calculation.cpp
test_ogg_duration_calculation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer time conversion test
test_ogg_time_conversion_SOURCES = test_ogg_time_conversion.cpp
test_ogg_time_conversion_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer data streaming test
test_ogg_data_streaming_SOURCES = test_ogg_data_streaming.cpp
test_ogg_data_streaming_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer memory safety test
test_ogg_memory_safety_SOURCES = test_ogg_memory_safety.cpp
test_ogg_memory_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer error handling test
test_error_handling_SOURCES = test_error_handling.cpp
test_error_handling_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer seeking integration test
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer integration test
test_ogg_demuxer_integration_SOURCES = test_ogg_demuxer_integration.cpp
test_ogg_demuxer_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
endif

# A-law codec table verification test
test_alaw_table_verification_SOURCES = test_alaw_table_verification.cpp
test_alaw_table_verification_LDADD = $(FULL_CODEC_LIBS) $(AM_LDFLAGS) $(COMMON_CODEC_LIBS)

# A-law codec sample conversion test
test_alaw_sample_conversion_SOURCES = test_alaw_sample_conversion.cpp
test_alaw_sample_conversion_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# A-law codec conversion accuracy test
test_alaw_conversion_accuracy_SOURCES = test_alaw_conversion_accuracy.cpp
test_alaw_conversion_accuracy_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# μ-law codec conversion accuracy test
test_mulaw_conversion_accuracy_SOURCES = test_mulaw_conversion_accuracy.cpp
test_mulaw_conversion_accuracy_LDADD = $(AM_LDFLAGS)

# WAV G.711 conversion test
test_wav_g711_SOURCES = test_wav_g711.cpp
test_wav_g711_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

# Improved WAV G.711 conversion test
test_wav_g711_improved_SOURCES = test_wav_g711_improved.cpp
test_wav_g711_improved_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

# Codec selection and validation test (simple)
test_codec_selection_validation_simple_SOURCES = test_codec_selection_validation_simple.cpp
test_codec_selection_validation_simple_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# Codec performance test
test_codec_performance_SOURCES = test_codec_performance.cpp
test_codec_performance_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# μ-law/A-law codec performance test
test_mulaw_alaw_performance_SOURCES = test_mulaw_alaw_performance.cpp
test_mulaw_alaw_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-lFLAC++ -lFLAC -lmpg123 -lvorbis -lopus -logg \
	$(AM_LDFLAGS)

# Codec thread safety test
test_codec_thread_safety_SOURCES = test_codec_thread_safety.cpp
test_codec_thread_safety_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# Codec concurrent instances test
test_codec_concurrent_instances_SOURCES = test_codec_concurrent_instances.cpp
test_codec_concurrent_instances_LDADD = $(COMMON_CODEC_DEPS) $(COMMON_CODEC_LIBS) $(AM_LDFLAGS)

# MPRIS types test (conditional on HAVE_DBUS)
if HAVE_DBUS
test_mpris_types_SOURCES = test_mpris_types.cpp
test_mpris_types_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_dbus_connection_manager_SOURCES = test_dbus_connection_manager.cpp
test_dbus_connection_manager_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_property_manager_SOURCES = test_property_manager.cpp player_stub.cpp
test_property_manager_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_method_handler_SOURCES = test_method_handler.cpp
test_method_handler_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_signal_emitter_SOURCES = test_signal_emitter.cpp
test_signal_emitter_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_manager_integration_SOURCES = test_mpris_manager_integration.cpp player_stub.cpp
test_mpris_manager_integration_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_manager_simple_SOURCES = test_mpris_manager_simple.cpp player_stub.cpp
test_mpris_manager_simple_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_manager_minimal_SOURCES = test_mpris_manager_minimal.cpp player_stub.cpp
test_mpris_manager_minimal_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_manager_basic_integration_SOURCES = test_mpris_manager_basic_integration.cpp player_stub.cpp
test_mpris_manager_basic_integration_LDADD = \
	$(COMMON_TEST_LIBS) \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_error_handling_SOURCES = test_mpris_error_handling.cpp
test_mpris_error_handling_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_error_integration_SOURCES = test_mpris_error_integration.cpp
test_mpris_error_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

# Vorbis Streaming Property Tests
# **Feature: vorbis-codec, Property 11: Bounded Buffer Size**
# **Feature: vorbis-codec, Property 12: Incremental Processing**
# **Feature: vorbis-codec, Property 7: Flush Outputs Remaining Samples**
# **Feature: vorbis-codec, Property 16: Reset Clears State**
# **Validates: Requirements 7.1, 7.2, 7.4, 7.5, 7.6, 4.8, 11.4, 11.5**
	libtest_utilities.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) $(VORBIS_LIBS) -lopus \
	$(AM_LDFLAGS)

# Vorbis Demuxer Integration Tests
# Task 15.1: Test integration with demuxer architecture
# **Validates: Requirements 6.1, 6.3, 11.3, 11.4, 12.8**
	libtest_utilities.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) $(VORBIS_LIBS) -lopus \
	$(AM_LDFLAGS)

# Vorbis Compatibility and Performance Tests
# Task 15.2: Validate compatibility and performance
# **Validates: Requirements 12.1, 12.2, 12.4, 13.1, 13.2, 13.8**
	libtest_utilities.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) $(VORBIS_LIBS) -lopus \
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
$(check_PROGRAMS): $(top_builddir)/src/core/libpsymp3-core.a

$(top_builddir)/src/core/libpsymp3-core.a:
	$(MAKE) -C $(top_builddir)/src rect.o

# Build rule for debug.o (core dependency)
$(top_builddir)/src/debug.o:
	$(MAKE) -C $(top_builddir)/src debug.o

# Build rules for A-law codec test dependencies
# Build rules for Audio thread safety test dependencies
$(top_builddir)/src/audio.o:
	$(MAKE) -C $(top_builddir)/src audio.o

$(top_builddir)/src/stream.o:
	$(MAKE) -C $(top_builddir)/src stream.o

$(top_builddir)/src/core/fft.o:
	$(MAKE) -C $(top_builddir)/src/core fft.o

$(top_builddir)/src/core/fft_draw.o:
	$(MAKE) -C $(top_builddir)/src/core fft_draw.o

$(top_builddir)/src/system.o:
	$(MAKE) -C $(top_builddir)/src system.o

$(top_builddir)/src/track.o:
	$(MAKE) -C $(top_builddir)/src track.o

# Build rules for Surface thread safety test dependencies
$(top_builddir)/src/surface.o:
	$(MAKE) -C $(top_builddir)/src surface.o

# Build rules for ISO demuxer test dependencies

# System-wide threading integration test
test_system_wide_threading_integration_SOURCES = test_system_wide_threading_integration.cpp
test_system_wide_threading_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Threading performance regression test
test_threading_performance_regression_SOURCES = test_threading_performance_regression.cpp
test_threading_performance_regression_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Simple codec performance test (minimal dependencies)
test_codec_performance_simple_SOURCES = test_codec_performance_simple.cpp
test_codec_performance_simple_LDADD = $(AM_LDFLAGS)

# Simple codec thread safety test (minimal dependencies)
test_codec_thread_safety_simple_SOURCES = test_codec_thread_safety_simple.cpp
test_codec_thread_safety_simple_LDADD = $(AM_LDFLAGS)

# Threading safety baseline test
test_threading_safety_baseline_SOURCES = test_threading_safety_baseline.cpp
test_threading_safety_baseline_LDADD = libtest_utilities.a -lpthread

# Audio thread safety test
test_audio_thread_safety_SOURCES = test_audio_thread_safety.cpp
test_audio_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/core/fft.o \
	$(top_builddir)/src/core/fft_draw.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(AM_LDFLAGS)

# Audio threading pattern test (simpler test without SDL dependencies)
test_audio_threading_pattern_SOURCES = test_audio_threading_pattern.cpp
test_audio_threading_pattern_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Surface thread safety test
test_surface_thread_safety_SOURCES = test_surface_thread_safety.cpp
test_surface_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Surface performance regression test
test_surface_performance_regression_SOURCES = test_surface_performance_regression.cpp
test_surface_performance_regression_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/surface.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# SimplePCMCodec integration test (minimal version)
test_simplepcmcodec_integration_minimal_SOURCES = test_simplepcmcodec_integration_minimal.cpp
test_simplepcmcodec_integration_minimal_LDADD = $(AM_LDFLAGS)

# SimplePCMCodec real integration test (with MuLaw/ALaw codecs)
test_simplepcmcodec_real_integration_SOURCES = test_simplepcmcodec_real_integration.cpp
test_simplepcmcodec_real_integration_LDADD = $(AM_LDFLAGS)

# SimplePCMCodec comprehensive integration test (minimal dependencies)
test_simplepcmcodec_comprehensive_integration_SOURCES = test_simplepcmcodec_comprehensive_integration.cpp
test_simplepcmcodec_comprehensive_integration_LDADD = $(AM_LDFLAGS)

# μ-law/A-law codec integration tests with SimplePCMCodec
test_mulaw_alaw_integration_SOURCES = test_mulaw_alaw_integration.cpp
test_mulaw_alaw_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# μ-law codec property-based tests (standalone, minimal dependencies)
test_mulaw_codec_properties_SOURCES = test_mulaw_codec_properties.cpp
test_mulaw_codec_properties_LDADD = $(AM_LDFLAGS)

# A-law codec property-based tests (standalone, minimal dependencies)
test_alaw_codec_properties_SOURCES = test_alaw_codec_properties.cpp
test_alaw_codec_properties_LDADD = $(AM_LDFLAGS)

# μ-law/A-law parameter handling property-based tests (standalone, minimal dependencies)
# Temporarily disabled - test not yet added to check_PROGRAMS
# test_mulaw_alaw_parameter_handling_SOURCES = test_mulaw_alaw_parameter_handling.cpp
# test_mulaw_alaw_parameter_handling_LDADD = $(AM_LDFLAGS)

# μ-law/A-law error state consistency property-based tests (standalone, minimal dependencies)
test_mulaw_alaw_error_state_consistency_SOURCES = test_mulaw_alaw_error_state_consistency.cpp
test_mulaw_alaw_error_state_consistency_LDADD = $(AM_LDFLAGS)

# Last.fm MD5 hash property-based tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Property 1: MD5 Hash Correctness**
# **Validates: Requirements 1.1, 1.2**

# Last.fm URL encoding property-based tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Property 2: URL Encoding Round-Trip**
# **Validates: Requirements 2.2**
test_lastfm_url_encoding_properties_SOURCES = test_lastfm_url_encoding_properties.cpp
test_lastfm_url_encoding_properties_LDADD = -lcurl $(AM_LDFLAGS)

# Last.fm exponential backoff property-based tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Property 5: Exponential Backoff Progression**
# **Feature: lastfm-performance-optimization, Property 4: Scrobble Batching Correctness**
# **Validates: Requirements 4.2, 4.3**
test_lastfm_exponential_backoff_properties_SOURCES = test_lastfm_exponential_backoff_properties.cpp
test_lastfm_exponential_backoff_properties_LDADD = $(AM_LDFLAGS)

# Last.fm lazy debug logging property-based tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Property 3: Debug Logging Lazy Evaluation**
# **Validates: Requirements 3.1, 3.3**
test_lastfm_lazy_logging_properties_SOURCES = test_lastfm_lazy_logging_properties.cpp
test_lastfm_lazy_logging_properties_LDADD = $(AM_LDFLAGS)

# Last.fm scrobble cache round-trip property-based tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Property 6: Scrobble Cache Round-Trip**
# **Validates: Requirements 5.2, 6.3, 6.4**
test_lastfm_scrobble_cache_roundtrip_properties_SOURCES = test_lastfm_scrobble_cache_roundtrip_properties.cpp
test_lastfm_scrobble_cache_roundtrip_properties_LDADD = $(AM_LDFLAGS)

# Last.fm configuration file backward compatibility tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Requirements 6.1, 6.2**
test_lastfm_config_backward_compatibility_SOURCES = test_lastfm_config_backward_compatibility.cpp
test_lastfm_config_backward_compatibility_LDADD = $(AM_LDFLAGS)

# Last.fm configuration round-trip property-based tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Property 7: Configuration Round-Trip**
# **Validates: Requirements 6.2**
test_lastfm_config_roundtrip_properties_SOURCES = test_lastfm_config_roundtrip_properties.cpp
test_lastfm_config_roundtrip_properties_LDADD = $(AM_LDFLAGS)

# Last.fm thread safety property-based tests (standalone, minimal dependencies)
# **Feature: lastfm-performance-optimization, Property 8: Thread-Safe Queue Operations**
# **Validates: Requirements 7.1, 7.4**
test_lastfm_thread_safety_properties_SOURCES = test_lastfm_thread_safety_properties.cpp
test_lastfm_thread_safety_properties_LDADD = -lpthread $(AM_LDFLAGS)

# FLAC stream marker validation property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 1: Stream Marker Validation**
# **Validates: Requirements 1.2, 1.3**
test_flac_stream_marker_properties_SOURCES = test_flac_stream_marker_properties.cpp
test_flac_stream_marker_properties_LDADD = $(AM_LDFLAGS)

# FLAC metadata block header property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 2: Metadata Block Header Bit Extraction**
# **Validates: Requirements 2.2, 2.3**
# **Feature: flac-demuxer, Property 3: Forbidden Block Type Detection**
# **Validates: Requirements 2.4, 18.1**
test_flac_metadata_block_header_properties_SOURCES = test_flac_metadata_block_header_properties.cpp
test_flac_metadata_block_header_properties_LDADD = $(AM_LDFLAGS)

# FLAC STREAMINFO block size property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 4: STREAMINFO Block Size Validation**
# **Validates: Requirements 3.6, 3.7**
# **Feature: flac-demuxer, Property 5: STREAMINFO Block Size Ordering**
# **Validates: Requirements 3.8**
test_flac_streaminfo_block_size_properties_SOURCES = test_flac_streaminfo_block_size_properties.cpp
test_flac_streaminfo_block_size_properties_LDADD = $(AM_LDFLAGS)

# FLAC SEEKTABLE placeholder detection property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 15: Seek Point Placeholder Detection**
# **Validates: Requirements 12.5**
test_flac_seektable_placeholder_properties_SOURCES = test_flac_seektable_placeholder_properties.cpp
test_flac_seektable_placeholder_properties_LDADD = $(AM_LDFLAGS)

# FLAC VORBIS_COMMENT endianness handling property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 16: Endianness Handling**
# **Validates: Requirements 19.1, 19.4, 13.1**
test_flac_vorbis_comment_endianness_properties_SOURCES = test_flac_vorbis_comment_endianness_properties.cpp
test_flac_vorbis_comment_endianness_properties_LDADD = $(AM_LDFLAGS)

# FLAC CUESHEET track count validation property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 17: CUESHEET Track Count Validation**
# **Validates: Requirements 16.6**
test_flac_cuesheet_track_count_properties_SOURCES = test_flac_cuesheet_track_count_properties.cpp
test_flac_cuesheet_track_count_properties_LDADD = $(AM_LDFLAGS)

# FLAC frame sync code detection property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 6: Frame Sync Code Detection**
# **Validates: Requirements 4.1, 4.2**
# **Feature: flac-demuxer, Property 7: Blocking Strategy Consistency**
# **Validates: Requirements 4.8**
test_flac_frame_sync_properties_SOURCES = test_flac_frame_sync_properties.cpp
test_flac_frame_sync_properties_LDADD = $(AM_LDFLAGS)

# FLAC block size bits parsing property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 8: Reserved Block Size Pattern Detection**
# **Validates: Requirements 5.2**
# **Feature: flac-demuxer, Property 9: Forbidden Block Size Detection**
# **Validates: Requirements 5.18**
test_flac_block_size_bits_properties_SOURCES = test_flac_block_size_bits_properties.cpp
test_flac_block_size_bits_properties_LDADD = $(AM_LDFLAGS)

# FLAC sample rate bits parsing property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 10: Forbidden Sample Rate Detection**
# **Validates: Requirements 6.17**
test_flac_sample_rate_bits_properties_SOURCES = test_flac_sample_rate_bits_properties.cpp
test_flac_sample_rate_bits_properties_LDADD = $(AM_LDFLAGS)

# FLAC CRC-8 calculation property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 13: CRC-8 Calculation Correctness**
# **Validates: Requirements 10.2**
test_flac_crc8_properties_SOURCES = test_flac_crc8_properties.cpp
test_flac_crc8_properties_LDADD = $(AM_LDFLAGS)

# FLAC CRC-16 calculation property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 14: CRC-16 Calculation Correctness**
# **Validates: Requirements 11.3**
test_flac_crc16_properties_SOURCES = test_flac_crc16_properties.cpp
test_flac_crc16_properties_LDADD = $(AM_LDFLAGS)

# FLAC frame size estimation property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 18: Frame Size Estimation**
# **Validates: Requirements 21.1**
test_flac_frame_size_estimation_properties_SOURCES = test_flac_frame_size_estimation_properties.cpp
test_flac_frame_size_estimation_properties_LDADD = $(AM_LDFLAGS)

# FLAC frame boundary search property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 19: Frame Boundary Search Limit**
# **Validates: Requirements 21.3**
test_flac_frame_boundary_search_properties_SOURCES = test_flac_frame_boundary_search_properties.cpp
test_flac_frame_boundary_search_properties_LDADD = $(AM_LDFLAGS)

# FLAC duration calculation property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 20: Duration Calculation**
# **Validates: Requirements 23.1, 23.4**
test_flac_duration_calculation_properties_SOURCES = test_flac_duration_calculation_properties.cpp
test_flac_duration_calculation_properties_LDADD = $(AM_LDFLAGS)

# FLAC sync resynchronization property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 21: Error Recovery - Sync Resynchronization**
# **Validates: Requirements 24.4**
test_flac_sync_resync_properties_SOURCES = test_flac_sync_resync_properties.cpp
test_flac_sync_resync_properties_LDADD = $(AM_LDFLAGS)

# FLAC thread safety lock pattern property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 22: Thread Safety - Lock Pattern**
# **Validates: Requirements 28.1, 28.2**
test_flac_thread_safety_lock_pattern_properties_SOURCES = test_flac_thread_safety_lock_pattern_properties.cpp
test_flac_thread_safety_lock_pattern_properties_LDADD = $(AM_LDFLAGS) -lpthread

# FLAC debug logging format property-based tests (standalone, minimal dependencies)
# **Feature: flac-demuxer, Property 23: Debug Logging Format**
# **Validates: Requirements 29.1**
test_flac_debug_logging_format_properties_SOURCES = test_flac_debug_logging_format_properties.cpp
test_flac_debug_logging_format_properties_LDADD = $(AM_LDFLAGS)

# μ-law/A-law codec selection unit tests (standalone, minimal dependencies)
test_mulaw_alaw_codec_selection_SOURCES = test_mulaw_alaw_codec_selection.cpp
test_mulaw_alaw_codec_selection_LDADD = $(AM_LDFLAGS)

# μ-law/A-law error handling unit tests (standalone, minimal dependencies)
test_mulaw_alaw_error_handling_SOURCES = test_mulaw_alaw_error_handling.cpp
test_mulaw_alaw_error_handling_LDADD = $(AM_LDFLAGS)

# Conditional codec compilation test
test_conditional_codec_compilation_SOURCES = test_conditional_codec_compilation.cpp
test_conditional_codec_compilation_LDADD = libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Note: This test only checks conditional compilation flags
# No codec object files are needed since we don't instantiate the classes

# FLAC demuxer simple test (minimal dependencies)
if HAVE_FLAC
test_flac_demuxer_simple_SOURCES = test_flac_demuxer_simple.cpp
test_flac_demuxer_simple_LDADD = \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer real file test
test_flac_real_file_SOURCES = test_flac_real_file.cpp
test_flac_real_file_LDADD = \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC STREAMINFO real files test
test_streaminfo_real_files_SOURCES = test_streaminfo_real_files.cpp
test_streaminfo_real_files_LDADD = \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC STREAMINFO recovery test
test_streaminfo_recovery_SOURCES = test_streaminfo_recovery.cpp
test_streaminfo_recovery_LDADD = \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC STREAMINFO debug/extraction test (standalone, no library dependencies)
test_streaminfo_debug_SOURCES = test_streaminfo_debug.cpp
test_streaminfo_debug_LDADD = $(AM_LDFLAGS)

# FLAC demuxer diagnostic test
test_flac_diagnostic_SOURCES = test_flac_diagnostic.cpp
test_flac_diagnostic_LDADD = \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# IOHandler deadlock test
test_iohandler_deadlock_SOURCES = test_iohandler_deadlock.cpp
test_iohandler_deadlock_LDADD = \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# FLAC demuxer compatibility test
test_flac_demuxer_compatibility_SOURCES = test_flac_demuxer_compatibility.cpp
test_flac_demuxer_compatibility_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)



# FLAC demuxer performance test
test_flac_demuxer_performance_SOURCES = test_flac_demuxer_performance.cpp
test_flac_demuxer_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC compatibility integration test
test_flac_compatibility_integration_SOURCES = test_flac_compatibility_integration.cpp
test_flac_compatibility_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer thread safety test
test_flac_demuxer_thread_safety_SOURCES = test_flac_demuxer_thread_safety.cpp
test_flac_demuxer_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC backward compatibility validation test (Requirements 27.1-27.8)
test_flac_backward_compatibility_validation_SOURCES = test_flac_backward_compatibility_validation.cpp
test_flac_backward_compatibility_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS) \
	-lpthread

# FLAC demuxer real file integration tests (discovers FLAC files in tests/data/)
test_flac_demuxer_real_files_SOURCES = test_flac_demuxer_real_files.cpp
test_flac_demuxer_real_files_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS) \
	-lpthread

# FLAC bisection seeking real file tests (Task 7.1, 7.2 from flac-bisection-seeking spec)
test_flac_bisection_real_files_SOURCES = test_flac_bisection_real_files.cpp
test_flac_bisection_real_files_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS) \
	-lpthread

# FLAC mini player test (Task 7.1, 7.2 from flac-bisection-seeking spec)
test_flac_mini_player_SOURCES = test_flac_mini_player.cpp
test_flac_mini_player_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS) \
	-lpthread

# FLAC demuxer comprehensive unit tests
test_flac_demuxer_unit_comprehensive_SOURCES = test_flac_demuxer_unit_comprehensive.cpp
test_flac_demuxer_unit_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer comprehensive integration tests
test_flac_demuxer_integration_comprehensive_SOURCES = test_flac_demuxer_integration_comprehensive.cpp
test_flac_demuxer_integration_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC demuxer unit tests (fixed, no external files required)
test_flac_demuxer_unit_fixed_SOURCES = test_flac_demuxer_unit_fixed.cpp
test_flac_demuxer_unit_fixed_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC index-based seeking test
test_flac_index_based_seeking_SOURCES = test_flac_index_based_seeking.cpp
test_flac_index_based_seeking_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec variable block size test
test_flac_variable_block_size_SOURCES = test_flac_variable_block_size.cpp
test_flac_variable_block_size_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec minimal container-agnostic test
test_flac_codec_minimal_container_agnostic_SOURCES = test_flac_codec_minimal_container_agnostic.cpp
test_flac_codec_minimal_container_agnostic_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec container-agnostic operation test
# Note: This test requires HAVE_NATIVE_FLAC; when not defined, it's a skip stub
test_flac_codec_container_agnostic_SOURCES = test_flac_codec_container_agnostic.cpp
test_flac_codec_container_agnostic_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)


# Simple FLAC codec quality validation test (minimal dependencies)
test_flac_quality_simple_SOURCES = test_flac_quality_simple.cpp

# FLAC thread safety validation test
test_flac_thread_safety_validation_SOURCES = test_flac_thread_safety_validation.cpp
test_flac_thread_safety_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
test_flac_quality_simple_LDADD = \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# Minimal FLAC codec unit tests
test_flac_codec_unit_minimal_SOURCES = test_flac_codec_unit_minimal.cpp
test_flac_codec_unit_minimal_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

endif

# IOHandler thread safety comprehensive test
test_iohandler_thread_safety_comprehensive_SOURCES = test_iohandler_thread_safety_comprehensive.cpp
test_iohandler_thread_safety_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(AM_LDFLAGS)

# IOHandler memory deadlock prevention test
test_iohandler_memory_deadlock_prevention_SOURCES = test_iohandler_memory_deadlock_prevention.cpp
test_iohandler_memory_deadlock_prevention_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# MemoryPoolManager integration test
test_memory_pool_manager_integration_SOURCES = test_memory_pool_manager_integration.cpp
test_memory_pool_manager_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# MemoryPoolManager comprehensive thread safety test
test_memory_pool_manager_thread_safety_comprehensive_SOURCES = test_memory_pool_manager_thread_safety_comprehensive.cpp
test_memory_pool_manager_thread_safety_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# MemoryPoolManager basic threading test
test_memory_pool_manager_basic_threading_SOURCES = test_memory_pool_manager_basic_threading.cpp
test_memory_pool_manager_basic_threading_LDADD = \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# MemoryPoolManager allocation failure test
test_memory_pool_allocation_failure_SOURCES = test_memory_pool_allocation_failure.cpp
test_memory_pool_allocation_failure_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_frame_size_estimation_consistency_SOURCES = test_frame_size_estimation_consistency.cpp
test_frame_size_estimation_consistency_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)
# FLAC AudioFrame creation test (uses libFLAC wrapper FLACCodec, not native codec)
# NOTE: This test is disabled when HAVE_NATIVE_FLAC is defined because it uses
# the libFLAC++ wrapper (FLACCodec) which is incompatible with the native codec.
# To enable this test, configure with --disable-native-flac
#test_flac_audioframe_creation_SOURCES = test_flac_audioframe_creation.cpp
#test_flac_audioframe_creation_LDADD = \
#	$(COMMON_TEST_OBJS) \
#	$(EXTENDED_TEST_OBJS) \
#	libtest_utilities.a \
#	$(top_builddir)/src/widget/ui/libpsymp3-widget-ui.a \
#	$(top_builddir)/src/widget/windowing/libpsymp3-widget-windowing.a \
#	$(top_builddir)/src/widget/foundation/libpsymp3-widget-foundation.a \
#	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
#	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
#	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
#	$(top_builddir)/src/io/libpsymp3-io.a \
#	$(top_builddir)/src/lastfm/libpsymp3-lastfm.a \
#	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
#	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
#	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
#	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
#	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
#	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
#	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
#	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
#	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
#	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
#	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
#	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
#	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
#	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
#	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
#	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
#	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
#	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
#	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
#	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
#	$(top_builddir)/src/io/libpsymp3-io.a \
#	-lFLAC++ -lFLAC \
#	$(SDL_LIBS) \
#	$(TAGLIB_LIBS) \
#	$(FREETYPE_LIBS) \
#	$(OPENSSL_LIBS) \
#	$(CURL_LIBS) \
#	$(LIBMPG123_LIBS) \
#	$(VORBIS_LIBS) \
#	$(OPUS_LIBS) \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
#	$(OGG_LIBS) \
#	$(DBUS_LIBS) \
#	$(AM_LDFLAGS)

# Note: FLACCodec.o and EnhancedAudioBufferPool.o build rules already defined above

# FLAC codec compatibility test
test_flac_codec_compatibility_SOURCES = test_flac_codec_compatibility.cpp
test_flac_codec_compatibility_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec integration test
test_flac_codec_integration_SOURCES = test_flac_codec_integration.cpp
test_flac_codec_integration_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)


# Comprehensive FLAC codec unit tests
test_flac_codec_unit_comprehensive_SOURCES = test_flac_codec_unit_comprehensive.cpp
test_flac_codec_unit_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec error handling tests
test_flac_codec_error_handling_SOURCES = test_flac_codec_error_handling.cpp
test_flac_codec_error_handling_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec performance comprehensive tests
test_flac_codec_performance_comprehensive_SOURCES = test_flac_codec_performance_comprehensive.cpp
test_flac_codec_performance_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec multi-threaded tests
test_flac_codec_multithreaded_SOURCES = test_flac_codec_multithreaded.cpp
test_flac_codec_multithreaded_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec demuxer integration tests
test_flac_codec_demuxer_integration_SOURCES = test_flac_codec_demuxer_integration.cpp
test_flac_codec_demuxer_integration_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec performance benchmarking tests
test_flac_performance_benchmarking_SOURCES = test_flac_performance_benchmarking.cpp
test_flac_performance_benchmarking_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec conditional compilation tests
test_flac_conditional_compilation_SOURCES = test_flac_conditional_compilation.cpp
test_flac_conditional_compilation_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC binary search limitations test
test_flac_binary_search_limitations_SOURCES = test_flac_binary_search_limitations.cpp
test_flac_binary_search_limitations_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec threading safety test


# FLAC demuxer threading test
test_flac_demuxer_threading_SOURCES = test_flac_demuxer_threading.cpp
test_flac_demuxer_threading_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC performance optimization test
test_flac_performance_optimization_SOURCES = test_flac_performance_optimization.cpp
test_flac_performance_optimization_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC performance with real files test
test_flac_performance_with_real_files_SOURCES = test_flac_performance_with_real_files.cpp
test_flac_performance_with_real_files_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC comprehensive validation test
test_flac_comprehensive_validation_SOURCES = test_flac_comprehensive_validation.cpp
test_flac_comprehensive_validation_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC RFC 9639 compliance validation test
test_flac_rfc_compliance_SOURCES = test_flac_rfc_compliance.cpp
test_flac_rfc_compliance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC RFC 9639 subframe type validation test
test_flac_rfc_subframe_validation_SOURCES = test_flac_rfc_subframe_validation.cpp
test_flac_rfc_subframe_validation_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC RFC 9639 entropy coding validation test
test_flac_rfc_entropy_coding_validation_SOURCES = test_flac_rfc_entropy_coding_validation.cpp
test_flac_rfc_entropy_coding_validation_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Simple entropy coding compilation test
test_entropy_coding_compilation_SOURCES = test_entropy_coding_compilation.cpp
test_entropy_coding_compilation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# FLAC test data validation test
test_flac_test_data_validation_SOURCES = test_flac_test_data_validation.cpp
test_flac_test_data_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# FLAC CRC validation test (RFC 9639 compliance)
test_flac_crc_validation_SOURCES = test_flac_crc_validation.cpp
test_flac_crc_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# RFC 9639 bit depth and sample format compliance validation test
test_flac_rfc_bit_depth_validation_SOURCES = test_flac_rfc_bit_depth_validation.cpp
test_flac_rfc_bit_depth_validation_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Simple RFC 9639 bit depth validation test (minimal dependencies)
test_flac_bit_depth_validation_simple_SOURCES = test_flac_bit_depth_validation_simple.cpp
test_flac_bit_depth_validation_simple_LDADD = $(AM_LDFLAGS)

# FLAC security validation tests
test_flac_security_validation_SOURCES = test_flac_security_validation.cpp
test_flac_security_validation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# RFC 9639 sample format and bit depth compliance test
test_flac_sample_format_rfc9639_compliance_SOURCES = test_flac_sample_format_rfc9639_compliance.cpp
test_flac_sample_format_rfc9639_compliance_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer performance test
test_oggdemuxer_performance_SOURCES = test_oggdemuxer_performance.cpp
test_oggdemuxer_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer comprehensive test suite
test_oggdemuxer_comprehensive_SOURCES = test_oggdemuxer_comprehensive.cpp
test_oggdemuxer_comprehensive_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer comprehensive integration test (Task 20)
test_ogg_comprehensive_integration_SOURCES = test_ogg_comprehensive_integration.cpp
test_ogg_comprehensive_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# Opus Codec Tests
test_opus_codec_core_decoding_SOURCES = test_opus_codec_core_decoding.cpp
test_opus_codec_core_decoding_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	-lmpg123 \
	$(AM_LDFLAGS)

test_opus_codec_error_handling_SOURCES = test_opus_codec_error_handling.cpp
test_opus_codec_error_handling_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	-lmpg123 \
	$(AM_LDFLAGS)

test_opus_integration_SOURCES = test_opus_integration.cpp
test_opus_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	-lmpg123 \
	$(AM_LDFLAGS)

test_opus_compatibility_performance_SOURCES = test_opus_compatibility_performance.cpp
test_opus_compatibility_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	-logg -lvorbis -lvorbisenc \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	-lmpg123 \
	$(AM_LDFLAGS)

# MPRIS Mock Framework Tests
if HAVE_DBUS
test_mpris_mock_framework_SOURCES = test_mpris_mock_framework.cpp
test_mpris_mock_framework_LDADD = libtest_utilities.a $(AM_LDFLAGS)

test_mpris_stress_testing_SOURCES = test_mpris_stress_testing.cpp
test_mpris_stress_testing_LDADD = libtest_utilities.a $(AM_LDFLAGS)

test_comprehensive_mock_framework_SOURCES = test_comprehensive_mock_framework.cpp player_stub.cpp
test_comprehensive_mock_framework_LDADD = libtest_utilities.a $(top_builddir)/src/mpris/libpsymp3-mpris.a $(top_builddir)/src/core/libpsymp3-core.a $(top_builddir)/src/debug.o $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_mock_framework_standalone_SOURCES = test_mock_framework_standalone.cpp
test_mock_framework_standalone_LDADD = libtest_utilities.a $(AM_LDFLAGS)

# Comprehensive MPRIS component tests
test_dbus_connection_manager_comprehensive_SOURCES = test_dbus_connection_manager_comprehensive.cpp player_stub.cpp
test_dbus_connection_manager_comprehensive_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_property_manager_comprehensive_SOURCES = test_property_manager_comprehensive.cpp player_stub.cpp
test_property_manager_comprehensive_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_method_handler_comprehensive_SOURCES = test_method_handler_comprehensive.cpp player_stub.cpp
test_method_handler_comprehensive_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_method_handler_simple_SOURCES = test_method_handler_simple.cpp player_stub.cpp
test_method_handler_simple_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_signal_emitter_comprehensive_SOURCES = test_signal_emitter_comprehensive.cpp player_stub.cpp
test_signal_emitter_comprehensive_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_manager_comprehensive_SOURCES = test_mpris_manager_comprehensive.cpp player_stub.cpp
test_mpris_manager_comprehensive_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_logger_basic_SOURCES = test_mpris_logger_basic.cpp
test_mpris_logger_basic_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_logger_dbus_tracing_SOURCES = test_mpris_logger_dbus_tracing.cpp player_stub.cpp
test_mpris_logger_dbus_tracing_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

test_mpris_logger_performance_SOURCES = test_mpris_logger_performance.cpp
test_mpris_logger_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)
endif

# MPRIS Integration Tests
test_mpris_spec_compliance_SOURCES = test_mpris_spec_compliance.cpp player_stub.cpp
test_mpris_spec_compliance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_concurrent_clients_SOURCES = test_mpris_concurrent_clients.cpp player_stub.cpp
test_mpris_concurrent_clients_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_reconnection_behavior_SOURCES = test_mpris_reconnection_behavior.cpp player_stub.cpp
test_mpris_reconnection_behavior_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)

test_mpris_memory_validation_SOURCES = test_mpris_memory_validation.cpp player_stub.cpp
test_mpris_memory_validation_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)

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
test_mpris_performance_profiler_LDADD = \
	$(AM_LDFLAGS)

test_mpris_regression_validation_SOURCES = test_mpris_regression_validation.cpp player_stub.cpp fake_player.h
test_mpris_regression_validation_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)
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

# ISO Demuxer Comprehensive Integration Tests (Task 21)

# ISO Demuxer FLAC Integration Tests

# ISO Demuxer Performance and Optimization Tests

# ISO Demuxer Additional Tests

# RFC 9639 compliance validator test (requires native FLAC codec)
if HAVE_NATIVE_FLAC
if HAVE_FLAC
test_flac_rfc9639_compliance_validator_SOURCES = test_flac_rfc9639_compliance_validator.cpp FLACRFCComplianceValidator.cpp
test_flac_rfc9639_compliance_validator_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS) \
	-logg -lvorbis -lvorbisenc -lopus -lmpg123 -lcurl -lssl -lcrypto -ltag
endif
endif

# RFC Compliance Debugging and Validation Tools Test
# Temporarily disabled due to missing class implementations
# noinst_PROGRAMS += test_rfc_compliance_debugging_tools
# test_rfc_compliance_debugging_tools_SOURCES = test_rfc_compliance_debugging_tools.cpp FLACRFCComplianceValidator.cpp
# test_rfc_compliance_debugging_tools_LDADD = \
#         libtest_utilities.a \
#         $(top_builddir)/src/debug.o \
#         $(top_builddir)/src/core/libpsymp3-core.a \
#         $(FLAC_LIBS) \
#         $(AM_LDFLAGS)

# FLAC audio output test (verifies synthetic STREAMINFO fix)
test_flac_audio_output_SOURCES = test_flac_audio_output.cpp
test_flac_audio_output_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC seeking crash test (reproduces memory corruption during seeking)
test_flac_seeking_crash_SOURCES = test_flac_seeking_crash.cpp
test_flac_seeking_crash_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC audio output debug test (comprehensive pipeline debugging)
test_flac_audio_output_debug_SOURCES = test_flac_audio_output_debug.cpp
test_flac_audio_output_debug_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC codec simple debug test (minimal codec testing)
test_flac_codec_simple_debug_SOURCES = test_flac_codec_simple_debug.cpp
test_flac_codec_simple_debug_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# FLAC streamable subset real files test
test_flac_streamable_subset_real_files_SOURCES = test_flac_streamable_subset_real_files.cpp
test_flac_streamable_subset_real_files_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Debug system test
noinst_PROGRAMS += test_debug_system
test_debug_system_SOURCES = test_debug_system.cpp
test_debug_system_LDADD = \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Native FLAC codec unit tests (conditional on HAVE_NATIVE_FLAC)
if HAVE_NATIVE_FLAC
check_PROGRAMS += test_bitstream_reader_unit test_frame_parser_unit test_subframe_decoder_unit \
	test_residual_decoder_unit test_channel_decorrelator_unit test_sample_reconstructor_unit \
	test_native_flac_real_files test_native_flac_containers test_native_flac_edge_cases \
	test_native_flac_performance_benchmark test_native_flac_memory_usage test_native_flac_threading \
	test_flac_rfc9639_compliance_validator

# BitstreamReader unit tests
test_bitstream_reader_unit_SOURCES = test_bitstream_reader_unit.cpp
test_bitstream_reader_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# FrameParser unit tests
test_frame_parser_unit_SOURCES = test_frame_parser_unit.cpp
test_frame_parser_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# SubframeDecoder unit tests
test_subframe_decoder_unit_SOURCES = test_subframe_decoder_unit.cpp
test_subframe_decoder_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# ResidualDecoder unit tests
test_residual_decoder_unit_SOURCES = test_residual_decoder_unit.cpp
test_residual_decoder_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# ChannelDecorrelator unit tests
test_channel_decorrelator_unit_SOURCES = test_channel_decorrelator_unit.cpp
test_channel_decorrelator_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# SampleReconstructor unit tests
test_sample_reconstructor_unit_SOURCES = test_sample_reconstructor_unit.cpp
test_sample_reconstructor_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Integration test: Real FLAC files
test_native_flac_real_files_SOURCES = test_native_flac_real_files.cpp
test_native_flac_real_files_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Integration test: Container formats
test_native_flac_containers_SOURCES = test_native_flac_containers.cpp
test_native_flac_containers_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS) \
	-logg

# Integration test: Edge cases
test_native_flac_edge_cases_SOURCES = test_native_flac_edge_cases.cpp
test_native_flac_edge_cases_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Performance test: Benchmark against libFLAC
test_native_flac_performance_benchmark_SOURCES = test_native_flac_performance_benchmark.cpp
test_native_flac_performance_benchmark_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Performance test: Memory usage
test_native_flac_memory_usage_SOURCES = test_native_flac_memory_usage.cpp
test_native_flac_memory_usage_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Performance test: Threading
test_native_flac_threading_SOURCES = test_native_flac_threading.cpp
test_native_flac_threading_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

endif

# Fixed predictor overflow tests (outside HAVE_NATIVE_FLAC - standalone test)
check_PROGRAMS += test_fixed_predictor_overflow
test_fixed_predictor_overflow_SOURCES = test_fixed_predictor_overflow.cpp
test_fixed_predictor_overflow_LDADD = \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)





# OggDemuxer property-based tests (RFC 3533 compliance)
# Works with or without RapidCheck - falls back to exhaustive unit tests



# ============================================================================
# FLAC Demuxer Property-Based Tests
# ============================================================================

# **Feature: flac-demuxer, Property 11: Reserved Channel Bits Detection**
# **Validates: Requirements 7.7**
check_PROGRAMS += test_flac_channel_bits_properties
test_flac_channel_bits_properties_SOURCES = test_flac_channel_bits_properties.cpp
test_flac_channel_bits_properties_LDFLAGS = 
test_flac_channel_bits_properties_LDADD = 

# **Feature: flac-demuxer, Property 12: Reserved Bit Depth Detection**
# **Validates: Requirements 8.5**
check_PROGRAMS += test_flac_bit_depth_bits_properties
test_flac_bit_depth_bits_properties_SOURCES = test_flac_bit_depth_bits_properties.cpp
test_flac_bit_depth_bits_properties_LDFLAGS = 
test_flac_bit_depth_bits_properties_LDADD = 

# **Feature: flac-bisection-seeking, Property 1: Byte Position Estimation Correctness**
# **Validates: Requirements 1.1, 1.2, 1.4, 1.5**
check_PROGRAMS += test_flac_byte_position_estimation_properties
test_flac_byte_position_estimation_properties_SOURCES = test_flac_byte_position_estimation_properties.cpp
test_flac_byte_position_estimation_properties_LDFLAGS = 
test_flac_byte_position_estimation_properties_LDADD = 

# **Feature: flac-bisection-seeking, Property 6: Bisection Range Adjustment**
# **Validates: Requirements 3.1, 3.2**
check_PROGRAMS += test_flac_bisection_range_adjustment_properties
test_flac_bisection_range_adjustment_properties_SOURCES = test_flac_bisection_range_adjustment_properties.cpp
test_flac_bisection_range_adjustment_properties_LDFLAGS = 
test_flac_bisection_range_adjustment_properties_LDADD = 

# **Feature: flac-bisection-seeking, Property 7: Convergence Termination**
# **Validates: Requirements 3.3, 3.4, 3.5, 3.6**
check_PROGRAMS += test_flac_bisection_convergence_properties
test_flac_bisection_convergence_properties_SOURCES = test_flac_bisection_convergence_properties.cpp
test_flac_bisection_convergence_properties_LDFLAGS = 
test_flac_bisection_convergence_properties_LDADD =


# **Feature: flac-bisection-seeking, Property 8: Time Differential Calculation**
# **Validates: Requirements 4.1, 4.2**
check_PROGRAMS += test_flac_time_differential_properties
test_flac_time_differential_properties_SOURCES = test_flac_time_differential_properties.cpp
test_flac_time_differential_properties_LDFLAGS = 
test_flac_time_differential_properties_LDADD =

# **Feature: flac-bisection-seeking, Property 9: Best Position Selection**
# **Validates: Requirements 4.3, 4.4**
check_PROGRAMS += test_flac_best_position_selection_properties
test_flac_best_position_selection_properties_SOURCES = test_flac_best_position_selection_properties.cpp
test_flac_best_position_selection_properties_LDFLAGS = 
test_flac_best_position_selection_properties_LDADD =

# **Feature: flac-bisection-seeking, Property 10: Strategy Priority**
# **Validates: Requirements 7.1, 7.2, 7.3, 7.5**
check_PROGRAMS += test_flac_strategy_priority_properties
test_flac_strategy_priority_properties_SOURCES = test_flac_strategy_priority_properties.cpp
test_flac_strategy_priority_properties_LDFLAGS = 
test_flac_strategy_priority_properties_LDADD =


# ============================================================================
# flacout - FLAC to PCM/WAV decoder utility
# ============================================================================
# This utility decodes FLAC files to either raw PCM or RIFF WAVE format.
# It serves as a test of the native FLAC decoder implementation.
#
# Usage:
#   flacout [options] input.flac [output.wav|output.pcm]
#
# Options:
#   -r, --raw       Output raw PCM instead of WAVE
#   -q, --quiet     Suppress progress output
#   -v, --verbose   Show detailed decoding information
#   -h, --help      Show help message
#
# Note: Output is 16-bit signed PCM. High bit-depth sources are converted.
# ============================================================================

if HAVE_FLAC
noinst_PROGRAMS += flacout

flacout_SOURCES = flacout.cpp
flacout_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	-logg -lvorbis -lvorbisenc -lopus -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)
endif

# ============================================================================
# vorbisout - Vorbis to PCM/WAV decoder utility
# ============================================================================
#
# Usage:
#   vorbisout [options] input.ogg [output.wav|output.pcm]
#
# Options:
#   -r, --raw       Output raw PCM instead of WAVE (default: WAVE)
#   -q, --quiet     Suppress progress output
#   -v, --verbose   Show detailed decoding information
#   -h, --help      Show this help message
#
# ============================================================================

if HAVE_VORBIS
noinst_PROGRAMS += vorbisout

vorbisout_SOURCES = vorbisout.cpp
vorbisout_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(VORBIS_LIBS) \
	-logg -lopus -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(FLAC_LIBS)
endif

# ============================================================================
# mp3out - MP3 to PCM/WAV decoder utility
# ============================================================================
#
# Usage:
#   mp3out [options] input.mp3 [output.wav|output.pcm]
#
# Options:
#   -r, --raw       Output raw PCM instead of WAVE (default: WAVE)
#   -q, --quiet     Suppress progress output
#   -v, --verbose   Show detailed decoding information
#   -h, --help      Show this help message
#
# ============================================================================

if HAVE_MP3
noinst_PROGRAMS += mp3out

mp3out_SOURCES = mp3out.cpp
mp3out_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(LIBMPG123_LIBS) \
	-logg -lvorbis -lvorbisenc -lopus -lmpg123 -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS)
endif


# ============================================================================
# Tag Framework Tests
# ============================================================================

check_PROGRAMS += test_tag_unit test_tag_properties test_id3v2_tag test_tag_factory test_tag_error_handling_manual test_demuxer_tag_extraction test_stream_tag_integration test_tag_thread_safety test_tag_picture_properties test_tag_picture_unit

# Tag unit tests
test_tag_unit_SOURCES = test_tag_unit.cpp
test_tag_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# Tag property-based tests
test_tag_properties_SOURCES = test_tag_properties.cpp
if HAVE_RAPIDCHECK
test_tag_properties_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(RAPIDCHECK_LIBS) \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)
else
test_tag_properties_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)
endif

# ID3v2 tag unit tests
test_id3v2_tag_SOURCES = test_id3v2_tag.cpp
test_id3v2_tag_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# TagFactory unit tests
test_tag_factory_SOURCES = test_tag_factory.cpp
test_tag_factory_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# Tag error handling tests
test_tag_error_handling_manual_SOURCES = test_tag_error_handling_manual.cpp
test_tag_error_handling_manual_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# Demuxer tag extraction tests
test_demuxer_tag_extraction_SOURCES = test_demuxer_tag_extraction.cpp
test_demuxer_tag_extraction_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(AM_LDFLAGS)

# Stream-Tag integration tests (Property 11: Stream-Tag Delegation Consistency)
test_stream_tag_integration_SOURCES = test_stream_tag_integration.cpp
if HAVE_RAPIDCHECK
test_stream_tag_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(RAPIDCHECK_LIBS) \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	-lvorbis -lmpg123 -lopus -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(FLAC_LIBS) $(TAGLIB_LIBS)
else
test_stream_tag_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	-lvorbis -lmpg123 -lopus -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(FLAC_LIBS) $(TAGLIB_LIBS)
endif

# Tag thread safety tests (Property 12: Thread-Safe Concurrent Reads)
test_tag_thread_safety_SOURCES = test_tag_thread_safety.cpp
if HAVE_RAPIDCHECK
test_tag_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(RAPIDCHECK_LIBS) \
	$(AM_LDFLAGS) -lpthread
else
test_tag_thread_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS) -lpthread
endif

# Tag picture access property tests (Property 13: Picture Index Access)
test_tag_picture_properties_SOURCES = test_tag_picture_properties.cpp
if HAVE_RAPIDCHECK
test_tag_picture_properties_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(RAPIDCHECK_LIBS) \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)
else
test_tag_picture_properties_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)
endif

# Tag picture access unit tests
test_tag_picture_unit_SOURCES = test_tag_picture_unit.cpp
test_tag_picture_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# Tag fuzzing tests and corpus generators
check_PROGRAMS += fuzz_tag_vorbis fuzz_tag_id3v1 fuzz_tag_id3v2 fuzz_tag_picture
check_PROGRAMS += generate_vorbis_corpus generate_id3v1_corpus generate_id3v2_corpus generate_picture_corpus

# VorbisComment fuzzer
fuzz_tag_vorbis_SOURCES = fuzz_tag_vorbis.cpp
fuzz_tag_vorbis_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# ID3v1 fuzzer
fuzz_tag_id3v1_SOURCES = fuzz_tag_id3v1.cpp
fuzz_tag_id3v1_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# ID3v2 fuzzer
fuzz_tag_id3v2_SOURCES = fuzz_tag_id3v2.cpp
fuzz_tag_id3v2_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# Picture data fuzzer
fuzz_tag_picture_SOURCES = fuzz_tag_picture.cpp
fuzz_tag_picture_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# VorbisComment corpus generator
generate_vorbis_corpus_SOURCES = generate_vorbis_corpus.cpp
generate_vorbis_corpus_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# ID3v1 corpus generator
generate_id3v1_corpus_SOURCES = generate_id3v1_corpus.cpp
generate_id3v1_corpus_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(TAGLIB_LIBS) \
	$(AM_LDFLAGS)

# ID3v2 corpus generator
generate_id3v2_corpus_SOURCES = generate_id3v2_corpus.cpp
generate_id3v2_corpus_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Picture corpus generator
generate_picture_corpus_SOURCES = generate_picture_corpus.cpp
generate_picture_corpus_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# ============================================================================
# Utility Tests
# ============================================================================

check_PROGRAMS += test_logarithmic_scale

test_logarithmic_scale_SOURCES = test_logarithmic_scale.cpp
test_logarithmic_scale_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

check_PROGRAMS += test_utility_unit

test_utility_unit_SOURCES = test_utility_unit.cpp
test_utility_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Playlist shuffle test
check_PROGRAMS += test_playlist_shuffle
test_playlist_shuffle_SOURCES = test_playlist_shuffle.cpp
test_playlist_shuffle_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(TAGLIB_LIBS) \
	$(SDL_LIBS) \
	$(AM_LDFLAGS)

# ============================================================================
# UTF8Util Tests
# ============================================================================

check_PROGRAMS += test_utf8util_unit test_utf8util_properties fuzz_utf8util

# UTF8Util unit tests
test_utf8util_unit_SOURCES = test_utf8util_unit.cpp
test_utf8util_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# UTF8Util property-based tests
test_utf8util_properties_SOURCES = test_utf8util_properties.cpp
test_utf8util_properties_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)
if HAVE_RAPIDCHECK
test_utf8util_properties_LDADD += -lrapidcheck
endif

# UTF8Util fuzzing tests
fuzz_utf8util_SOURCES = fuzz_utf8util.cpp
fuzz_utf8util_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)


# Base64 Tests
# ============================================================================

check_PROGRAMS += test_base64_unit

# Base64 unit tests
test_base64_unit_SOURCES = test_base64_unit.cpp
test_base64_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Run all check programs
TESTS = $(check_PROGRAMS)

if HAVE_DBUS
check_PROGRAMS += test_loop_status
test_loop_status_SOURCES = test_loop_status.cpp player_stub.cpp
test_loop_status_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(DBUS_LIBS) \
	$(AM_LDFLAGS)
endif

if HAVE_OGGDEMUXER
check_PROGRAMS += test_issue_ogg_duration
test_issue_ogg_duration_SOURCES = test_issue_ogg_duration.cpp
test_issue_ogg_duration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(VORBIS_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
endif
