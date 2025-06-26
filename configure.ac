#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([PsyMP3],[1.99.3],[segin2005@gmail.com],[psymp3],[https://github.com/segin/psymp3])
AM_INIT_AUTOMAKE([1.10])
AC_INIT([psymp3], [2-CURRENT], [segin2005@gmail.com])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([include/config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AU_DEFUN([AX_REQUIRE_CPP], [ax_require_cpp(std_17, reject)])

AC_PROG_CPP
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CXX
AC_HEADER_STDBOOL
AC_FUNC_MALLOC
AX_CXX_COMPILE_STDCXX_17

AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
# Checks for libraries.
PKG_PROG_PKG_CONFIG

AC_CHECK_HEADERS([stdlib.h string.h unistd.h])
# Check for all dependencies in one go.
# SDL_gfx has been removed as it's no longer used.
PSYMP3_MODULES="
    sdl >= 1.2.15
    SDL_ttf
    libmpg123
    taglib
    vorbisfile
    opusfile
    FLAC++
"
PKG_CHECK_MODULES([DEPENDS], [$PSYMP3_MODULES])

AC_CHECK_FUNCS([atexit getcwd memset])
AC_SUBST(DEPENDS_CFLAGS)
AC_SUBST(DEPENDS_LIBS)

PKG_CHECK_MODULES([SDL], [sdl >= 1.2])
PKG_CHECK_MODULES([LIBMPG123], [libmpg123 >= 1.8])
PKG_CHECK_MODULES([TAGLIB], [taglib >= 1.6])
PKG_CHECK_MODULES([VORBISFILE], [vorbisfile >= 1.3])
PKG_CHECK_MODULES([SDL_GFX], [SDL_gfx >= 2.0])
PKG_CHECK_MODULES([SDL_TTF], [SDL_ttf >= 2.0])
PKG_CHECK_MODULES([FLACMM], [flac++ >= 1.2])
PKG_CHECK_MODULES([OPUSFILE], [opusfile >= 0.9])

PKG_CHECK_MODULES([DEPENDS], [sdl >= 1.2 libmpg123 >= 1.8 taglib >= 1.6 vorbisfile >= 1.3 SDL_gfx >= 2.0 SDL_ttf >= 2.0 flac++ >= 1.2 opusfile >= 0.9])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile src/Makefile res/Makefile])
AC_OUTPUT
