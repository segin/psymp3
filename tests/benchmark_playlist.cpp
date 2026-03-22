#include <iostream>
#include <vector>
#include <string>
#include <chrono>

class DummyPlayer {
public:
    void playlistPopulatorLoopValue(std::vector<std::string> args) {
        // Prevent optimization
        volatile size_t s = 0;
        for (const auto& arg : args) {
            s += arg.size();
        }
        (void)s;
    }

    void playlistPopulatorLoopConstRef(const std::vector<std::string>& args) {
        // Prevent optimization
        volatile size_t s = 0;
        for (const auto& arg : args) {
            s += arg.size();
        }
        (void)s;
    }
};

int main() {
    std::vector<std::string> test_args;
    // 10000 long strings to force multiple allocations and copying overheads
    for (int i = 0; i < 10000; ++i) {
        test_args.push_back("a_reasonably_long_string_that_wont_fit_in_the_small_string_optimization_buffer_" + std::to_string(i));
    }

    DummyPlayer player;

    int iterations = 1000;

    auto start_val = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        player.playlistPopulatorLoopValue(test_args);
    }
    auto end_val = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> val_duration = end_val - start_val;

    auto start_ref = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        player.playlistPopulatorLoopConstRef(test_args);
    }
    auto end_ref = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ref_duration = end_ref - start_ref;

    std::cout << "Baseline (Pass by Value):      " << val_duration.count() << " ms\n";
    std::cout << "Optimized (Pass by Const Ref): " << ref_duration.count() << " ms\n";

    double improvement = val_duration.count() - ref_duration.count();
    double percent = (improvement / val_duration.count()) * 100.0;
    std::cout << "Improvement:                   " << improvement << " ms (" << percent << "%)\n";

    return 0;
}
