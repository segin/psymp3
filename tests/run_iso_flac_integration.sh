#!/bin/bash

# Integration test script for FLAC-in-MP4 support
# This file is part of PsyMP3.

set -e

echo "Building FLAC-in-MP4 integration test..."

# Compile the integration test
g++ -std=c++17 -I../include -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT \
    -I/usr/include/taglib -I/usr/include/freetype2 -I/usr/include/libpng16 \
    -I/usr/include/x86_64-linux-gnu -DPSYMP3_DATADIR='"/usr/local/share/psymp3/data"' \
    -Wall -g -O2 -I/usr/include/x86_64-linux-gnu -I/usr/include/opus \
    -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
    -o test_iso_flac_integration test_iso_flac_integration.cpp \
    ../src/ISODemuxer.o ../src/ISODemuxerBoxParser.o ../src/ISODemuxerSampleTableManager.o \
    ../src/ISODemuxerFragmentHandler.o ../src/ISODemuxerMetadataExtractor.o \
    ../src/ISODemuxerStreamManager.o ../src/ISODemuxerSeekingEngine.o \
    ../src/ISODemuxerErrorRecovery.o ../src/ISODemuxerComplianceValidator.o \
    ../src/FileIOHandler.o ../src/IOHandler.o ../src/Demuxer.o \
    ../src/MemoryTracker.o ../src/debug.o \
    -lSDL -ltag -lfreetype -lpng -lxml2 -lz -lm -ldl -lpthread

echo "Running FLAC-in-MP4 integration test..."
./test_iso_flac_integration

echo "FLAC-in-MP4 integration test completed successfully!"