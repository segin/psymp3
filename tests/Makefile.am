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
# The vendored SheenBidi headers are needed because several tests compile
# ../src/core/font.cpp directly, and it includes <SheenBidi/SheenBidi.h>. The
# main build carries the same two paths in src/Makefile.am.
AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/third_party/sheenbidi/Headers -I$(top_srcdir)/third_party/sheenbidi/Source $(HARFBUZZ_CFLAGS) $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(OPUS_CFLAGS) $(OGG_CFLAGS) $(SPEEX_CFLAGS) $(AAC_CFLAGS)

if HAVE_DBUS
AM_CPPFLAGS += $(DBUS_CFLAGS)
endif

# Common flags for all test programs

# Define optional codec/demuxer libraries (same as main binary)
CODEC_LIBS =
DEMUXER_LIBS =

# MP3/MP2/ALAC codecs and the MP3 demuxer are always available (bundled
# minimp3, kjmp2, and Apple ALAC — mirrors CODEC_LIBS in src/Makefile.am;
# CodecRegistration.o references all three unconditionally).
CODEC_LIBS += $(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a
CODEC_LIBS += $(top_builddir)/src/codecs/mp2/libpsymp3-codec-mp2.a
CODEC_LIBS += $(top_builddir)/src/codecs/alac/libpsymp3-codec-alac.a
CODEC_LIBS += $(top_builddir)/src/codecs/mlp/libpsymp3-codec-mlp.a
DEMUXER_LIBS += $(top_builddir)/src/demuxer/mp3/libpsymp3-demuxer-mp3.a
DEMUXER_LIBS += $(top_builddir)/src/demuxer/mlp/libpsymp3-demuxer-mlp.a
# Matroska/WebM: no external dependency, so it is always built. Needed by any
# test that links CodecRegistration, which constructs the demuxer.
DEMUXER_LIBS += $(top_builddir)/src/demuxer/matroska/libpsymp3-demuxer-matroska.a
# AC-3: in tree, no external dependency, and referenced by anything that
# links the codec registry. The demuxer parses syncframe headers with the
# codec's parser, so the codec archive comes after it.
DEMUXER_LIBS += $(top_builddir)/src/demuxer/ac3/libpsymp3-demuxer-ac3.a
DEMUXER_LIBS += $(top_builddir)/src/codecs/ac3/libpsymp3-codec-ac3.a
# Repeated after the demuxer: the MLP demuxer decodes the first access unit
# to learn the channel layout, so it needs the decoder, and ld resolves
# archives left to right -- the codec archive has already been passed by the
# time the demuxer's reference to it appears.
DEMUXER_LIBS += $(top_builddir)/src/codecs/mlp/libpsymp3-codec-mlp.a

if HAVE_FLAC
DEMUXER_LIBS += $(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a
if HAVE_NATIVE_FLAC
CODEC_LIBS += $(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a
else
CODEC_LIBS += $(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a
endif
endif

if HAVE_VORBIS
CODEC_LIBS += $(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a
endif

if HAVE_OPUS
CODEC_LIBS += $(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a
endif

if HAVE_AAC
CODEC_LIBS += $(top_builddir)/src/codecs/aac/libpsymp3-codec-aac.a
endif

if HAVE_OGGDEMUXER
DEMUXER_LIBS += $(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a
endif

# System libraries for all tests
SYSTEM_LIBS = \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(Z_LIBS) \
	$(FREETYPE_LIBS) \
	$(DBUS_LIBS) \
	$(OPUS_LIBS) \
	$(OPENSSL_LIBS) \
	$(CURL_LIBS) \
	$(FLAC_LIBS) \
	$(OGG_LIBS) \
	$(AAC_LIBS) \
	$(SPEEX_LIBS)

# Common library set for tests (mirrors main binary linking)
# Order: architecture layers top-down, objects first, then archives
# Note: Libraries that USE symbols must come BEFORE libraries that DEFINE them
# We list libraries multiple times to handle circular dependencies (e.g., Codec <-> Demuxer)
COMMON_TEST_LIBS = \
	$(top_builddir)/src/codecs/mlp/libpsymp3-codec-mlp.a \
	$(top_builddir)/src/demuxer/mlp/libpsymp3-demuxer-mlp.a \
	$(top_builddir)/src/codecs/mlp/libpsymp3-codec-mlp.a \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/dsp/Equalizer.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/playlist.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/core/surface.o \
	$(top_builddir)/src/mediafile.o \
	$(top_builddir)/src/core/display.o \
	$(top_builddir)/src/core/font.o \
	$(top_builddir)/src/core/truetype.o \
	$(top_builddir)/src/mpris/libpsymp3-mpris.a \
	$(top_builddir)/src/discord/libpsymp3-discord.a \
	$(top_builddir)/src/lastfm/libpsymp3-lastfm.a \
	$(top_builddir)/src/widget/windowing/libpsymp3-widget-windowing.a \
	$(top_builddir)/src/widget/ui/libpsymp3-widget-ui.a \
	$(top_builddir)/src/widget/windowing/libpsymp3-widget-windowing.a \
	$(top_builddir)/src/widget/foundation/libpsymp3-widget-foundation.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/demuxer/matroska/libpsymp3-demuxer-matroska.a \
	$(top_builddir)/src/libpsymp3-sheenbidi.a \
	$(HARFBUZZ_LIBS) \
	$(SYSTEM_LIBS)

# Minimal library set for simple tests (I/O, demuxer core, codecs)
# Note: Libraries with circular dependencies must be listed multiple times
# Order: high-level -> mid-level -> low-level, then repeat for circular deps
# Note: Tag library is required because demuxers now depend on it for metadata extraction
MINIMAL_TEST_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a

# Common library set for Ogg demuxer-focused tests
OGG_DEMUXER_TEST_LIBS = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

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
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/mp2/libpsymp3-codec-mp2.a \
	$(top_builddir)/src/codecs/alac/libpsymp3-codec-alac.a \
	$(top_builddir)/src/codecs/aac/libpsymp3-codec-aac.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/codecs/pcm/libpsymp3-codec-pcm.a \
	$(top_builddir)/src/codecs/opus/libpsymp3-codec-opus.a \
	$(top_builddir)/src/codecs/vorbis/libpsymp3-codec-vorbis.a \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/mp2/libpsymp3-codec-mp2.a \
	$(top_builddir)/src/codecs/alac/libpsymp3-codec-alac.a \
	$(top_builddir)/src/codecs/aac/libpsymp3-codec-aac.a \
	$(top_builddir)/src/codecs/flac/libpsymp3-codec-flac.a \
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(DEMUXER_LIBS) \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(SYSTEM_LIBS)

# Legacy variable for backward compatibility (deprecated - use FULL_CODEC_LIBS)
COMMON_CODEC_DEPS = $(FULL_CODEC_LIBS)

# MPRIS testing libraries
MPRIS_TEST_LIBS = $(COMMON_TEST_LIBS)

# Common library links for codec tests
COMMON_CODEC_LIBS = -lopus -logg
AM_LDFLAGS = $(SDL_LIBS) $(TAGLIB_LIBS) $(FREETYPE_LIBS) $(DBUS_LIBS) $(OPUS_LIBS) $(OPENSSL_LIBS) $(CURL_LIBS) $(WINSOCK_LIBS) $(SPEEX_LIBS) $(AAC_LIBS)

# Common test utilities library
noinst_LIBRARIES = libtest_utilities.a
libtest_utilities_a_SOURCES = test_framework.cpp test_rect_utilities.cpp test_framework_threading.cpp
if HAVE_DBUS
libtest_utilities_a_SOURCES += mock_dbus_connection.cpp mock_player.cpp
endif

# Test harness executable and debug programs
if BUILD_TEST_HARNESS
EXTRA_PROGRAMS = test_streaminfo_debug test-harness debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_seek_deadlock test_ogg_codec_detection test_error_handling test_mulaw_codec_properties test_mulaw_alaw_error_state_consistency test_lastfm_url_encoding_properties test_lastfm_exponential_backoff_properties test_lastfm_lazy_logging_properties test_lastfm_scrobble_cache_roundtrip_properties test_lastfm_config_backward_compatibility test_lastfm_config_roundtrip_properties test_lastfm_thread_safety_properties test_flac_stream_marker_properties test_flac_stream_marker_with_logging test_flac_metadata_block_header_properties test_flac_streaminfo_block_size_properties test_flac_seektable_placeholder_properties test_flac_vorbis_comment_endianness_properties test_flac_vorbis_field_name_properties test_flac_cuesheet_track_count_properties test_flac_frame_sync_properties test_flac_block_size_bits_properties test_flac_sample_rate_bits_properties test_flac_crc8_properties test_flac_crc16_properties test_flac_frame_size_estimation_properties test_flac_frame_boundary_search_properties test_flac_duration_calculation_properties test_flac_sync_resync_properties test_flac_thread_safety_lock_pattern_properties test_flac_debug_logging_format_properties
test_harness_SOURCES = test_harness.cpp test_discovery.cpp test_executor.cpp test_reporter.cpp
test_harness_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)
else
EXTRA_PROGRAMS = test_streaminfo_debug debug_dual_flac debug_bit_extraction debug_flac_seeking test_iohandler_seek_comprehensive test_iohandler_buffer_analysis test_debug_iohandler_position test_seek_deadlock test_frame_size_estimation_consistency test_ogg_codec_detection test_error_handling test_mulaw_codec_properties test_mulaw_alaw_error_state_consistency test_lastfm_url_encoding_properties test_lastfm_exponential_backoff_properties test_lastfm_lazy_logging_properties test_lastfm_scrobble_cache_roundtrip_properties test_lastfm_config_backward_compatibility test_lastfm_config_roundtrip_properties test_lastfm_thread_safety_properties test_flac_stream_marker_properties test_flac_stream_marker_with_logging test_flac_metadata_block_header_properties test_flac_streaminfo_block_size_properties test_flac_seektable_placeholder_properties test_flac_vorbis_comment_endianness_properties test_flac_vorbis_field_name_properties test_flac_cuesheet_track_count_properties test_flac_frame_sync_properties test_flac_block_size_bits_properties test_flac_crc8_properties test_flac_crc16_properties test_flac_frame_size_estimation_properties test_flac_frame_boundary_search_properties test_flac_duration_calculation_properties test_flac_sync_resync_properties test_flac_thread_safety_lock_pattern_properties test_flac_debug_logging_format_properties
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
EXTRA_PROGRAMS += opusout

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
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OPUS_LIBS) $(OGG_LIBS) \
	-lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(FLAC_LIBS) $(TAGLIB_LIBS) $(AAC_LIBS)
endif

debug_dual_flac_SOURCES = debug_dual_flac.cpp
debug_dual_flac_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Memory Management Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_memory_suite_SOURCES = test_memory_suite.cpp \
	core/memory/test_memory_leak_prevention.cpp \
	core/memory/test_memory_optimizer.cpp \
	core/memory/test_memory_pool_allocation_failure.cpp \
	core/memory/test_memory_pool_manager_basic_threading.cpp \
	core/memory/test_memory_pool_manager_integration.cpp \
	core/memory/test_memory_pool_manager_thread_safety_comprehensive.cpp \
	core/memory/test_memory_tracker_unit.cpp
test_memory_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

debug_bit_extraction_SOURCES = debug_bit_extraction.cpp
debug_bit_extraction_LDADD = $(AM_LDFLAGS)

debug_flac_seeking_SOURCES = debug_flac_seeking.cpp
debug_flac_seeking_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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

# Individual test executables
check_PROGRAMS = test_mediafile_suite test_rect_suite \
	test_harness_integration \
	test_iohandler_suite \
	test_integration_verification \
	test_psymp3_system_integration \
	test_api_consistency \
	test_seeking_engine \
	test_demuxer_suite \
	test_widget_hierarchy_properties \
	test_widget_suite \
	test_matroska_suite \
	test_ac3_suite \
	test_ac3_container_suite \
	test_mp2_codec \
	test_pcm_codec \
	test_demuxed_stream_seek \
	test_http_suite \
	test_http_client_fallback \
	test_http_io_handler \
	test_core_utils_suite \
	test_io_buffer_pool \
	test_enhanced_buffer_pool \
	test_surface_suite \
	test_extensibility_utils \
	test_lastfm_url_encode_unit \
	test_verify_random

if HAVE_AAC
check_PROGRAMS += test_aac_suite 
# AAC Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_aac_suite_SOURCES = test_aac_suite.cpp \
	codecs/aac/test_aac_integration.cpp \
	codecs/aac/test_aac_output_format.cpp
test_aac_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# HE-AAC streams made with FDK's encoder, which is in the decoder's library.
check_PROGRAMS += test_iso_seek
test_iso_seek_SOURCES = test_iso_seek.cpp
test_iso_seek_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)
endif

if HAVE_FLAC
# FLAC Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_flac_suite_SOURCES = test_flac_suite.cpp \
	codecs/flac/test_flac_audio_output.cpp \
	codecs/flac/test_flac_audio_output_debug.cpp \
	codecs/flac/test_flac_backward_compatibility_validation.cpp \
	codecs/flac/test_flac_binary_search_limitations.cpp \
	codecs/flac/test_flac_bisection_real_files.cpp \
	codecs/flac/test_flac_bit_depth_validation_simple.cpp \
	codecs/flac/test_flac_codec_compatibility.cpp \
	codecs/flac/test_flac_codec_container_agnostic.cpp \
	codecs/flac/test_flac_codec_demuxer_integration.cpp \
	codecs/flac/test_flac_codec_error_handling.cpp \
	codecs/flac/test_flac_codec_integration.cpp \
	codecs/flac/test_flac_codec_minimal_container_agnostic.cpp \
	codecs/flac/test_flac_codec_multithreaded.cpp \
	codecs/flac/test_flac_codec_performance_comprehensive.cpp \
	codecs/flac/test_flac_codec_simple_debug.cpp \
	codecs/flac/test_flac_codec_unit_comprehensive.cpp \
	codecs/flac/test_flac_codec_unit_minimal.cpp \
	codecs/flac/test_flac_compatibility_integration.cpp \
	codecs/flac/test_flac_comprehensive_validation.cpp \
	codecs/flac/test_flac_conditional_compilation.cpp \
	codecs/flac/test_flac_crc_validation.cpp \
	codecs/flac/test_flac_demuxer_compatibility.cpp \
	codecs/flac/test_flac_demuxer_integration_comprehensive.cpp \
	codecs/flac/test_flac_demuxer_performance.cpp \
	codecs/flac/test_flac_demuxer_real_files.cpp \
	codecs/flac/test_flac_demuxer_rfc9639_regressions.cpp \
	codecs/flac/test_flac_demuxer_simple.cpp \
	codecs/flac/test_flac_demuxer_thread_safety.cpp \
	codecs/flac/test_flac_demuxer_threading.cpp \
	codecs/flac/test_flac_demuxer_unit_comprehensive.cpp \
	codecs/flac/test_flac_demuxer_unit_fixed.cpp \
	codecs/flac/test_flac_diagnostic.cpp \
	codecs/flac/test_flac_index_based_seeking.cpp \
	codecs/flac/test_flac_mini_player.cpp \
	codecs/flac/test_flac_performance_benchmarking.cpp \
	codecs/flac/test_flac_performance_optimization.cpp \
	codecs/flac/test_flac_performance_with_real_files.cpp \
	codecs/flac/test_flac_quality_simple.cpp \
	codecs/flac/test_flac_real_file.cpp \
	codecs/flac/test_flac_rfc9639_error_handling.cpp \
	codecs/flac/test_flac_rfc_bit_depth_validation.cpp \
	codecs/flac/test_flac_rfc_block_size_sample_rate_validation.cpp \
	codecs/flac/test_flac_rfc_compliance.cpp \
	codecs/flac/test_flac_rfc_entropy_coding_validation.cpp \
	codecs/flac/test_flac_rfc_subframe_validation.cpp \
	codecs/flac/test_flac_sample_format_rfc9639_compliance.cpp \
	codecs/flac/test_flac_security_validation.cpp \
	codecs/flac/test_flac_seek_landing.cpp \
	codecs/flac/test_flac_seeking_crash.cpp \
	codecs/flac/test_flac_test_data_validation.cpp \
	codecs/flac/test_flac_thread_safety_validation.cpp \
	codecs/flac/test_flac_variable_block_size.cpp
test_flac_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

endif

# AVI files, which ChunkDemuxer reads as the RIFF form they are.
check_PROGRAMS += test_avi_demuxer
test_avi_demuxer_SOURCES = test_avi_demuxer.cpp
test_avi_demuxer_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_extensibility_utils_SOURCES = test_extensibility_utils.cpp $(top_srcdir)/src/demuxer/DemuxerExtensibility.cpp
test_extensibility_utils_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Widget and Rendering Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_widget_suite_SOURCES = test_widget_suite.cpp \
	widget/test_widget_rendering.cpp \
	widget/test_complex_script_rendering.cpp \
	widget/test_widget_event_routing.cpp \
	widget/test_list_view_selection.cpp
test_widget_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)
# The bundled font and the script corpus live in the source tree, not
# beside the test binary.
test_widget_suite_CPPFLAGS = $(AM_CPPFLAGS) \
	-DPSYMP3_TOP_SRCDIR='"$(top_srcdir)"'

# The bundled font lives in the source tree, not beside the test binary.
# The corpus is rendered with the bundled font, which lives in the source tree
# rather than beside the test binary.
# The Matroska tests share one program, as the AC-3 ones do: each keeps its
# file and its suite, and they no longer link the static stack six times over.
test_matroska_suite_SOURCES = test_matroska_suite.cpp \
	demuxer/matroska/test_ebml_reader.cpp \
	demuxer/matroska/test_matroska_segment.cpp \
	demuxer/matroska/test_matroska_block.cpp \
	demuxer/matroska/test_matroska_cues.cpp \
	demuxer/matroska/test_matroska_seek.cpp \
	demuxer/matroska/test_matroska_tags.cpp
test_matroska_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# The AC-3 and E-AC-3 unit tests share one program. Each still has its own
# file and its own suite; what they no longer have is a link apiece, and each
# of these links the whole static stack for a test that runs in milliseconds.
test_ac3_suite_SOURCES = test_ac3_suite.cpp \
	codecs/ac3/test_ac3_frame_header.cpp \
	codecs/ac3/test_ac3_exponents.cpp \
	codecs/ac3/test_ac3_bit_allocation.cpp \
	codecs/ac3/test_ac3_mantissas.cpp \
	codecs/ac3/test_ac3_transform.cpp \
	codecs/ac3/test_eac3_frame.cpp \
	codecs/ac3/test_eac3_spx.cpp \
	codecs/ac3/test_eac3_tpnp.cpp \
	codecs/ac3/test_eac3_aht.cpp \
	codecs/ac3/test_eac3_ecpl.cpp \
	codecs/ac3/test_ac3_drc.cpp \
	codecs/ac3/test_ac3_downmix.cpp
test_ac3_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Decodes the broadband oracle streams of generate_ac3_corpus.sh through
# DemuxedStream and compares band energies with ffmpeg's decode; skips (77)
# when the corpus has not been generated.
# DemuxedStream keeps its buffered audio when a demuxer refuses a seek.
test_demuxed_stream_seek_SOURCES = test_demuxed_stream_seek.cpp
test_demuxed_stream_seek_LDADD = $(FULL_CODEC_LIBS) $(AM_LDFLAGS)

# MP2Codec's channel handling (kjmp2 always decodes to stereo).
test_mp2_codec_SOURCES = test_mp2_codec.cpp
test_mp2_codec_LDADD = $(FULL_CODEC_LIBS) $(AM_LDFLAGS)
test_pcm_codec_SOURCES = test_pcm_codec.cpp
test_pcm_codec_LDADD = $(FULL_CODEC_LIBS) $(AM_LDFLAGS)

# AC-3 Container and Codec Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_ac3_container_suite_SOURCES = test_ac3_container_suite.cpp \
	demuxer/ac3/test_ac3_codec.cpp \
	demuxer/ac3/test_ac3_null_demuxer.cpp \
	demuxer/ac3/test_ac3_oracle.cpp
test_ac3_container_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# FULL_CODEC_LIBS names the AC-3 codec archive after the codec base it needs,
# so it is named once more ahead of them.
generate_matroska_corpus_SOURCES = generate_matroska_corpus.cpp
generate_matroska_corpus_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# OggDemuxer tests (conditional on HAVE_OGGDEMUXER)
if HAVE_OGGDEMUXER
check_PROGRAMS += test_ogg_suite \
	test_compression_lz77

if HAVE_RAPIDCHECK
check_PROGRAMS += test_ogg_fuzzing
endif

test_compression_lz77_SOURCES = test_compression_lz77.cpp
test_compression_lz77_LDADD = \
	$(top_builddir)/src/core/compression/libpsymp3-core-compression.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(LDADD)
endif

check_PROGRAMS += test_path_security
test_path_security_SOURCES = test_path_security.cpp
test_path_security_LDADD = \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(OPENSSL_LIBS) \
	$(CURL_LIBS) \
	$(AM_LDFLAGS)

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
	$(FLAC_LIBS) \
	$(FLAC_LIBS) \
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

if HAVE_G722
check_PROGRAMS += test_g722_suite 
endif

check_PROGRAMS += test_raw_audio_stream_integration

# G711 codec tests (both A-law and μ-law)
if HAVE_G711
  # A-law tests
check_PROGRAMS += test_g711_suite test_wav_g711 test_wav_g711_improved
check_PROGRAMS += test_raw_audio_tag_safety
  # WaveStream test
  check_PROGRAMS += test_wav_stream
  # μ-law tests
  # Combined codec selection tests
  # Performance tests for G711
  # Integration tests for G711
endif

# Codec selection and validation tests

# Performance and thread safety tests
check_PROGRAMS += test_codec_suite test_system_wide_threading_integration test_memory_suite 

# SimplePCMCodec integration tests
check_PROGRAMS += test_simplepcm_suite 

# Conditional compilation tests
check_PROGRAMS += test_conditional_codec_compilation

# MPRIS tests (conditional on HAVE_DBUS)
if HAVE_DBUS
check_PROGRAMS += test_dbus_suite test_method_handler_suite test_mock_framework_standalone test_mpris_suite 
endif

# FLAC demuxer compatibility tests (conditional on HAVE_FLAC)
if HAVE_FLAC
check_PROGRAMS += test_streaminfo_suite \
	test_iohandler_deadlock \
	test_entropy_coding_compilation \
	test_flac_suite \
	test_flac_parser_vulnerability
endif

# Test program sources and dependencies
# MediaFile Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_mediafile_suite_SOURCES = test_mediafile_suite.cpp \
	core/mediafile/test_mediafile.cpp \
	core/mediafile/test_mediafile_split.cpp
test_mediafile_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Rect Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_rect_suite_SOURCES = test_rect_suite.cpp \
	core/rect/test_rect_area_validation.cpp \
	core/rect/test_rect_backward_compatibility.cpp \
	core/rect/test_rect_centering.cpp \
	core/rect/test_rect_centering_overflow.cpp \
	core/rect/test_rect_containment.cpp \
	core/rect/test_rect_expansion.cpp \
	core/rect/test_rect_integration.cpp \
	core/rect/test_rect_intersection.cpp \
	core/rect/test_rect_modern_cpp.cpp \
	core/rect/test_rect_normalization.cpp \
	core/rect/test_rect_pair_variants.cpp \
	core/rect/test_rect_performance.cpp \
	core/rect/test_rect_transformation.cpp \
	core/rect/test_rect_union.cpp \
	core/rect/test_rect_unit.cpp \
	core/rect/test_widget_rect_integration.cpp
test_rect_suite_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)

test_widget_hierarchy_properties_SOURCES = test_widget_hierarchy_properties.cpp
test_widget_hierarchy_properties_LDADD = $(COMMON_TEST_LIBS) $(RAPIDCHECK_LIBS) $(AM_LDFLAGS)

test_harness_integration_SOURCES = test_harness_integration.cpp
test_harness_integration_LDADD = libtest_utilities.a $(top_builddir)/src/core/libpsymp3-core.a $(AM_LDFLAGS)
# Whether there is a harness to test is compiled in, not read from the
# directory. A ./test-harness left behind by an earlier harness-enabled
# configuration used to make this test run, and fail, in a tree that never
# asked for it. When the harness is configured it is built first, so that
# this test also works when built and run on its own.
# CPPFLAGS is set in both branches so that AM_CPPFLAGS survives in either.
if BUILD_TEST_HARNESS
test_harness_integration_CPPFLAGS = $(AM_CPPFLAGS) -DPSYMP3_TEST_HARNESS_BUILT=1
EXTRA_test_harness_integration_DEPENDENCIES = test-harness$(EXEEXT)
else
test_harness_integration_CPPFLAGS = $(AM_CPPFLAGS)
endif

	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# IOHandler Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_iohandler_suite_SOURCES = test_iohandler_suite.cpp \
	io/test_iohandler_basic.cpp \
	io/test_iohandler_demuxer_integration.cpp \
	io/test_iohandler_demuxer_integration_simple.cpp \
	io/test_iohandler_integration_simple.cpp \
	io/test_iohandler_legacy_compatibility.cpp \
	io/test_iohandler_memory_deadlock_prevention.cpp \
	io/test_iohandler_minimal.cpp \
	io/test_iohandler_performance_validation.cpp \
	io/test_iohandler_thread_safety_comprehensive.cpp
test_iohandler_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_integration_verification_SOURCES = test_integration_verification.cpp
test_integration_verification_LDADD = libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

test_psymp3_system_integration_SOURCES = test_psymp3_system_integration.cpp
test_psymp3_system_integration_LDADD = libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(AM_LDFLAGS)

test_api_consistency_SOURCES = test_api_consistency.cpp
test_api_consistency_LDADD = libtest_utilities.a $(AM_LDFLAGS)

# Demuxer architecture unit tests
# Demuxer Tag and Unit Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_demuxer_extra_suite_SOURCES = test_demuxer_extra_suite.cpp \
	demuxer/test_demuxer_tag_extraction.cpp \
	demuxer/test_demuxer_unit.cpp
test_demuxer_extra_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_seeking_engine_SOURCES = test_seeking_engine.cpp
test_seeking_engine_LDADD = libtest_utilities.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Demuxer and MediaFactory Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_demuxer_suite_SOURCES = test_demuxer_suite.cpp \
	demuxer/test_demuxer_factory_unit.cpp \
	demuxer/test_media_factory_unit.cpp \
	demuxer/test_demuxed_stream_unit.cpp \
	demuxer/test_demuxer_integration.cpp \
	demuxer/test_media_factory_integration.cpp \
	demuxer/test_demuxer_performance.cpp
test_demuxer_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Integration tests
# Performance and regression tests
# IOHandler backward compatibility and performance validation tests
# HTTP Client Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_http_suite_SOURCES = test_http_suite.cpp \
	io/http/test_http_client_parse_url.cpp \
	io/http/test_http_iohandler_validation.cpp
test_http_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# These two stay programs of their own, and cannot join test_http_suite:
# each replaces HTTPClient in the link, and they do it differently.
# test_http_client_fallback compiles src/io/http/HTTPClient.cpp itself
# with HTTP_CLIENT_NO_CURL defined, to exercise the path taken when
# curl is absent; test_http_io_handler defines stub HTTPClient methods
# so that HTTPIOHandler can be tested without a server. One program
# cannot hold both sets of definitions. Merging them would mean giving
# HTTPIOHandler an injectable client instead of mocking it at link
# time, which is a change to the code under test, not to its tests.
test_http_client_fallback_SOURCES = test_http_client_fallback.cpp
test_http_client_fallback_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Bounded Buffer, Queue and Threading Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_core_utils_suite_SOURCES = test_core_utils_suite.cpp \
	core/utils/test_bounded_buffer.cpp \
	core/utils/test_bounded_queue.cpp \
	core/utils/test_threading_performance_regression.cpp \
	core/utils/test_threading_safety_baseline.cpp
test_core_utils_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_io_buffer_pool_SOURCES = test_io_buffer_pool.cpp
test_io_buffer_pool_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Enhanced Buffer Pool Test
test_enhanced_buffer_pool_SOURCES = test_enhanced_buffer_pool.cpp
test_enhanced_buffer_pool_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Surface Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_surface_suite_SOURCES = test_surface_suite.cpp \
	core/surface/test_surface_from_bmp_error.cpp \
	core/surface/test_surface_performance_regression.cpp \
	core/surface/test_surface_thread_safety.cpp
test_surface_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# OggDemuxer page extraction test
if HAVE_OGGDEMUXER
# Ogg Demuxer Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_ogg_suite_SOURCES = test_ogg_suite.cpp \
	demuxer/ogg/test_ogg_page_extraction.cpp \
	demuxer/ogg/test_ogg_duration_calculation.cpp \
	demuxer/ogg/test_ogg_time_conversion.cpp \
	demuxer/ogg/test_ogg_data_streaming.cpp \
	demuxer/ogg/test_ogg_memory_safety.cpp \
	demuxer/ogg/test_oggdemuxer_performance.cpp \
	demuxer/ogg/test_oggdemuxer_comprehensive.cpp \
	demuxer/ogg/test_ogg_comprehensive_integration.cpp \
	demuxer/ogg/test_ogg_multiplexed_seeking.cpp \
	demuxer/ogg/test_ogg_seeking_stress.cpp
test_ogg_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_ogg_fuzzing_SOURCES = test_ogg_fuzzing.cpp
test_ogg_fuzzing_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	../src/io/file/libpsymp3-io-file.a \
	../src/io/libpsymp3-io.a \
	../src/debug.o \
	../src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(RAPIDCHECK_LIBS) \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

# Ogg Demuxer Behaviour Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_ogg_extra_suite_SOURCES = test_ogg_extra_suite.cpp \
	demuxer/ogg/extra/test_ogg_aggressive.cpp \
	demuxer/ogg/extra/test_ogg_error_handling.cpp \
	demuxer/ogg/extra/test_ogg_integration.cpp \
	demuxer/ogg/extra/test_ogg_seeking_algorithms.cpp
test_ogg_extra_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

check_PROGRAMS += test_stream_manager test_ogg_extra_suite 
test_stream_manager_SOURCES = test_stream_manager.cpp
test_stream_manager_LDADD = libtest_utilities.a \
	../src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	../src/demuxer/libpsymp3-demuxer.a \
	../src/demuxer/raw/libpsymp3-demuxer-raw.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	../src/demuxer/raw/libpsymp3-demuxer-raw.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	../src/demuxer/raw/libpsymp3-demuxer-raw.a \
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
	../src/demuxer/raw/libpsymp3-demuxer-raw.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	../src/demuxer/raw/libpsymp3-demuxer-raw.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	../src/demuxer/raw/libpsymp3-demuxer-raw.a \
	../src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	../src/demuxer/raw/libpsymp3-demuxer-raw.a \
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

# OggDemuxer codec detection test
test_ogg_codec_detection_SOURCES = test_ogg_codec_detection.cpp
test_ogg_codec_detection_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) $(OPUS_LIBS) $(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer duration calculation test
# OggDemuxer time conversion test
# OggDemuxer data streaming test
# OggDemuxer memory safety test
# OggDemuxer error handling test
test_error_handling_SOURCES = test_error_handling.cpp
test_error_handling_LDADD = $(OGG_DEMUXER_TEST_LIBS)

# OggDemuxer seeking integration test
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
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
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

# OggDemuxer integration test
# Runtime fixture encoding needs libvorbisenc — a TEST-ONLY dependency (the
# player decodes with the bundled stb_vorbis). Skipped when not installed.
if HAVE_VORBISENC_TESTS
check_PROGRAMS += test_ogg_demuxer_integration
test_ogg_demuxer_integration_SOURCES = test_ogg_demuxer_integration.cpp
test_ogg_demuxer_integration_CPPFLAGS = $(AM_CPPFLAGS) $(VORBISENC_CFLAGS)
# The decode-pinning test drives VorbisCodec, whose object also carries the
# Vorbis Stream wrapper - so this links the full stream/codec stack like the
# codec tests do. libvorbis/-lvorbisenc are the TEST-ONLY fixture encoder.
test_ogg_demuxer_integration_LDADD = \
	libtest_utilities.a \
	$(FULL_CODEC_LIBS) \
	$(OGG_LIBS) \
	$(VORBISENC_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

check_PROGRAMS += test_vorbis_codec_private
test_vorbis_codec_private_SOURCES = test_vorbis_codec_private.cpp
test_vorbis_codec_private_CPPFLAGS = $(AM_CPPFLAGS) $(VORBISENC_CFLAGS)
test_vorbis_codec_private_LDADD = \
	libtest_utilities.a \
	$(FULL_CODEC_LIBS) \
	$(OGG_LIBS) \
	$(VORBISENC_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)

check_PROGRAMS += test_ogg_seek_landing
test_ogg_seek_landing_SOURCES = test_ogg_seek_landing.cpp
test_ogg_seek_landing_CPPFLAGS = $(AM_CPPFLAGS) $(VORBISENC_CFLAGS)
test_ogg_seek_landing_LDADD = \
	libtest_utilities.a \
	$(FULL_CODEC_LIBS) \
	$(OGG_LIBS) \
	$(VORBISENC_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
endif
endif

# A-law codec table verification test
# G.711 Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_g711_suite_SOURCES = test_g711_suite.cpp \
	codecs/g711/test_alaw_codec_properties.cpp \
	codecs/g711/test_alaw_conversion_accuracy.cpp \
	codecs/g711/test_alaw_sample_conversion.cpp \
	codecs/g711/test_alaw_table_verification.cpp \
	codecs/g711/test_mulaw_alaw_codec_selection.cpp \
	codecs/g711/test_mulaw_alaw_error_handling.cpp \
	codecs/g711/test_mulaw_alaw_integration.cpp \
	codecs/g711/test_mulaw_alaw_performance.cpp \
	codecs/g711/test_mulaw_conversion_accuracy.cpp
# The union of what these linked apiece: the codec archives for the A-law
# and mu-law codecs themselves, and the utilities the rest use.
test_g711_suite_LDADD = libtest_utilities.a $(FULL_CODEC_LIBS) $(AM_LDFLAGS) $(COMMON_CODEC_LIBS)

# Playlist and Track Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_playlist_suite_SOURCES = test_playlist_suite.cpp \
	core/playlist/test_playlist_block_edits.cpp \
	core/playlist/test_playlist_load.cpp \
	core/playlist/test_playlist_shuffle.cpp \
	core/playlist/test_track_utils.cpp
test_playlist_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_verify_random_SOURCES = verify_random.cpp

test_raw_audio_tag_safety_SOURCES = test_raw_audio_tag_safety.cpp
test_raw_audio_tag_safety_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(AM_LDFLAGS)

# G.722 Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_g722_suite_SOURCES = test_g722_suite.cpp \
	codecs/g722/test_g722_conformance.cpp \
	codecs/g722/test_g722_integration.cpp \
	codecs/g722/test_g722_wav.cpp
test_g722_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# ITU-T G.722 Appendix II sequences, fetched by fetch_g722_testvectors.sh;
# skips (77) without them.
test_raw_audio_stream_integration_SOURCES = test_raw_audio_stream_integration.cpp
test_raw_audio_stream_integration_LDADD = \
	$(COMMON_TEST_LIBS) \
	$(AM_LDFLAGS)

# A-law codec sample conversion test
# A-law codec conversion accuracy test
# μ-law codec conversion accuracy test
# WAV G.711 conversion test
test_wav_g711_SOURCES = test_wav_g711.cpp
test_wav_g711_LDADD = libtest_utilities.a $(top_builddir)/src/stream.o $(top_builddir)/src/track.o $(top_builddir)/src/debug.o $(top_builddir)/src/io/file/libpsymp3-io-file.a $(top_builddir)/src/io/libpsymp3-io.a $(top_builddir)/src/tag/libpsymp3-tag.a $(top_builddir)/src/core/libpsymp3-core.a $(SDL_LIBS) $(TAGLIB_LIBS) $(Z_LIBS) $(FREETYPE_LIBS) $(DBUS_LIBS) $(OPUS_LIBS) $(OPENSSL_LIBS) $(CURL_LIBS) $(SPEEX_LIBS) $(AM_LDFLAGS) -latomic

# WaveStream test
test_wav_stream_SOURCES = test_wav_stream.cpp
test_wav_stream_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/demuxer/riff/libpsymp3-demuxer-riff.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Improved WAV G.711 conversion test
test_wav_g711_improved_SOURCES = test_wav_g711_improved.cpp
test_wav_g711_improved_LDADD = libtest_utilities.a $(top_builddir)/src/stream.o $(top_builddir)/src/track.o $(top_builddir)/src/debug.o $(top_builddir)/src/io/file/libpsymp3-io-file.a $(top_builddir)/src/io/libpsymp3-io.a $(top_builddir)/src/tag/libpsymp3-tag.a $(top_builddir)/src/core/libpsymp3-core.a $(SDL_LIBS) $(TAGLIB_LIBS) $(Z_LIBS) $(FREETYPE_LIBS) $(DBUS_LIBS) $(OPUS_LIBS) $(OPENSSL_LIBS) $(CURL_LIBS) $(SPEEX_LIBS) $(AM_LDFLAGS) -latomic

# Codec selection and validation test (simple)
# Codec Framework Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_codec_suite_SOURCES = test_codec_suite.cpp \
	codecs/test_codec_concurrent_instances.cpp \
	codecs/test_codec_performance.cpp \
	codecs/test_codec_performance_simple.cpp \
	codecs/test_codec_selection_validation_simple.cpp \
	codecs/test_codec_thread_safety.cpp \
	codecs/test_codec_thread_safety_simple.cpp
test_codec_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Codec performance test
# μ-law/A-law codec performance test
# Codec thread safety test
# Codec concurrent instances test
# MPRIS types test (conditional on HAVE_DBUS)
if HAVE_DBUS
# MPRIS Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
# player_stub.cpp stands in for the player the MPRIS code calls into;
# eleven of these tests compiled it in, and one program needs it once.
test_mpris_suite_SOURCES = test_mpris_suite.cpp player_stub.cpp \
	mpris/test_mpris_concurrent_clients.cpp \
	mpris/test_mpris_error_handling.cpp \
	mpris/test_mpris_error_integration.cpp \
	mpris/test_mpris_logger_basic.cpp \
	mpris/test_mpris_logger_dbus_tracing.cpp \
	mpris/test_mpris_logger_performance.cpp \
	mpris/test_mpris_manager_basic_integration.cpp \
	mpris/test_mpris_manager_comprehensive.cpp \
	mpris/test_mpris_manager_integration.cpp \
	mpris/test_mpris_manager_minimal.cpp \
	mpris/test_mpris_manager_simple.cpp \
	mpris/test_mpris_memory_validation.cpp \
	mpris/test_mpris_mock_framework.cpp \
	mpris/test_mpris_performance_profiler.cpp \
	mpris/test_mpris_reconnection_behavior.cpp \
	mpris/test_mpris_regression_validation.cpp \
	mpris/test_mpris_spec_compliance.cpp \
	mpris/test_mpris_stress_testing.cpp \
	mpris/test_mpris_types.cpp
# libtest_utilities.a again at the end: the MPRIS archive's references to
# Player are satisfied by mock_player.o, and a static archive only
# supplies what is still wanted when the linker reaches it.
test_mpris_suite_LDADD = $(MPRIS_TEST_LIBS) libtest_utilities.a $(AM_LDFLAGS)

# D-Bus Connection, Property and Signal Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
# player_stub.cpp: the property manager reads the player's state.
test_dbus_suite_SOURCES = test_dbus_suite.cpp player_stub.cpp \
	mpris/dbus/test_dbus_connection_manager.cpp \
	mpris/dbus/test_dbus_connection_manager_comprehensive.cpp \
	mpris/dbus/test_property_manager.cpp \
	mpris/dbus/test_property_manager_comprehensive.cpp \
	mpris/dbus/test_signal_emitter.cpp \
	mpris/dbus/test_signal_emitter_comprehensive.cpp
test_dbus_suite_LDADD = $(MPRIS_TEST_LIBS) libtest_utilities.a $(AM_LDFLAGS)

# MPRIS Method Handler Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
# player_stub.cpp for the same reason the MPRIS suite compiles it: the
# method handler calls into the player.
test_method_handler_suite_SOURCES = test_method_handler_suite.cpp player_stub.cpp \
	mpris/method/test_method_handler.cpp \
	mpris/method/test_method_handler_comprehensive.cpp \
	mpris/method/test_method_handler_simple.cpp
test_method_handler_suite_LDADD = $(MPRIS_TEST_LIBS) libtest_utilities.a $(AM_LDFLAGS)

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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) -lopus \
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) -lopus \
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/io/TagLibIOHandlerAdapter.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(OGG_LIBS) -lopus \
	$(AM_LDFLAGS)
endif

# test-harness is an EXTRA_PROGRAMS (so it is not built by `make all` / `make
# install`); list it as a check-local prerequisite via this conditional variable
# so `make check` still builds it before running.
if BUILD_TEST_HARNESS
CHECK_LOCAL_DEPS = test-harness
endif

# Test execution target. With the harness configured, it runs the suite: make
# hands it exactly $(check_PROGRAMS), so a program need not be named test_*,
# and a stale binary that is no longer (or never was) a check program is not
# run. The sources are read from $(srcdir) for their annotations, which in an
# out-of-tree build such as distcheck's is not where the binaries are. The
# output is kept in test-harness.log as well, since CI keeps the *.log files.
# Without the harness, automake's check-TESTS has already run TESTS, treating
# exit 77 as a skip, so there is nothing left to do here.
check-local: $(check_PROGRAMS) $(CHECK_LOCAL_DEPS)
if BUILD_TEST_HARNESS
	@rm -f check-programs.list test-harness.status
	@for program in $(check_PROGRAMS); do echo "$$program"; done > check-programs.list
	@{ ./test-harness$(EXEEXT) --verbose --source-dir $(srcdir) \
	      --programs-file check-programs.list; \
	   echo $$? > test-harness.status; } 2>&1 | tee test-harness.log
	@status=`cat test-harness.status`; rm -f test-harness.status; exit $$status
endif

# Clean up test executables. test-harness is listed whatever the configuration,
# so that `make clean` also removes one left by an earlier harness-enabled one.
CLEANFILES = $(check_PROGRAMS) test-harness$(EXEEXT) \
	check-programs.list test-harness.log test-harness.status

# Runtime debug logs some tests write into the build directory; distcheck's
# distcleancheck fails on anything left behind after distclean.
CLEANFILES += \
	performance_test.log \
	flacout_debug.log \
	mp3out_debug.log \
	opusout_debug.log \
	vorbisout_debug.log

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

$(top_builddir)/src/dsp/Equalizer.o:
	$(MAKE) -C $(top_builddir)/src dsp/Equalizer.o

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
$(top_builddir)/src/core/surface.o:
	$(MAKE) -C $(top_builddir)/src/core surface.o

# Build rules for ISO demuxer test dependencies

# System-wide threading integration test
test_system_wide_threading_integration_SOURCES = test_system_wide_threading_integration.cpp
test_system_wide_threading_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/audio.o \
	$(top_builddir)/src/dsp/Equalizer.o \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/core/surface.o \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# Threading performance regression test
# Simple codec performance test (minimal dependencies)
# Simple codec thread safety test (minimal dependencies)
# Threading safety baseline test
# Audio thread safety test
# Audio Pipeline Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_audio_suite_SOURCES = test_audio_suite.cpp \
	audio/test_audio_buffer_cursor.cpp \
	audio/test_audio_frame_assembly.cpp \
	audio/test_audio_thread_safety.cpp \
	audio/test_audio_threading_pattern.cpp
test_audio_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Audio's PCM queue: read cursor, compaction, and continuity through the
# real SDL callback and decoder thread (SDL dummy driver, no device output)
check_PROGRAMS += test_audio_suite 
# Cover art selection and data: URIs for MPRIS
check_PROGRAMS += test_cover_art
test_cover_art_SOURCES = test_cover_art.cpp
test_cover_art_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Decoded PCM stays whole-frame and gapless across stream reads
# Audio threading pattern test (simpler test without SDL dependencies)
# Surface thread safety test
# Surface performance regression test
# SimplePCMCodec integration test (minimal version)
# Simple PCM Codec Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_simplepcm_suite_SOURCES = test_simplepcm_suite.cpp \
	codecs/pcm/test_simplepcmcodec_comprehensive_integration.cpp \
	codecs/pcm/test_simplepcmcodec_integration_minimal.cpp \
	codecs/pcm/test_simplepcmcodec_real_integration.cpp
test_simplepcm_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# SimplePCMCodec real integration test (with MuLaw/ALaw codecs)
# SimplePCMCodec comprehensive integration test (minimal dependencies)
# μ-law/A-law codec integration tests with SimplePCMCodec
# μ-law codec property-based tests (standalone, minimal dependencies)
test_mulaw_codec_properties_SOURCES = test_mulaw_codec_properties.cpp
test_mulaw_codec_properties_LDADD = $(AM_LDFLAGS)

# A-law codec property-based tests (standalone, minimal dependencies)
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
test_lastfm_url_encode_unit_SOURCES = test_lastfm_url_encode_unit.cpp
test_lastfm_url_encode_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/lastfm/libpsymp3-lastfm.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(SYSTEM_LIBS) \
	$(AM_LDFLAGS)

test_lastfm_url_encoding_properties_SOURCES = test_lastfm_url_encoding_properties.cpp
test_lastfm_url_encoding_properties_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/lastfm/libpsymp3-lastfm.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(SYSTEM_LIBS) \
	$(AM_LDFLAGS)

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
test_flac_vorbis_field_name_properties_SOURCES = test_flac_vorbis_field_name_properties.cpp
test_flac_vorbis_field_name_properties_LDADD = $(AM_LDFLAGS) $(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a $(top_builddir)/src/demuxer/libpsymp3-demuxer.a $(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a $(top_builddir)/src/tag/libpsymp3-tag.a $(top_builddir)/src/core/utility/libpsymp3-core-utility.a $(top_builddir)/src/core/libpsymp3-core.a $(top_builddir)/src/io/libpsymp3-io.a $(top_builddir)/src/debug.o
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
# μ-law/A-law error handling unit tests (standalone, minimal dependencies)
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
# FLAC demuxer real file test
# FLAC STREAMINFO real files test
# StreamInfo Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_streaminfo_suite_SOURCES = test_streaminfo_suite.cpp \
	demuxer/streaminfo/test_streaminfo_real_files.cpp \
	demuxer/streaminfo/test_streaminfo_recovery.cpp
test_streaminfo_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# FLAC STREAMINFO recovery test
# FLAC STREAMINFO debug/extraction test (standalone, no library dependencies)
test_streaminfo_debug_SOURCES = test_streaminfo_debug.cpp
test_streaminfo_debug_LDADD = $(AM_LDFLAGS)

# FLAC demuxer diagnostic test
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

# FLAC demuxer performance test
# FLAC compatibility integration test
# FLAC demuxer thread safety test
# FLAC backward compatibility validation test (Requirements 27.1-27.8)
# FLAC demuxer real file integration tests (discovers FLAC files in tests/data/)
# FLAC bisection seeking real file tests (Task 7.1, 7.2 from flac-bisection-seeking spec)
# FLAC mini player test (Task 7.1, 7.2 from flac-bisection-seeking spec)
# FLAC demuxer comprehensive unit tests
# FLAC demuxer comprehensive integration tests
# FLAC demuxer unit tests (fixed, no external files required)
# FLAC index-based seeking test
# FLAC codec variable block size test
# FLAC codec minimal container-agnostic test
# FLAC codec container-agnostic operation test
# Note: This test requires HAVE_NATIVE_FLAC; when not defined, it's a skip stub

# Simple FLAC codec quality validation test (minimal dependencies)
# FLAC thread safety validation test
# Minimal FLAC codec unit tests
test_flac_parse_coded_number_SOURCES = test_flac_parse_coded_number.cpp
test_flac_parse_coded_number_LDADD = \
	$(FULL_CODEC_LIBS) \
	$(COMMON_CODEC_LIBS) \
	$(AM_LDFLAGS)

endif

# IOHandler thread safety comprehensive test
# IOHandler memory deadlock prevention test
# MemoryPoolManager integration test
# MemoryTracker unit test
# MemoryPoolManager comprehensive thread safety test
# MemoryPoolManager basic threading test
# MemoryPoolManager allocation failure test
test_frame_size_estimation_consistency_SOURCES = test_frame_size_estimation_consistency.cpp
test_frame_size_estimation_consistency_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/flac/libpsymp3-demuxer-flac.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)
# FLAC AudioFrame creation test
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
	$(top_builddir)/src/codecs/mp2/libpsymp3-codec-mp2.a \
	$(top_builddir)/src/codecs/alac/libpsymp3-codec-alac.a \
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
	$(top_builddir)/src/codecs/mp2/libpsymp3-codec-mp2.a \
	$(top_builddir)/src/codecs/alac/libpsymp3-codec-alac.a \
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
##	$(OPUS_LIBS) \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
#	$(OGG_LIBS) \
#	$(DBUS_LIBS) \
#	$(AM_LDFLAGS)

# Note: FLACCodec.o and EnhancedAudioBufferPool.o build rules already defined above

# FLAC codec compatibility test
# FLAC codec integration test

# Comprehensive FLAC codec unit tests
# FLAC codec error handling tests
# FLAC codec performance comprehensive tests
# FLAC codec multi-threaded tests
# FLAC codec demuxer integration tests
# FLAC codec performance benchmarking tests
# FLAC codec conditional compilation tests
# FLAC binary search limitations test
# FLAC codec threading safety test

# FLAC demuxer threading test
# FLAC performance optimization test
# FLAC performance with real files test
# FLAC comprehensive validation test
# FLAC RFC 9639 compliance validation test
# FLAC RFC 9639 subframe type validation test
# FLAC RFC 9639 entropy coding validation test
# Simple entropy coding compilation test
test_entropy_coding_compilation_SOURCES = test_entropy_coding_compilation.cpp
test_entropy_coding_compilation_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(AM_LDFLAGS)

# FLAC test data validation test
# FLAC CRC validation test (RFC 9639 compliance)
# RFC 9639 bit depth and sample format compliance validation test
# Simple RFC 9639 bit depth validation test (minimal dependencies)
# FLAC security validation tests
# RFC 9639 sample format and bit depth compliance test
# OggDemuxer performance test
# OggDemuxer comprehensive test suite
# OggDemuxer comprehensive integration test (Task 20)
# Opus Codec Tests
test_opus_codec_core_decoding_SOURCES = test_opus_codec_core_decoding.cpp
test_opus_codec_core_decoding_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-logg \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	$(AM_LDFLAGS)

test_opus_codec_error_handling_SOURCES = test_opus_codec_error_handling.cpp
test_opus_codec_error_handling_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	-logg \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	$(AM_LDFLAGS)

test_opus_integration_SOURCES = test_opus_integration.cpp
test_opus_integration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	-logg \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/mp2/libpsymp3-codec-mp2.a \
	$(top_builddir)/src/codecs/alac/libpsymp3-codec-alac.a \
	$(AM_LDFLAGS)

test_opus_compatibility_performance_SOURCES = test_opus_compatibility_performance.cpp
test_opus_compatibility_performance_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/stream.o \
	$(top_builddir)/src/system.o \
	$(top_builddir)/src/track.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	-logg \
	$(OPUS_LIBS) \
	$(OPUS_LIBS) \
	$(CODEC_LIBS) \
	$(top_builddir)/src/codecs/mp3/libpsymp3-codec-mp3.a \
	$(top_builddir)/src/codecs/mp2/libpsymp3-codec-mp2.a \
	$(top_builddir)/src/codecs/alac/libpsymp3-codec-alac.a \
	$(AM_LDFLAGS)

# MPRIS Mock Framework Tests
if HAVE_DBUS

test_mock_framework_standalone_SOURCES = test_mock_framework_standalone.cpp
test_mock_framework_standalone_LDADD = libtest_utilities.a $(AM_LDFLAGS)

# Comprehensive MPRIS component tests
endif

# MPRIS Integration Tests
# Test scripts for running comprehensive test suites
EXTRA_DIST = matroska_ebml_builder.h \
	fdk_aac_test_encoder.h \
	flac_test_stream.h \
	run_mpris_mock_tests.sh \
	run_mpris_stress_tests.sh \
	run_mpris_logging_tests.sh \
	run_mpris_integration_tests.sh

# MPRIS Final Validation Tests (Task 15)
if HAVE_DBUS
endif

# Additional test scripts and headers to be distributed
EXTRA_DIST += \
	generate_ac3_corpus.sh \
	fetch_g722_testvectors.sh \
	run_mpris_static_analysis.sh \
	run_mpris_final_validation.sh \
	run_mpris_integration_tests.sh \
	run_basic_integration_tests.sh \
	run_mpris_logging_tests.sh \
	fake_player.h

# Test-support headers (mocks, fixtures, utilities): #included by the
# distributed test sources, so the dist tree cannot compile without them.
EXTRA_DIST += \
	FLACRFCComplianceValidator.h \
	MockOggFile.h \
	flac_test_data_utils.h \
	g722_fixture.h \
	mock_dbus_connection.h \
	mock_include/psymp3.h \
	mock_player.h \
	mocks/dbus/dbus.h \
	mocks/dummy_inc/FLAC++/decoder.h \
	mocks/dummy_inc/SDL.h \
	mocks/dummy_inc/SDL/SDL.h \
	mocks/dummy_inc/SDL/SDL_mutex.h \
	mocks/dummy_inc/SDL/SDL_thread.h \
	mocks/dummy_inc/SDL_mutex.h \
	mocks/dummy_inc/SDL_thread.h \
	mocks/dummy_inc/SDL_ttf.h \
	mocks/dummy_inc/curl/curl.h \
	mocks/dummy_inc/dbus/dbus.h \
	mocks/dummy_inc/freetype.h \
	mocks/dummy_inc/freetype/freetype.h \
	mocks/dummy_inc/ft2build.h \
	mocks/dummy_inc/mpris/MPRISTypes.h \
	mocks/dummy_inc/mpris/MethodHandler.h \
	mocks/dummy_inc/mpris/PropertyManager.h \
	mocks/dummy_inc/openssl/bio.h \
	mocks/dummy_inc/openssl/crypto.h \
	mocks/dummy_inc/openssl/err.h \
	mocks/dummy_inc/openssl/evp.h \
	mocks/dummy_inc/openssl/md5.h \
	mocks/dummy_inc/openssl/pem.h \
	mocks/dummy_inc/openssl/rand.h \
	mocks/dummy_inc/openssl/ssl.h \
	mocks/dummy_inc/openssl/x509.h \
	mocks/dummy_inc/openssl/x509v3.h \
	mocks/dummy_inc/opus/opus.h \
	mocks/dummy_inc/opus/opus_multistream.h \
	mocks/dummy_inc/psymp3.h \
	mocks/dummy_inc/taglib/fileref.h \
	mocks/dummy_inc/taglib/tag.h \
	mocks/dummy_inc/taglib/tiostream.h \
	mocks/dummy_inc/taglib/tstring.h \
	mocks/dummy_inc/vorbis/vorbisfile.h \
	mocks/io/FileIOHandler.h \
	mocks/io/IOHandler.h \
	mocks/io/URI.h \
	mocks/mock_dbus/dbus/dbus.h \
	mocks/mock_include/SDL/SDL.h \
	mocks/mock_include/SDL/SDL_mutex.h \
	mocks/mock_include/SDL/SDL_thread.h \
	mocks/mock_include/dbus/dbus.h \
	mocks/mock_include/debug.h \
	mocks/mock_include/psymp3.h \
	mocks/mock_include/taglib/fileref.h \
	mocks/mock_include/taglib/tag.h \
	mocks/mock_include/taglib/tiostream.h \
	mocks/mock_include/taglib/tstring.h \
	mocks/psymp3.h \
	mocks/stream.h \
	mocks/taglib/tstring.h \
	test_discovery.h \
	test_executor.h \
	test_framework.h \
	test_framework_threading.h \
	test_rect_utilities.h \
	test_reporter.h

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
	rm -rf tests # corpus generators (run as TESTS) write tests/data/fuzz_corpus/ here; distcleancheck flags leftovers

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
	-logg -lopus -lcurl -lssl -lcrypto -ltag
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
# FLAC seeking crash test (reproduces memory corruption during seeking)
# FLAC audio output debug test (comprehensive pipeline debugging)
# FLAC codec simple debug test (minimal codec testing)
# FLAC parser vulnerability test (SEC-02)
test_flac_parser_vulnerability_SOURCES = test_flac_parser_vulnerability.cpp
test_flac_parser_vulnerability_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/iso/libpsymp3-demuxer-iso.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# FLAC streamable subset real files test

# Debug system test
EXTRA_PROGRAMS += test_debug_system
test_debug_system_SOURCES = test_debug_system.cpp
test_debug_system_LDADD = \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Native FLAC codec unit tests (conditional on HAVE_NATIVE_FLAC)
if HAVE_NATIVE_FLAC
check_PROGRAMS += test_bitstream_reader_unit test_frame_parser_unit test_subframe_decoder_unit \
	test_residual_decoder_unit test_channel_decorrelator_unit test_sample_reconstructor_unit \
	test_native_flac_suite \
	test_flac_parse_coded_number \
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
# Native FLAC Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_native_flac_suite_SOURCES = test_native_flac_suite.cpp \
	codecs/flac/native/test_native_flac_containers.cpp \
	codecs/flac/native/test_native_flac_edge_cases.cpp \
	codecs/flac/native/test_native_flac_memory_usage.cpp \
	codecs/flac/native/test_native_flac_performance_benchmark.cpp \
	codecs/flac/native/test_native_flac_real_files.cpp \
	codecs/flac/native/test_native_flac_threading.cpp
test_native_flac_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# Integration test: Container formats
# Integration test: Edge cases
# Performance test: benchmark native FLAC decoding
# Performance test: Memory usage
# Performance test: Threading
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
# FLAC Seeking Property Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_flac_properties_suite_SOURCES = test_flac_properties_suite.cpp \
	codecs/flac/properties/test_flac_best_position_selection_properties.cpp \
	codecs/flac/properties/test_flac_bisection_convergence_properties.cpp \
	codecs/flac/properties/test_flac_bisection_range_adjustment_properties.cpp \
	codecs/flac/properties/test_flac_bit_depth_bits_properties.cpp \
	codecs/flac/properties/test_flac_byte_position_estimation_properties.cpp \
	codecs/flac/properties/test_flac_channel_bits_properties.cpp \
	codecs/flac/properties/test_flac_strategy_priority_properties.cpp \
	codecs/flac/properties/test_flac_time_differential_properties.cpp
test_flac_properties_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

# **Feature: flac-demuxer, Property 12: Reserved Bit Depth Detection**
# **Validates: Requirements 8.5**
# **Feature: flac-bisection-seeking, Property 1: Byte Position Estimation Correctness**
# **Validates: Requirements 1.1, 1.2, 1.4, 1.5**
# **Feature: flac-bisection-seeking, Property 6: Bisection Range Adjustment**
# **Validates: Requirements 3.1, 3.2**
# **Feature: flac-bisection-seeking, Property 7: Convergence Termination**
# **Validates: Requirements 3.3, 3.4, 3.5, 3.6**

# **Feature: flac-bisection-seeking, Property 8: Time Differential Calculation**
# **Validates: Requirements 4.1, 4.2**

# **Feature: flac-bisection-seeking, Property 9: Best Position Selection**
# **Validates: Requirements 4.3, 4.4**
check_PROGRAMS += test_flac_properties_suite 

# **Feature: flac-bisection-seeking, Property 10: Strategy Priority**
# **Validates: Requirements 7.1, 7.2, 7.3, 7.5**

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
EXTRA_PROGRAMS += flacout

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
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(FLAC_LIBS) \
	-logg -lopus -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(AAC_LIBS)
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
EXTRA_PROGRAMS += vorbisout

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
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-logg -lopus -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(FLAC_LIBS) $(AAC_LIBS)
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

EXTRA_PROGRAMS += mp3out

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
	$(top_builddir)/src/codecs/libpsymp3-codecs.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	-logg -lopus -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(AAC_LIBS)

# ============================================================================
# Tag Framework Tests
# ============================================================================

check_PROGRAMS += test_tag_suite test_id3v2_tag test_demuxer_extra_suite test_stream_tag_integration 

# Tag unit tests
# Tag Framework Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_tag_suite_SOURCES = test_tag_suite.cpp \
	tag/test_tag_unit.cpp \
	tag/test_tag_properties.cpp \
	tag/test_tag_factory.cpp \
	tag/test_tag_error_handling_manual.cpp \
	tag/test_tag_thread_safety.cpp \
	tag/test_tag_picture_properties.cpp \
	tag/test_tag_picture_unit.cpp
test_tag_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)
if HAVE_RAPIDCHECK
# Two of these are property-based tests; they were linking it themselves.
test_tag_suite_LDADD += $(RAPIDCHECK_LIBS)
endif

# Tag property-based tests
if HAVE_RAPIDCHECK
else
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
# Tag error handling tests
# Demuxer tag extraction tests
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	-lopus -lcurl -lssl -lcrypto \
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
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
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
	-lopus -lcurl -lssl -lcrypto \
	$(AM_LDFLAGS) $(FLAC_LIBS) $(TAGLIB_LIBS)
endif

# Tag thread safety tests (Property 12: Thread-Safe Concurrent Reads)
if HAVE_RAPIDCHECK
else
endif

# Tag picture access property tests (Property 13: Picture Index Access)
if HAVE_RAPIDCHECK
else
endif

# Tag picture access unit tests
# Tag fuzzing tests and corpus generators
check_PROGRAMS += fuzz_tag_vorbis fuzz_tag_id3v1 fuzz_tag_id3v2 fuzz_tag_picture
check_PROGRAMS += generate_vorbis_corpus generate_id3v1_corpus generate_id3v2_corpus generate_picture_corpus
# Nothing reads its output, so make check does not run it; build it by name.
EXTRA_PROGRAMS += generate_matroska_corpus

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
check_PROGRAMS += test_playlist_suite 
# ============================================================================
# UTF8Util Tests
# ============================================================================

check_PROGRAMS += test_utf8util_suite 

# UTF8Util unit tests
# UTF-8 Utility Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_utf8util_suite_SOURCES = test_utf8util_suite.cpp \
	core/utf8/fuzz_utf8util.cpp \
	core/utf8/test_utf8util_properties.cpp \
	core/utf8/test_utf8util_unit.cpp
test_utf8util_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)
if HAVE_RAPIDCHECK
# One of these is a property-based test, and linked it itself.
test_utf8util_suite_LDADD += -lrapidcheck
endif

# UTF8Util property-based tests
if HAVE_RAPIDCHECK
endif

# UTF8Util fuzzing tests

# Playlist load test
# Base64 Tests
# ============================================================================

check_PROGRAMS += test_base64_unit

# Base64 unit tests
test_base64_unit_SOURCES = test_base64_unit.cpp
# FFT Tests: one program, as the AC-3 and Matroska tests are. Each test keeps
# its file and its suite; what they no longer have is a link apiece.
test_fft_suite_SOURCES = test_fft_suite.cpp \
	core/fft/test_fft_mode.cpp \
	core/fft/test_fft_unit.cpp
test_fft_suite_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_base64_unit_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# BoundedQueue Tests
# ============================================================================

# BoundedQueue tests
check_PROGRAMS += test_fft_suite 
# Every check program is a test, and runs exactly once per `make check`. With
# the harness configured, it is the runner (see check-local) and automake's
# check-TESTS is given nothing, or the whole suite would run twice. Without the
# harness, check-TESTS runs them.
if BUILD_TEST_HARNESS
TESTS =
else
TESTS = $(check_PROGRAMS)
endif

if HAVE_DBUS
check_PROGRAMS += test_loop_status
test_loop_status_SOURCES = test_loop_status.cpp player_stub.cpp
test_loop_status_LDADD = $(MPRIS_TEST_LIBS) $(AM_LDFLAGS)
endif

if HAVE_OGGDEMUXER
check_PROGRAMS += test_issue_ogg_duration
test_issue_ogg_duration_SOURCES = test_issue_ogg_duration.cpp
test_issue_ogg_duration_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/demuxer/ogg/libpsymp3-demuxer-ogg.a \
	$(top_builddir)/src/demuxer/libpsymp3-demuxer.a \
	$(top_builddir)/src/demuxer/raw/libpsymp3-demuxer-raw.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/io/http/libpsymp3-io-http.a \
	$(top_builddir)/src/io/file/libpsymp3-io-file.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/tag/libpsymp3-tag.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(OGG_LIBS) \
	$(OPUS_LIBS) \
	$(FLAC_LIBS) \
	$(AM_LDFLAGS)
endif
test_flac_stream_marker_with_logging_SOURCES = test_flac_stream_marker_with_logging.cpp
test_flac_stream_marker_with_logging_LDADD = $(AM_LDFLAGS)

check_PROGRAMS += test_storage_permissions
test_storage_permissions_SOURCES = test_storage_permissions.cpp
test_storage_permissions_LDADD = $(COMMON_TEST_LIBS) $(AM_LDFLAGS)

test_http_io_handler_SOURCES = test_http_io_handler.cpp $(top_srcdir)/src/io/http/HTTPIOHandler.cpp
test_http_io_handler_LDADD = \
	libtest_utilities.a \
	$(top_builddir)/src/core/libpsymp3-core.a \
	$(top_builddir)/src/core/utility/libpsymp3-core-utility.a \
	$(top_builddir)/src/io/libpsymp3-io.a \
	$(top_builddir)/src/debug.o \
	$(AM_LDFLAGS)

# Project-owned hardening/ABI C++ flags (see configure.ac PSYMP3_CXXFLAGS)
AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
