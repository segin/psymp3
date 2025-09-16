#!/bin/bash

# run_iso_performance_tests.sh - Run ISO demuxer performance optimization tests
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>

set -e

echo "Building ISO Demuxer Performance Tests..."
echo "========================================"

# Build the test
cd "$(dirname "$0")"
cd ../src

# Compile the performance test
g++ -DHAVE_CONFIG_H -I. -I../include \
    -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT \
    -I/usr/include/taglib -I/usr/include/freetype2 -I/usr/include/libpng16 \
    -I/usr/include/x86_64-linux-gnu \
    -DPSYMP3_DATADIR='"/usr/local/share/psymp3/data"' \
    -Wall -g -O2 \
    -I/usr/include/x86_64-linux-gnu \
    -I/usr/include/opus \
    -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
    -o ../tests/test_iso_performance_optimization \
    ../tests/test_iso_performance_optimization.cpp \
    ISODemuxerSampleTableManager.o \
    MemoryTracker.o MemoryOptimizer.o MemoryPoolManager.o \
    debug.o \
    -lpthread -lm

echo ""
echo "Running ISO Demuxer Performance Tests..."
echo "========================================"

cd ../tests
./test_iso_performance_optimization

echo ""
echo "Performance tests completed!"