#include <iostream>
#include <vector>
#include <chrono>

// Mock TagLib::String
namespace TagLib {
    class String {
    public:
        String() : m_str("") {}
        String(const char* str) : m_str(str) {}
        String(const String& other) : m_str(other.m_str) {}
        String& operator=(const String& other) { m_str = other.m_str; return *this; }

        // Let's add some artificial delay/allocation to make copying measurable
        String(const std::string& str) : m_str(str) {
            // Force some allocation to simulate actual complex string behavior
            volatile char* buf = new char[100];
            for(int i=0; i<100; ++i) buf[i] = 'a';
            delete[] buf;
        }
    private:
        std::string m_str;
    };
}

// Mock Stream
class Stream {
public:
    Stream() {}
    virtual ~Stream() {}
};

// Target classes to test
class ChainedStreamOld : public Stream {
public:
    ChainedStreamOld(std::vector<TagLib::String> paths)
        : m_paths(std::move(paths)) {}
    std::vector<TagLib::String> m_paths;
};

class ChainedStreamNew : public Stream {
public:
    ChainedStreamNew(const std::vector<TagLib::String>& paths)
        : m_paths(paths) {}
    std::vector<TagLib::String> m_paths;
};

int main() {
    const int num_iterations = 10000;
    const int num_paths = 100;

    std::vector<TagLib::String> test_paths;
    for (int i = 0; i < num_paths; ++i) {
        test_paths.push_back(TagLib::String("test_path_" + std::to_string(i)));
    }

    // Benchmark Old
    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ChainedStreamOld stream(test_paths); // Let compiler copy it naturally
    }
    auto end_old = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_old = end_old - start_old;

    // Benchmark New
    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ChainedStreamNew stream(test_paths);
    }
    auto end_new = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_new = end_new - start_new;

    std::cout << "Baseline (pass by value + move): " << duration_old.count() << " ms\n";
    std::cout << "Optimized (pass by const ref): " << duration_new.count() << " ms\n";
    std::cout << "Improvement: " << (duration_old.count() - duration_new.count()) / duration_old.count() * 100 << "%\n";

    return 0;
}
