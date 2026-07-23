AC_PREREQ([2.69])
AC_INIT([PsyMP3],[1.99.13-RELEASE],[segin2005@gmail.com],[psymp3],[https://github.com/segin/psymp3])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.10 subdir-objects])

AU_DEFUN([AX_REQUIRE_CPP], [ax_require_cpp(std_17, reject)])

AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_SYS_LARGEFILE
AC_HEADER_STDBOOL
AC_FUNC_MALLOC
AX_CXX_COMPILE_STDCXX_17

# Position-independent executable (PIE) hardening. Enabled by default, but
# auto-disabled on i386: the system libraries available there are often non-PIC
# static archives (e.g. the TagLib package's libtag.a) that cannot be linked
# into a PIE -- ld reports "relocation R_386_32 ... recompile with -fPIC"
# against .eh_frame. --enable-pie forces it on, --disable-pie forces it off.
AC_ARG_ENABLE([pie],
  [AS_HELP_STRING([--enable-pie/--disable-pie],
    [build position-independent executables @<:@default=yes, except i386@:>@])],
  [enable_pie=$enableval], [enable_pie=auto])
if test "x$enable_pie" = "xauto"; then
  case "$host_cpu" in
    i?86) enable_pie=no ;;
    *)    enable_pie=yes ;;
  esac
fi
AC_MSG_NOTICE([position-independent executable (PIE): $enable_pie])

# Project-owned C++ flags (ABI-quieting + security hardening). These are kept
# in a substituted variable wired into each Makefile.am's AM_CXXFLAGS rather
# than appended to the user's CXXFLAGS, so a `make CXXFLAGS=...` override (or a
# distro build that resets CXXFLAGS) cannot silently drop hardening or
# re-expose the GCC 7.1 ARM psabi warnings. Each flag is probed for support.
#   -Wno-psabi             : suppress GCC 7.1 ARM ABI-change notes (noise only)
#   -fstack-protector-strong, -fPIE, -D_FORTIFY_SOURCE=2 : hardening
#   -pthread               : threading; AM_CXXFLAGS is used on the link command
#                            line too, so this also pulls in libthr/-lpthread.
psymp3_candidate_flags="-Wno-psabi -fstack-protector-strong -pthread"
if test "x$enable_pie" = "xyes"; then
  psymp3_candidate_flags="$psymp3_candidate_flags -fPIE"
fi
PSYMP3_CXXFLAGS=""
AC_LANG_PUSH([C++])
for psymp3_flag in $psymp3_candidate_flags; do
  AC_MSG_CHECKING([if compiler supports $psymp3_flag])
  psymp3_save_cxxflags="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS $psymp3_flag -Werror"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
    [AC_MSG_RESULT([yes]); PSYMP3_CXXFLAGS="$PSYMP3_CXXFLAGS $psymp3_flag"],
    [AC_MSG_RESULT([no])])
  CXXFLAGS="$psymp3_save_cxxflags"
done
AC_LANG_POP([C++])
# _FORTIFY_SOURCE needs an optimizing build; it is inert at -O0. Add it only if
# the toolchain/environment has not already defined it: distributions (e.g. Arch
# builds with =3) set their own via CPPFLAGS/CFLAGS, and defining it a second
# time is a fatal redefinition under -Werror.
AC_MSG_CHECKING([whether _FORTIFY_SOURCE is already defined])
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([[
#ifndef _FORTIFY_SOURCE
#error _FORTIFY_SOURCE is not defined
#endif
  ]], [[]])],
  [AC_MSG_RESULT([yes; leaving it to the toolchain])],
  [AC_MSG_RESULT([no])
   PSYMP3_CXXFLAGS="$PSYMP3_CXXFLAGS -D_FORTIFY_SOURCE=2"])
AC_LANG_POP([C++])
AC_SUBST([PSYMP3_CXXFLAGS])


