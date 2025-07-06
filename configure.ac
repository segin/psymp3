AC_PREREQ([2.69])
AC_INIT([PsyMP3],[1.99.4-RELEASE],[segin2005@gmail.com],[psymp3],[https://github.com/segin/psymp3])
AM_INIT_AUTOMAKE([1.10])

AU_DEFUN([AX_REQUIRE_CPP], [ax_require_cpp(std_17, reject)])

AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_HEADER_STDBOOL
AC_FUNC_MALLOC
AX_CXX_COMPILE_STDCXX_17

AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

AC_CHECK_FUNCS([atexit getcwd memset])

PKG_CHECK_MODULES([SDL], [sdl >= 1.2])
PKG_CHECK_MODULES([LIBMPG123], [libmpg123 >= 1.8])
PKG_CHECK_MODULES([TAGLIB], [taglib >= 1.6])
PKG_CHECK_MODULES([VORBISFILE], [vorbisfile >= 1.3])
PKG_CHECK_MODULES([FREETYPE], [freetype2])
PKG_CHECK_MODULES([FLACMM], [flac++ >= 1.2])
PKG_CHECK_MODULES([OPUSFILE], [opusfile >= 0.9])
PKG_CHECK_MODULES([VORBIS], [vorbis])
PKG_CHECK_MODULES([OPUS], [opus])
PKG_CHECK_MODULES([OGG], [ogg])

# MPRIS support (requires D-Bus)
AC_ARG_ENABLE([mpris],
  [AS_HELP_STRING([--enable-mpris], [Enable MPRIS media player remote interface support (requires D-Bus) @<:@default=auto@:>@])],
  [enable_mpris=$enableval],
  [enable_mpris=auto])

if test "x$enable_mpris" != "xno"; then
  PKG_CHECK_MODULES([DBUS], [dbus-1], 
    [
      AC_DEFINE([HAVE_DBUS], [1], [Define to 1 if you have the DBus library])
      have_dbus=yes
    ], 
    [
      have_dbus=no
      if test "x$enable_mpris" = "xyes"; then
        AC_MSG_ERROR([MPRIS support was requested but D-Bus library not found])
      fi
    ])
else
  have_dbus=no
fi

AM_CONDITIONAL([HAVE_DBUS], [test "x$have_dbus" = "xyes"])

PKG_CHECK_MODULES([DEPENDS], [sdl >= 1.2 libmpg123 >= 1.8 taglib >= 1.6 vorbisfile >= 1.3 flac++ >= 1.2 opusfile >= 0.9 vorbis opus ogg])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile src/Makefile res/Makefile])

AC_OUTPUT