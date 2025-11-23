noinst_LIBRARIES = libmpris.a

libmpris_a_SOURCES = \
	MPRISManager.cpp \
	DBusConnectionManager.cpp \
	MPRISTypes.cpp \
	MethodHandler.cpp \
	PropertyManager.cpp \
	SignalEmitter.cpp

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(FLAC_CFLAGS) $(LIBMPG123_CFLAGS)
libmpris_a_CXXFLAGS = $(AM_CXXFLAGS)