# 64-bit atomics need libatomic on some 32-bit targets (i386, ARM) where the
# compiler lowers std::atomic<uint64_t> ops to __atomic_*_8 library calls
# instead of inlining them. Probe with a REAL std::atomic<uint64_t> program:
# the old AC_SEARCH_LIBS([__atomic_compare_exchange_8]) is unreliable because
# GCC treats that name as a builtin, so the conftest mis-links and no -latomic
# is added (observed: NetBSD i386/GCC 10 -> undefined __atomic_store_8 at link).
# Resolution order: inline (amd64, clang/i386) -> -latomic -> on i386 enable
# CMPXCHG8B via -march=i586 so GCC inlines them (NetBSD's base GCC ships no
# libatomic and defaults to i486, which lacks the instruction).
AC_MSG_CHECKING([how to link 64-bit atomics])
AC_LANG_PUSH([C++])
psymp3_atomic_prog="AC_LANG_PROGRAM([[#include <atomic>
#include <cstdint>
static std::atomic<std::uint64_t> a;]], [[
  a.store(2, std::memory_order_relaxed);
  return (int)(a.fetch_add(1) + a.load(std::memory_order_relaxed));]])"
AC_LINK_IFELSE([$psymp3_atomic_prog],
  [AC_MSG_RESULT([inlined])],
  [psymp3_save_libs="$LIBS"
   LIBS="$LIBS -latomic"
   AC_LINK_IFELSE([$psymp3_atomic_prog],
     [AC_MSG_RESULT([-latomic])],
     [LIBS="$psymp3_save_libs"
      psymp3_atomic_fixed=no
      case "$host_cpu" in
        i?86)
          psymp3_save_cxxflags="$CXXFLAGS"
          CXXFLAGS="$CXXFLAGS -march=i586"
          AC_LINK_IFELSE([$psymp3_atomic_prog],
            [AC_MSG_RESULT([-march=i586])
             PSYMP3_CXXFLAGS="$PSYMP3_CXXFLAGS -march=i586"
             psymp3_atomic_fixed=yes])
          CXXFLAGS="$psymp3_save_cxxflags"
          ;;
      esac
      if test "x$psymp3_atomic_fixed" != "xyes"; then
        AC_MSG_RESULT([unavailable])
        AC_MSG_WARN([could not make 64-bit std::atomic link; the build may fail])
      fi])])
AC_LANG_POP([C++])

AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

AC_CHECK_FUNCS([atexit getcwd memset])

# Core mandatory dependencies
PKG_CHECK_MODULES([SDL], [sdl2 >= 2.0], [], [
  AC_MSG_ERROR([SDL2 >= 2.0 is required. Install libsdl2-dev or equivalent.])
])
PKG_CHECK_MODULES([TAGLIB], [taglib >= 1.6])
PKG_CHECK_MODULES([FREETYPE], [freetype2])
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([CURL], [libcurl])

# Optional external codec support
AC_ARG_ENABLE([flac],
  [AS_HELP_STRING([--enable-flac], [Enable FLAC support (native implementation, no libFLAC dependency) @<:@default=yes@:>@])],
  [enable_flac=$enableval], [enable_flac=yes])

AC_ARG_ENABLE([vorbis],
  [AS_HELP_STRING([--enable-vorbis], [Enable Vorbis support @<:@default=yes@:>@])],
  [enable_vorbis=$enableval], [enable_vorbis=yes])

AC_ARG_ENABLE([opus],
  [AS_HELP_STRING([--enable-opus], [Enable Opus support @<:@default=yes@:>@])],
  [enable_opus=$enableval], [enable_opus=yes])

AC_ARG_ENABLE([aac],
  [AS_HELP_STRING([--enable-aac], [Enable AAC support via faad2 @<:@default=yes@:>@])],
  [enable_aac=$enableval], [enable_aac=yes])

AC_ARG_ENABLE([g722],
  [AS_HELP_STRING([--enable-g722], [Enable G.722 support via spandsp @<:@default=yes@:>@])],
  [enable_g722=$enableval], [enable_g722=yes])

# Check for optional codec libraries
have_flac=no
have_native_flac=no
have_vorbis=no
have_opus=no
have_aac=no
have_g722=no

