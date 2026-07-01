#
# Makefile.am - automake input for core subsystem
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

SUBDIRS = utility compression

noinst_LIBRARIES = libpsymp3-core.a

libpsymp3_core_a_SOURCES = \
	exceptions.cpp \
	song.cpp \
	rect.cpp \
	fft.cpp \
	fft_draw.cpp \
	about.cpp \
	persistentstorage.cpp \
	lyrics.cpp

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS)

# Project-owned hardening/ABI C++ flags (see configure.ac PSYMP3_CXXFLAGS)
AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)

# Native open-file dialog backend (Qt/GTK/Win32), isolated in its own
# convenience library so the GUI-toolkit headers/flags ($DIALOG_CFLAGS) touch
# only this one TU and don't collide with SDL/X11/TagLib elsewhere. Built only
# when configure found a backend (HAVE_FILEDIALOG).
if HAVE_FILEDIALOG
noinst_LIBRARIES += libpsymp3-filedialog.a
libpsymp3_filedialog_a_SOURCES = FileDialog.cpp
libpsymp3_filedialog_a_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include $(DIALOG_CFLAGS)
# -fPIC (overriding the project default -fPIE): Qt6 exports protected-visibility
# symbols (e.g. QCoreApplication::self) that the linker can't reach via a copy
# relocation from a PIE, so this TU must use full position-independent code.
libpsymp3_filedialog_a_CXXFLAGS = $(PSYMP3_CXXFLAGS) -fPIC
endif
