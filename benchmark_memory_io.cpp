#include "psymp3.h"
#include "io/MemoryIOHandler.h"
#include <iostream>
#include <vector>
#include <chrono>

using namespace PsyMP3::IO;

int main() {
    MemoryIOHandler handler;
    const size_t num_iterations = 50000;
    const size_t chunk_size = 4096;

    std::vector<uint8_t> chunk(chunk_size, 0);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_iterations; ++i) {
        // Write a chunk
        handler.write(chunk.data(), chunk.size());

        // Read half the chunk
        std::vector<uint8_t> read_buffer(chunk_size / 2);
        handler.read(read_buffer.data(), 1, read_buffer.size());

        // Discard what we read
        handler.discardRead();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;

    return 0;
}