# Native FLAC decoder - no external library dependency
if test "x$enable_flac" = "xyes"; then
  have_native_flac=yes
  have_flac=yes
  AC_DEFINE([HAVE_NATIVE_FLAC], [1], [Define to 1 if native FLAC decoder is enabled])
  AC_DEFINE([HAVE_FLAC], [1], [Define to 1 if FLAC support is enabled])
  AC_MSG_NOTICE([Using native FLAC decoder (RFC 9639 compliant, no external dependencies)])
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

if test "x$enable_aac" = "xyes"; then
  PKG_CHECK_MODULES([AAC], [faad2], [have_aac=yes], [have_aac=no])
  if test "x$have_aac" = "xyes"; then
    AC_DEFINE([HAVE_AAC], [1], [Define to 1 if AAC support is enabled])
  else
    AC_MSG_WARN([AAC support disabled: faad2 not found])
  fi
else
  have_aac=no
fi

if test "x$enable_g722" = "xyes"; then
  PKG_CHECK_MODULES([G722], [spandsp], [have_g722=yes], [have_g722=no])
  if test "x$have_g722" = "xyes"; then
    AC_DEFINE([HAVE_G722], [1], [Define to 1 if G.722 support is enabled])
  else
    AC_MSG_WARN([G.722 support disabled: spandsp not found])
  fi
else
  have_g722=no
fi

# Check for Speex (Ogg container required)
AC_ARG_ENABLE([speex],
  [AS_HELP_STRING([--enable-speex], [Enable Speex support @<:@default=yes@:>@])],
  [enable_speex=$enableval], [enable_speex=yes])

have_speex=no
if test "x$enable_speex" = "xyes"; then
  if test "x$have_ogg" = "xyes"; then
    PKG_CHECK_MODULES([SPEEX], [speex >= 1.2], [have_speex=yes], [have_speex=no])
    if test "x$have_speex" = "xyes"; then
      AC_DEFINE([HAVE_SPEEX], [1], [Define to 1 if Speex support is enabled])
    fi
  else
    have_speex=no
    AC_MSG_WARN([Speex support disabled: libogg not found])
  fi
else
  have_speex=no
fi

# Determine if OggDemuxer should be built
# OggDemuxer is built if:
# - Both Vorbis and Opus are enabled, OR
# - Only Vorbis is enabled, OR  
# - Only Opus is enabled, OR
# - Speex is enabled, OR
# - FLAC is enabled and libogg is available (for Ogg FLAC support)
have_oggdemuxer=no
if test "x$have_vorbis" = "xyes" || test "x$have_opus" = "xyes" || test "x$have_speex" = "xyes"; then
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
fi

# G711 codec option (enables both μ-law and A-law)
AC_ARG_ENABLE([g711],
  [AS_HELP_STRING([--enable-g711], [Enable G.711 codecs (μ-law and A-law) @<:@default=auto@:])],
  [enable_g711=$enableval],
  [enable_g711=auto])

# Determine G711 support
if test "x$enable_g711" = "xno"; then
  have_g711=no
  have_alaw=no
  have_mulaw=no
else
  have_g711=yes
  # Enable both codecs by default
  have_alaw=yes
  have_mulaw=yes
fi

# Define codec macros based on final have_alaw/have_mulaw values
if test "x$have_alaw" = "xyes"; then
  AC_DEFINE([ENABLE_ALAW_CODEC], [1], [Define to 1 if A-law codec support is enabled])
fi
if test "x$have_mulaw" = "xyes"; then
  AC_DEFINE([ENABLE_MULAW_CODEC], [1], [Define to 1 if μ-law codec support is enabled])
fi

AM_CONDITIONAL([HAVE_G711], [test "x$have_g711" = "xyes"])
AM_CONDITIONAL([HAVE_FLAC], [test "x$have_flac" = "xyes"])
AM_CONDITIONAL([HAVE_VORBIS], [test "x$have_vorbis" = "xyes"])
AM_CONDITIONAL([HAVE_OPUS], [test "x$have_opus" = "xyes"])
AM_CONDITIONAL([HAVE_AAC], [test "x$have_aac" = "xyes"])
AM_CONDITIONAL([HAVE_G722], [test "x$have_g722" = "xyes"])
AM_CONDITIONAL([HAVE_OGG], [test "x$have_ogg" = "xyes"])
AM_CONDITIONAL([HAVE_OGGDEMUXER], [test "x$have_oggdemuxer" = "xyes"])
AM_CONDITIONAL([HAVE_SPEEX], [test "x$have_speex" = "xyes"])
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

