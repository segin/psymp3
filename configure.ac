AC_PREREQ([2.69])
AC_INIT([PsyMP3],[2-CURRENT],[segin2005@gmail.com],[psymp3],[https://github.com/segin/psymp3])
AM_INIT_AUTOMAKE([1.10 subdir-objects])

# Enable host detection (populates $host_cpu)
AC_CANONICAL_HOST

AU_DEFUN([AX_REQUIRE_CPP], [ax_require_cpp(std_17, reject)])

AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_HEADER_STDBOOL
AC_FUNC_MALLOC
AX_CXX_COMPILE_STDCXX_17

# Check for -Wno-psabi (suppress GCC 7.1 ABI warnings on ARM)
AC_MSG_CHECKING([if compiler supports -Wno-psabi])
OLD_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -Wno-psabi"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])], [
  AC_MSG_RESULT([yes])
  # Keep -Wno-psabi in CXXFLAGS
], [
  AC_MSG_RESULT([no])
  CXXFLAGS="$OLD_CXXFLAGS"
])


# Check for atomic library
# Explicitly check for __atomic_compare_exchange_8 (64-bit atomic CAS)
# Check for atomic library
# Explicitly link -latomic on ARM architectures where it's often needed for 64-bit atomics
atomic_linked="no"
case "$host_cpu" in
  arm*)
    AC_MSG_NOTICE([ARM architecture detected: appending -latomic to LIBS])
    LIBS="$LIBS -latomic"
    atomic_linked="yes (forced for ARM)"
    ;;
esac

AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

AC_CHECK_FUNCS([atexit getcwd memset])

# ... (omitted lines) ...

echo "Optional Features:"
echo "  MPRIS (D-Bus):     $have_dbus"
echo "  Test Harness:      $enable_test_harness"
echo "  RapidCheck (PBT):  $have_rapidcheck"
echo "  Atomic Linking:    $atomic_linked"
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