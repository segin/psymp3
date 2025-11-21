AC_PREREQ([2.69])
AC_INIT([PsyMP3],[2-CURRENT],[segin2005@gmail.com],[psymp3],[https://github.com/segin/psymp3])
AM_INIT_AUTOMAKE([1.10])

AU_DEFUN([AX_REQUIRE_CPP], [ax_require_cpp(std_17, reject)])

AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_HEADER_STDBOOL
AC_FUNC_MALLOC
AX_CXX_COMPILE_STDCXX_17

AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

AC_CHECK_FUNCS([atexit getcwd memset])

# Core mandatory dependencies
PKG_CHECK_MODULES([SDL], [sdl >= 1.2])
PKG_CHECK_MODULES([TAGLIB], [taglib >= 1.6])
PKG_CHECK_MODULES([FREETYPE], [freetype2])
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([CURL], [libcurl])

# Optional external codec support
AC_ARG_ENABLE([flac],
  [AS_HELP_STRING([--enable-flac], [Enable FLAC support @<:@default=yes@:>@])],
  [enable_flac=$enableval], [enable_flac=yes])

AC_ARG_ENABLE([native-flac],
  [AS_HELP_STRING([--enable-native-flac], [Use native FLAC decoder instead of libFLAC @<:@default=no@:>@])],
  [enable_native_flac=$enableval], [enable_native_flac=no])

AC_ARG_ENABLE([mp3],
  [AS_HELP_STRING([--enable-mp3], [Enable MP3 support @<:@default=yes@:>@])],
  [enable_mp3=$enableval], [enable_mp3=yes])

AC_ARG_ENABLE([vorbis],
  [AS_HELP_STRING([--enable-vorbis], [Enable Vorbis support @<:@default=yes@:>@])],
  [enable_vorbis=$enableval], [enable_vorbis=yes])

AC_ARG_ENABLE([opus],
  [AS_HELP_STRING([--enable-opus], [Enable Opus support @<:@default=yes@:>@])],
  [enable_opus=$enableval], [enable_opus=yes])

# Check for optional codec libraries
have_flac=no
have_native_flac=no
have_mp3=no
have_vorbis=no
have_opus=no

if test "x$enable_flac" = "xyes"; then
  if test "x$enable_native_flac" = "xyes"; then
    # Use native FLAC decoder (no external dependency)
    have_native_flac=yes
    have_flac=yes
    AC_DEFINE([HAVE_NATIVE_FLAC], [1], [Define to 1 if native FLAC decoder is enabled])
    AC_DEFINE([HAVE_FLAC], [1], [Define to 1 if FLAC support is enabled])
    AC_MSG_NOTICE([Using native FLAC decoder (no libFLAC dependency)])
  else
    # Use libFLAC wrapper
    PKG_CHECK_MODULES([FLAC], [flac++], [have_flac=yes], [have_flac=no])
    if test "x$have_flac" = "xyes"; then
      AC_DEFINE([HAVE_FLAC], [1], [Define to 1 if FLAC support is enabled])
      AC_MSG_NOTICE([Using libFLAC wrapper for FLAC decoding])
    fi
  fi
fi

if test "x$enable_mp3" = "xyes"; then
  PKG_CHECK_MODULES([LIBMPG123], [libmpg123 >= 1.8], [have_mp3=yes], [have_mp3=no])
  if test "x$have_mp3" = "xyes"; then
    AC_DEFINE([HAVE_MP3], [1], [Define to 1 if MP3 support is enabled])
  fi
fi

# Check for libogg first - required for Vorbis, Opus, and optionally FLAC
PKG_CHECK_MODULES([OGG], [ogg], [have_ogg=yes], [have_ogg=no])

if test "x$enable_vorbis" = "xyes"; then
  if test "x$have_ogg" = "xyes"; then
    PKG_CHECK_MODULES([VORBIS], [vorbis], [have_vorbis=yes], [have_vorbis=no])
    if test "x$have_vorbis" = "xyes"; then
      AC_DEFINE([HAVE_VORBIS], [1], [Define to 1 if Vorbis support is enabled])
    fi
  else
    have_vorbis=no
    AC_MSG_WARN([Vorbis support disabled: libogg not found])
  fi
else
  have_vorbis=no
fi

if test "x$enable_opus" = "xyes"; then
  if test "x$have_ogg" = "xyes"; then
    PKG_CHECK_MODULES([OPUS], [opus], [have_opus=yes], [have_opus=no])
    if test "x$have_opus" = "xyes"; then
      AC_DEFINE([HAVE_OPUS], [1], [Define to 1 if Opus support is enabled])
    fi
  else
    have_opus=no
    AC_MSG_WARN([Opus support disabled: libogg not found])
  fi
else
  have_opus=no
fi

# Determine if OggDemuxer should be built
# OggDemuxer is built if:
# - Both Vorbis and Opus are enabled, OR
# - Only Vorbis is enabled, OR  
# - Only Opus is enabled, OR
# - FLAC is enabled and libogg is available (for Ogg FLAC support)
have_oggdemuxer=no
if test "x$have_vorbis" = "xyes" || test "x$have_opus" = "xyes"; then
  have_oggdemuxer=yes
elif test "x$have_flac" = "xyes" && test "x$have_ogg" = "xyes"; then
  have_oggdemuxer=yes
  AC_DEFINE([HAVE_OGG_FLAC], [1], [Define to 1 if Ogg FLAC support is enabled])
fi

if test "x$have_oggdemuxer" = "xyes"; then
  AC_DEFINE([HAVE_OGGDEMUXER], [1], [Define to 1 if OggDemuxer should be built])
fi

# Optional G.711 codec support (A-law and μ-law)
AC_ARG_ENABLE([alaw],
  [AS_HELP_STRING([--enable-alaw], [Enable A-law (G.711 A-law) codec support @<:@default=yes@:>@])],
  [enable_alaw=$enableval], [enable_alaw=yes])

AC_ARG_ENABLE([mulaw],
  [AS_HELP_STRING([--enable-mulaw], [Enable μ-law (G.711 μ-law) codec support @<:@default=yes@:>@])],
  [enable_mulaw=$enableval], [enable_mulaw=yes])

# G.711 codecs have no external dependencies - they are always available if enabled
have_alaw=no
have_mulaw=no

if test "x$enable_alaw" = "xyes"; then
  have_alaw=yes
  AC_DEFINE([ENABLE_ALAW_CODEC], [1], [Define to 1 if A-law codec support is enabled])
fi

if test "x$enable_mulaw" = "xyes"; then
  have_mulaw=yes
  AC_DEFINE([ENABLE_MULAW_CODEC], [1], [Define to 1 if μ-law codec support is enabled])
fi

AM_CONDITIONAL([HAVE_FLAC], [test "x$have_flac" = "xyes"])
AM_CONDITIONAL([HAVE_NATIVE_FLAC], [test "x$have_native_flac" = "xyes"])
AM_CONDITIONAL([HAVE_MP3], [test "x$have_mp3" = "xyes"])
AM_CONDITIONAL([HAVE_VORBIS], [test "x$have_vorbis" = "xyes"])
AM_CONDITIONAL([HAVE_OPUS], [test "x$have_opus" = "xyes"])
AM_CONDITIONAL([HAVE_OGG], [test "x$have_ogg" = "xyes"])
AM_CONDITIONAL([HAVE_OGGDEMUXER], [test "x$have_oggdemuxer" = "xyes"])
AM_CONDITIONAL([HAVE_ALAW], [test "x$have_alaw" = "xyes"])
AM_CONDITIONAL([HAVE_MULAW], [test "x$have_mulaw" = "xyes"])

AC_ARG_ENABLE(mpris,
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

# Test harness configuration
# Detect Windows builds and disable testing by default unless explicitly enabled
AC_ARG_ENABLE([test-harness],
  [AS_HELP_STRING([--enable-test-harness], [Build test harness @<:@default=yes on Unix, no on Windows@:>@])],
  [enable_test_harness=$enableval],
  [
    # Default: enable on Unix/Linux, disable on Windows
    # Windows builds have issues with SDL_main wrapper in test programs
    case "$host_os" in
      mingw* | msys* | cygwin*)
        enable_test_harness=no
        AC_MSG_NOTICE([Windows build detected - test harness disabled by default (use --enable-test-harness to override)])
        ;;
      *)
        enable_test_harness=yes
        ;;
    esac
  ])

AM_CONDITIONAL([BUILD_TEST_HARNESS], [test "x$enable_test_harness" = "xyes"])

# Release build optimization
AC_ARG_ENABLE([release],
  [AS_HELP_STRING([--enable-release], [Enable release build optimizations (no debug symbols) @<:@default=no@:>@])],
  [enable_release=$enableval],
  [enable_release=no])

if test "x$enable_release" = "xyes"; then
  AC_DEFINE([RELEASE_BUILD], [1], [Define to 1 for release builds])
  CXXFLAGS="$CXXFLAGS -O3 -DNDEBUG -fomit-frame-pointer -s"
  CFLAGS="$CFLAGS -O3 -DNDEBUG -fomit-frame-pointer -s"
  LDFLAGS="$LDFLAGS -s"
fi

AM_CONDITIONAL([RELEASE_BUILD], [test "x$enable_release" = "xyes"])

# Final build (single compilation unit)
AC_ARG_ENABLE([final],
  [AS_HELP_STRING([--enable-final], [Enable final build (single compilation unit like KDE3) @<:@default=no@:>@])],
  [enable_final=$enableval],
  [enable_final=no])

if test "x$enable_final" = "xyes"; then
  AC_DEFINE([FINAL_BUILD], [1], [Define to 1 for final builds])
fi

AM_CONDITIONAL([FINAL_BUILD], [test "x$enable_final" = "xyes"])

# Check for windres (Windows Resource Compiler) for cross-compilation
AC_CHECK_TOOL([WINDRES], [windres], [no])
AM_CONDITIONAL([HAVE_WINDRES], [test "x$WINDRES" != "xno"])

# Windows socket libraries
case "$host_os" in
  *mingw*|*cygwin*|*msys*)
    WINSOCK_LIBS="-lws2_32 -lmswsock"
    AC_SUBST([WINSOCK_LIBS])
    ;;
  *)
    WINSOCK_LIBS=""
    AC_SUBST([WINSOCK_LIBS])
    ;;