# Native open-file dialog backend for the "I"/"L" runtime track loader (optional).
# Build-time selection, preferring Qt over GTK and newer over older:
#   Qt6 -> Qt5 -> Qt4 -> Qt3 -> GTK4 -> GTK+3 -> GTK+2 ; Win32 on Windows.
# If nothing is found the feature compiles out and the I/L keys are unbound.
AC_ARG_ENABLE([filedialog],
  [AS_HELP_STRING([--disable-filedialog], [Disable the native open-file dialog / I+L keys @<:@default=auto@:>@])],
  [enable_filedialog=$enableval], [enable_filedialog=auto])
AC_ARG_WITH([qt3-dir],
  [AS_HELP_STRING([--with-qt3-dir=DIR], [Qt3 install prefix for the file dialog (enables the Qt3 backend)])],
  [with_qt3_dir=$withval], [with_qt3_dir=no])

have_filedialog=no
filedialog_backend=none
psymp3_qt_major=
psymp3_gtk_major=

if test "x$enable_filedialog" != "xno"; then
  case "$host_os" in
    mingw*|msys*|cygwin*|*windows*)
      have_filedialog=yes
      filedialog_backend="Win32"
      DIALOG_LIBS="-lcomdlg32 -lole32 -lshell32"
      AC_DEFINE([PSYMP3_FILEDIALOG_WIN32], [1], [Use the Win32 comdlg32 open-file dialog])
      ;;
    *)
      # Pick the first available toolkit (existence-only probes; no side effects).
      dialog_pkg=
      PKG_CHECK_EXISTS([Qt6Widgets], [dialog_pkg="Qt6Widgets"; filedialog_backend="Qt6"; psymp3_qt_major=6])
      if test -z "$dialog_pkg"; then
        PKG_CHECK_EXISTS([Qt5Widgets], [dialog_pkg="Qt5Widgets"; filedialog_backend="Qt5"; psymp3_qt_major=5])
      fi
      if test -z "$dialog_pkg"; then
        PKG_CHECK_EXISTS([QtGui], [dialog_pkg="QtGui"; filedialog_backend="Qt4"; psymp3_qt_major=4])
      fi
      # Qt3 has no pkg-config; opt in explicitly with --with-qt3-dir=PREFIX.
      if test -z "$dialog_pkg" && test "x$with_qt3_dir" != "xno"; then
        if test -f "$with_qt3_dir/include/qfiledialog.h"; then
          have_filedialog=yes
          filedialog_backend="Qt3"
          psymp3_qt_major=3
          DIALOG_CFLAGS="-I$with_qt3_dir/include"
          DIALOG_LIBS="-L$with_qt3_dir/lib -lqt-mt"
        else
          AC_MSG_WARN([--with-qt3-dir given but $with_qt3_dir/include/qfiledialog.h not found; ignoring])
        fi
      fi
      if test -z "$dialog_pkg" && test "x$have_filedialog" = "xno"; then
        PKG_CHECK_EXISTS([gtk4], [dialog_pkg="gtk4"; filedialog_backend="GTK4"; psymp3_gtk_major=4])
      fi
      if test -z "$dialog_pkg" && test "x$have_filedialog" = "xno"; then
        PKG_CHECK_EXISTS([gtk+-3.0], [dialog_pkg="gtk+-3.0"; filedialog_backend="GTK+3"; psymp3_gtk_major=3])
      fi
      if test -z "$dialog_pkg" && test "x$have_filedialog" = "xno"; then
        PKG_CHECK_EXISTS([gtk+-2.0], [dialog_pkg="gtk+-2.0"; filedialog_backend="GTK+2"; psymp3_gtk_major=2])
      fi
      # Pull in the chosen toolkit's flags (single PKG_CHECK_MODULES on the winner).
      if test -n "$dialog_pkg"; then
        PKG_CHECK_MODULES([DIALOG], [$dialog_pkg], [have_filedialog=yes], [have_filedialog=no])
      fi
      if test "x$have_filedialog" = "xyes"; then
        if test -n "$psymp3_qt_major"; then
          AC_DEFINE([PSYMP3_FILEDIALOG_QT], [1], [Use a Qt QFileDialog backend])
          AC_DEFINE_UNQUOTED([PSYMP3_QT_MAJOR], [$psymp3_qt_major], [Qt major version for the file dialog])
        elif test -n "$psymp3_gtk_major"; then
          AC_DEFINE([PSYMP3_FILEDIALOG_GTK], [1], [Use a GTK file-chooser backend])
          AC_DEFINE_UNQUOTED([PSYMP3_GTK_MAJOR], [$psymp3_gtk_major], [GTK major version for the file dialog])
        fi
      fi
      ;;
  esac

  if test "x$have_filedialog" = "xyes"; then
    AC_DEFINE([HAVE_FILEDIALOG], [1], [Define to 1 if a native open-file dialog backend is available])
  else
    if test "x$enable_filedialog" = "xyes"; then
      AC_MSG_ERROR([--enable-filedialog requested but no Qt (6/5/4/3) or GTK (4/3/2) toolkit was found])
    fi
    AC_MSG_WARN([No file-dialog toolkit found; the I/L track-loader keys will be disabled])
  fi
