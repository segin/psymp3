AC_PREREQ([2.69])
AC_INIT([PsyMP3], [1.99.2], [segin2005@gmail.com], [psymp3], [https://github.com/segin/psymp3])
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

PKG_CHECK_MODULES([DEPENDS], [sdl >= 1.2.13 libmpg123 >= 1.8 taglib >= 1.6 vorbisfile >= 1.3 SDL_gfx >= 2.0])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile src/Makefile res/Makefile])


AC_OUTPUT
