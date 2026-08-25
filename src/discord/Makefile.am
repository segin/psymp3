#
# Makefile.am - automake input for Discord Rich Presence submodule
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

noinst_LIBRARIES = libpsymp3-discord.a

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) -Wall -Werror

libpsymp3_discord_a_SOURCES = \
	DiscordPresence.cpp

# Project-owned hardening/ABI C++ flags (see configure.ac PSYMP3_CXXFLAGS)
AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
