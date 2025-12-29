noinst_LIBRARIES = libpsymp3-mpris.a

libpsymp3_mpris_a_SOURCES = \
	MPRISManager.cpp \
	DBusConnectionManager.cpp \
	MPRISTypes.cpp \
	MethodHandler.cpp \
	PropertyManager.cpp \
	SignalEmitter.cpp

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(FLAC_CFLAGS) $(LIBMPG123_CFLAGS)
