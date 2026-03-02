#!/bin/bash
set -e

# Compile BufferPool.cpp
# We use -Imock_headers before -Iinclude so that our mock psymp3.h is used instead of the real one
g++ -c src/io/BufferPool.cpp -Imock_headers -Iinclude -std=c++17 -o BufferPool.o

# Compile rect.cpp (required by TestFramework)
g++ -c src/core/rect.cpp -Imock_headers -Iinclude -std=c++17 -o rect.o

# Compile and link test
g++ -std=c++17 \
    tests/test_io_buffer_pool.cpp \
    tests/mock_debug.cpp \
    tests/mock_iohandler.cpp \
    tests/test_framework.cpp \
    tests/test_rect_utilities.cpp \
    BufferPool.o \
    rect.o \
    -Imock_headers -Iinclude -Itests \
    -o tests/test_io_buffer_pool_bin \
    -lpthread

# Run
echo "Running test_io_buffer_pool_bin..."
./tests/test_io_buffer_pool_bin