fi

AC_SUBST([DIALOG_CFLAGS])
AC_SUBST([DIALOG_LIBS])
AM_CONDITIONAL([HAVE_FILEDIALOG], [test "x$have_filedialog" = "xyes"])

# RapidCheck property-based testing library (optional)
AC_ARG_ENABLE([rapidcheck],
  [AS_HELP_STRING([--enable-rapidcheck], [Enable RapidCheck property-based testing @<:@default=auto@:>@])],
  [enable_rapidcheck=$enableval],
  [enable_rapidcheck=auto])

have_rapidcheck=no
if test "x$enable_rapidcheck" != "xno"; then
  # Check for RapidCheck headers and library
  # RapidCheck is a C++ library, so we need to use C++ compiler for the check
  AC_LANG_PUSH([C++])
  AC_CHECK_HEADERS([rapidcheck.h], [have_rapidcheck_headers=yes], [have_rapidcheck_headers=no])
  if test "x$have_rapidcheck_headers" = "xyes"; then
    AC_CHECK_LIB([rapidcheck], [main], [have_rapidcheck=yes], [have_rapidcheck=no])
  fi
  AC_LANG_POP([C++])
  
  if test "x$have_rapidcheck" = "xyes"; then
    AC_DEFINE([HAVE_RAPIDCHECK], [1], [Define to 1 if RapidCheck is available])
    RAPIDCHECK_LIBS="-lrapidcheck"
    AC_SUBST([RAPIDCHECK_LIBS])
  else
    if test "x$enable_rapidcheck" = "xyes"; then
      case "$host_os" in
        freebsd*|dragonfly*|openbsd*|netbsd*|minix*)
          AC_MSG_ERROR([RapidCheck was requested but not found. Install devel/rapidcheck or use --disable-rapidcheck])
          ;;
        *)
          AC_MSG_ERROR([RapidCheck was requested but not found. Install librapidcheck-dev or use --disable-rapidcheck])
          ;;
      esac
    fi
  fi
fi

AM_CONDITIONAL([HAVE_RAPIDCHECK], [test "x$have_rapidcheck" = "xyes"])

# Test harness configuration
# Detect Windows builds and final builds and disable testing by default
AC_ARG_ENABLE([test-harness],
  [AS_HELP_STRING([--enable-test-harness], [Build test harness @<:@default=yes on Unix, no on Windows/final@:>@])],
  [enable_test_harness=$enableval],
  [
    # Default: enable on Unix/Linux, disable on Windows and final builds
    # Windows builds have issues with SDL_main wrapper in test programs
    # Final builds don't build static libraries that tests depend on
    case "$host_os" in
      mingw* | msys* | cygwin*)
        enable_test_harness=no
        AC_MSG_NOTICE([Windows build detected - test harness disabled by default])
        ;;
      *)
        if test "x$enable_final" = "xyes"; then
          enable_test_harness=no
          AC_MSG_NOTICE([Final build mode - test harness disabled (incompatible with unity build)])
        else
          enable_test_harness=yes
        fi
        ;;
    esac
  ])

