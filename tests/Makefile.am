#
# tests/Makefile.am - automake input for test directory
# This file is part of PsyMP3.
# Copyright © 2011 Kirn Gill <segin2005@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any purpose
# with or without fee is hereby granted, provided that the above copyright
# notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
# OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
# DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
# ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

SUBDIRS = .

# Common flags for all test programs
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS)
AM_LDFLAGS = $(SDL_LIBS) $(TAGLIB_LIBS) $(FREETYPE_LIBS)

# Test harness executable
if BUILD_TEST_HARNESS
bin_PROGRAMS = test-harness
test_harness_SOURCES = test_harness.cpp test_discovery.cpp test_executor.cpp test_reporter.cpp
test_harness_CPPFLAGS = $(AM_CPPFLAGS)
test_harness_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)
endif

# Individual test executables
check_PROGRAMS = \
	test_rect_area_validation \
	test_rect_containment \
	test_rect_intersection \
	test_rect_union \
	test_rect_centering \
	test_rect_centering_overflow \
	test_rect_expansion \
	test_rect_transformation \
	test_rect_normalization \
	test_rect_modern_cpp

# Test program sources and dependencies
test_rect_area_validation_SOURCES = test_rect_area_validation.cpp
test_rect_area_validation_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_containment_SOURCES = test_rect_containment.cpp
test_rect_containment_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_intersection_SOURCES = test_rect_intersection.cpp
test_rect_intersection_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_union_SOURCES = test_rect_union.cpp
test_rect_union_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_centering_SOURCES = test_rect_centering.cpp
test_rect_centering_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_centering_overflow_SOURCES = test_rect_centering_overflow.cpp
test_rect_centering_overflow_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_expansion_SOURCES = test_rect_expansion.cpp
test_rect_expansion_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_transformation_SOURCES = test_rect_transformation.cpp
test_rect_transformation_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_normalization_SOURCES = test_rect_normalization.cpp
test_rect_normalization_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

test_rect_modern_cpp_SOURCES = test_rect_modern_cpp.cpp
test_rect_modern_cpp_LDADD = $(top_builddir)/src/rect.o $(AM_LDFLAGS)

# Test execution target
check-local: $(check_PROGRAMS)
if BUILD_TEST_HARNESS
	./test-harness --verbose
else
	@echo "Running individual tests..."
	@for test in $(check_PROGRAMS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
endif

# Clean up test executables
CLEANFILES = $(check_PROGRAMS)
if BUILD_TEST_HARNESS
CLEANFILES += test-harness
endif

# Ensure rect.o is built before tests
$(check_PROGRAMS): $(top_builddir)/src/rect.o

$(top_builddir)/src/rect.o:
	$(MAKE) -C $(top_builddir)/src rect.o