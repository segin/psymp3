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

# The decoder itself is not built here: src/Makefile.am compiles
# codecs/mlp/mlp_decoder_impl.cpp straight into the binary, because the demuxer
# needs it too and an archive would have to be scanned twice to satisfy both.
libpsymp3_codec_mlp_a_SOURCES = \
	MLPCodec.cpp

AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
