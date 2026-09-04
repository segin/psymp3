#
# Makefile.am - automake input for the MLP/Dolby TrueHD codec
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(SPEEX_CFLAGS) $(OGG_CFLAGS) $(AAC_CFLAGS) $(G722_CFLAGS) -Wall -Werror

noinst_LIBRARIES = libpsymp3-codec-mlp.a

# The decoder is also compiled straight into the binary by src/Makefile.am,
# because the demuxer needs it too and CODEC_LIBS is scanned before
# DEMUXER_LIBS. Carrying it here as well costs one extra compile and lets the
# test suite pull it from the archive on demand -- an archive member is only
# taken when something still needs it, so the two copies can never collide.
libpsymp3_codec_mlp_a_SOURCES = \
	MLPCodec.cpp \
	mlp_decoder_impl.cpp

AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