esac

# Core dependencies check for final validation
PKG_CHECK_MODULES([DEPENDS], [sdl >= 1.2 taglib >= 1.6 openssl >= 1.0 libcurl >= 7.20.0])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile src/Makefile res/Makefile tests/Makefile])

# Print configuration summary
echo ""
echo "=========================================="
echo "PsyMP3 Configuration Summary"
echo "=========================================="
echo ""
echo "Core Dependencies:"
echo "  SDL:           yes"
echo "  TagLib:        yes"
echo "  FreeType:      yes"
echo "  OpenSSL:       yes"
echo "  libcurl:       yes"
echo ""
echo "Audio Codec Support:"
echo "  MP3 (libmpg123):   $have_mp3"
echo "  FLAC:              $have_flac"
if test "x$have_native_flac" = "xyes"; then
  echo "    Implementation:  Native (no libFLAC)"
else
  echo "    Implementation:  libFLAC wrapper"
fi
echo "  Vorbis:            $have_vorbis"
echo "  Opus:              $have_opus"
echo "  A-law (G.711):     $have_alaw"
echo "  μ-law (G.711):     $have_mulaw"
echo ""
echo "Container Support:"
echo "  FLAC:              $have_flac"
echo "  Ogg:               $have_ogg"
echo "  ISO (MP4):         yes"
echo "  RIFF (WAV):        yes"
echo ""
echo "Optional Features:"
echo "  MPRIS (D-Bus):     $have_dbus"
echo "  Test Harness:      $enable_test_harness"
echo "  Release Build:     $enable_release"
echo "  Final Build:       $enable_final"
echo ""
echo "=========================================="
echo ""

AC_OUTPUT