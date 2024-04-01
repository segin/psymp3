AC_PREREQ([2.69])
AC_INIT([PsyMP3], [1.99.3], [segin2005@gmail.com], [psymp3], [https://github.com/segin/psymp3])
AM_INIT_AUTOMAKE([1.10])

AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_HEADER_STDBOOL
AC_FUNC_MALLOC

AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

AC_CHECK_FUNCS([atexit getcwd memset])

PKG_CHECK_MODULES([SDL], [sdl >= 1.2.13])
PKG_CHECK_MODULES([LIBMPG123], [libmpg123 >= 1.8])
PKG_CHECK_MODULES([TAGLIB], [taglib >= 1.6])
PKG_CHECK_MODULES([VORBISFILE], [vorbisfile >= 1.3])
PKG_CHECK_MODULES([SDL_GFX], [SDL_gfx >= 2.0])
PKG_CHECK_MODULES([SDL_TTF], [SDL_ttf >= 2.0])
PKG_CHECK_MODULES([FLACMM], [flac++ >= 1.4])
PKG_CHECK_MODULES([OPUSFILE], [opusfile >= 0.12])

PKG_CHECK_MODULES([DEPENDS], [sdl >= 1.2.13 libmpg123 >= 1.8 taglib >= 1.6 vorbisfile >= 1.3 SDL_gfx >= 2.0 SDL_ttf >= 2.0 flac++ >= 1.4 opusfile >= 0.12])

AC_SUBST([DEPENDS_CFLAGS])
AC_SUBST([DEPENDS_LIBS])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile src/Makefile res/Makefile])

AC_OUTPUT
