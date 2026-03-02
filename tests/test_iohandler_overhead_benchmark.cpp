#include "psymp3.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <numeric>
#include <thread>

// Using namespaces to access classes
using namespace PsyMP3::IO;
using namespace PsyMP3::IO::File;

int main() {
    std::cout << "Starting FileIOHandler read overhead benchmark..." << std::endl;

    // Create a dummy file
    std::string filename = "bench_overhead.dat";
    size_t file_size = 10 * 1024 * 1024; // 10 MB
    {
        std::ofstream f(filename, std::ios::binary);
        std::vector<char> buf(4096, 'X');
        for (size_t i = 0; i < file_size / buf.size(); ++i) {
            f.write(buf.data(), buf.size());
        }
    }

    try {
        FileIOHandler handler{TagLib::String(filename)};

        // Warm up
        std::vector<char> buffer(64);
        handler.read(buffer.data(), 1, buffer.size());
        handler.seek(0, SEEK_SET);

        // Benchmark small reads (high overhead scenario)
        size_t read_size = 64; // Small reads to amplify overhead
        size_t iterations = 100000;

        std::cout << "Running " << iterations << " reads of " << read_size << " bytes..." << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < iterations; ++i) {
            size_t bytes = handler.read(buffer.data(), 1, read_size);
            if (bytes < 1) { // 1 element of read_size
                handler.seek(0, SEEK_SET);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        double ms = duration / 1000.0;
        std::cout << "Time: " << ms << " ms" << std::endl;

        double ops_per_sec = (double)iterations / (duration / 1000000.0);
        std::cout << "Operations per second: " << ops_per_sec << std::endl;
        std::cout << "Average latency: " << (double)duration / iterations << " us" << std::endl;

        // EOF Benchmark
        std::cout << "Running EOF benchmark..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        size_t eof_iterations = 1000000;
        for (size_t i = 0; i < eof_iterations; ++i) {
            volatile bool is_eof = handler.eof();
            (void)is_eof;
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        std::cout << "Time for " << eof_iterations << " eof() calls: " << duration / 1000.0 << " ms" << std::endl;
        std::cout << "Average eof() latency: " << (double)duration / eof_iterations << " us" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    std::remove(filename.c_str());
    return 0;
}