AM_CONDITIONAL([BUILD_TEST_HARNESS], [test "x$enable_test_harness" = "xyes"])

# Sanitizer support for security testing
AC_ARG_ENABLE([asan],
  [AS_HELP_STRING([--enable-asan], [Enable AddressSanitizer for memory error detection @<:@default=no@:>@])],
  [enable_asan=$enableval],
  [enable_asan=no])

AC_ARG_ENABLE([ubsan],
  [AS_HELP_STRING([--enable-ubsan], [Enable UndefinedBehaviorSanitizer for undefined behavior detection @<:@default=no@:>@])],
  [enable_ubsan=$enableval],
  [enable_ubsan=no])

AC_ARG_ENABLE([tsan],
  [AS_HELP_STRING([--enable-tsan], [Enable ThreadSanitizer for thread safety detection @<:@default=no@:>@])],
  [enable_tsan=$enableval],
  [enable_tsan=no])

# Apply sanitizer flags
SANITIZER_FLAGS=""

if test "x$enable_asan" = "xyes"; then
  SANITIZER_FLAGS="$SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer"
  AC_DEFINE([ENABLE_ASAN], [1], [Define to 1 if AddressSanitizer is enabled])
  AC_MSG_NOTICE([AddressSanitizer enabled])
fi

if test "x$enable_ubsan" = "xyes"; then
  SANITIZER_FLAGS="$SANITIZER_FLAGS -fsanitize=undefined -fno-omit-frame-pointer"
  AC_DEFINE([ENABLE_UBSAN], [1], [Define to 1 if UndefinedBehaviorSanitizer is enabled])
  AC_MSG_NOTICE([UndefinedBehaviorSanitizer enabled])
fi

if test "x$enable_tsan" = "xyes"; then
  SANITIZER_FLAGS="$SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer"
  AC_DEFINE([ENABLE_TSAN], [1], [Define to 1 if ThreadSanitizer is enabled])
  AC_MSG_NOTICE([ThreadSanitizer enabled])
fi

if test -n "$SANITIZER_FLAGS"; then
  CXXFLAGS="$CXXFLAGS $SANITIZER_FLAGS"
  CFLAGS="$CFLAGS $SANITIZER_FLAGS"
  LDFLAGS="$LDFLAGS $SANITIZER_FLAGS"
  AC_SUBST([CXXFLAGS])
  AC_SUBST([CFLAGS])
  AC_SUBST([LDFLAGS])
fi

# Security Hardening Flags
# Compile-side hardening (-fstack-protector-strong, -fPIE, -D_FORTIFY_SOURCE=2)
# is carried in PSYMP3_CXXFLAGS (see above) and applied via each AM_CXXFLAGS so
# it survives a user CXXFLAGS override. Link-side hardening stays here.
# ELF uses -z relro/-z now (+ -pie); the mingw PE linker rejects -z ("ld:
# unrecognized option '-z'"), so use the PE equivalents (ASLR/DEP) there.
case "$host_os" in
  mingw*|msys*|cygwin*)
    LDFLAGS="$LDFLAGS -Wl,--dynamicbase,--nxcompat"
    ;;
  *)
    LDFLAGS="$LDFLAGS -Wl,-z,relro,-z,now"
    if test "x$enable_pie" = "xyes"; then
      LDFLAGS="$LDFLAGS -pie"
    fi
    ;;
esac

AM_CONDITIONAL([ENABLE_ASAN], [test "x$enable_asan" = "xyes"])
AM_CONDITIONAL([ENABLE_UBSAN], [test "x$enable_ubsan" = "xyes"])
AM_CONDITIONAL([ENABLE_TSAN], [test "x$enable_tsan" = "xyes"])

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

# Static Build Support
AC_ARG_ENABLE([static-binary],
  [AS_HELP_STRING([--enable-static-binary], [Enable static linking for the final binary @<:@default=no@:>@])],
  [enable_static_binary=$enableval],
  [enable_static_binary=no])

if test "x$enable_static_binary" = "xyes"; then
  AC_MSG_NOTICE([Static linking enabled])
  LDFLAGS="$LDFLAGS -static"
  
  # For Windows/MinGW, we might need additional flags or specific handling
  # but -static is usually the main switch for gcc/ld.
  case "$host_os" in
    *mingw*|*cygwin*|*msys*)
       # Ensure we don't have conflicting flags if necessary
       AC_DEFINE([TAGLIB_STATIC], [1], [Define to 1 for static TagLib linking])
       AC_DEFINE([FLAC__NO_DLL], [1], [Define to 1 for static FLAC linking])
       AC_DEFINE([CURL_STATICLIB], [1], [Define to 1 for static libcurl linking])
       CPPFLAGS="$CPPFLAGS -DTAGLIB_STATIC -DFLAC__NO_DLL -DCURL_STATICLIB"
       ;;
  esac
fi

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

# Large-file offsets on Windows. AC_SYS_LARGEFILE records _FILE_OFFSET_BITS in
# config.h, but on mingw off_t only widens to 64-bit when the macro is defined
# on the command line, before any system header. Under llvm-mingw/clang the
# config.h define lands too late and off_t stays 32-bit (long, LLP64), tripping
# the IOHandler filesize_t static_assert. Force it on the command line for mingw.
case "$host_os" in
  *mingw*|*cygwin*|*msys*)
    CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64"
    ;;
esac

# Core dependencies check for final validation
# SDL: Try pkg-config first, fall back to sdl-config (already checked above, just validate others)
PKG_CHECK_MODULES([DEPENDS], [taglib >= 1.6 openssl >= 1.0 libcurl >= 7.20.0])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile src/Makefile src/codecs/Makefile src/codecs/pcm/Makefile src/codecs/flac/Makefile src/codecs/opus/Makefile src/codecs/vorbis/Makefile src/codecs/mp3/Makefile src/codecs/mp2/Makefile src/codecs/alac/Makefile src/codecs/aac/Makefile src/lastfm/Makefile src/mpris/Makefile src/io/Makefile src/io/file/Makefile src/io/http/Makefile src/demuxer/Makefile src/demuxer/iso/Makefile src/demuxer/riff/Makefile src/demuxer/raw/Makefile src/demuxer/flac/Makefile src/demuxer/ogg/Makefile src/demuxer/mp3/Makefile src/tag/Makefile src/widget/Makefile src/widget/foundation/Makefile src/widget/windowing/Makefile src/widget/ui/Makefile src/core/Makefile src/core/utility/Makefile src/core/compression/Makefile res/Makefile tests/Makefile])

# Print configuration summary
echo ""
echo "=========================================="
echo "PsyMP3 Configuration Summary"
echo "=========================================="
echo ""
echo "Core Dependencies:"
echo "  SDL:           yes"
echo "  TagLib:        yes"
echo "  FreeType2:     yes"
echo "  OpenSSL:       yes"
echo "  libcurl:       yes"
echo ""
echo "Audio Codec Support:"
echo "  MP3 (minimp3):     yes (bundled, no external dependency)"
echo "  FLAC:              $have_flac"
if test "x$have_flac" = "xyes"; then
  echo "    Implementation:  Native"
fi
echo "  Vorbis:            $have_vorbis"
echo "  Opus:              $have_opus"
echo "  AAC (faad2):       $have_aac"
echo "  G.722 (spandsp):   $have_g722"
echo "  Speex:             $have_speex"
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
echo "  File dialog (I/L): $filedialog_backend"
echo "  Test Harness:      $enable_test_harness"
echo "  RapidCheck (PBT):  $have_rapidcheck"
echo "  Release Build:     $enable_release"
echo "  Final Build:       $enable_final"
echo ""
echo "Security Testing:"
echo "  AddressSanitizer:  $enable_asan"
echo "  UBSanitizer:       $enable_ubsan"
echo "  ThreadSanitizer:   $enable_tsan"
echo ""
echo "=========================================="
echo ""

AC_OUTPUT
AM_CONDITIONAL([HAVE_NATIVE_FLAC], [test "x$have_native_flac" = "xyes"])
