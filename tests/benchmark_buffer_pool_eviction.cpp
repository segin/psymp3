#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

struct PoolEntry {
    std::mutex mutex;
    std::vector<uint8_t*> available_buffers;
    PoolEntry() {}
};

constexpr int ITERATIONS = 100000;

double run_baseline(std::map<size_t, std::shared_ptr<PoolEntry>>& m_pools) {
    auto start_baseline = std::chrono::high_resolution_clock::now();
    size_t total_evicted_baseline = 0;
    for (int it = 0; it < ITERATIONS; ++it) {
        std::vector<std::pair<size_t, double>> size_efficiency;
        for (const auto& pool_pair : m_pools) {
            size_efficiency.emplace_back(pool_pair.first, (pool_pair.first % 10) / 10.0);
        }

        for (size_t i = 0; i < size_efficiency.size(); ++i) {
            size_t buffer_size = size_efficiency[i].first;
            double efficiency = size_efficiency[i].second;

            if (efficiency < 0.3) {
                auto pool_it = m_pools.find(buffer_size);
                if (pool_it != m_pools.end()) {
                    total_evicted_baseline++;
                }
            }
        }
    }
    auto end_baseline = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end_baseline - start_baseline).count();
}

double run_optimized(std::map<size_t, std::shared_ptr<PoolEntry>>& m_pools) {
    auto start_optimized = std::chrono::high_resolution_clock::now();
    size_t total_evicted_optimized = 0;
    for (int it = 0; it < ITERATIONS; ++it) {
        struct EfficiencyData {
            size_t size;
            double efficiency;
            PoolEntry* entry;
        };
        std::vector<EfficiencyData> size_efficiency;
        size_efficiency.reserve(m_pools.size());

        for (const auto& pool_pair : m_pools) {
            size_efficiency.push_back({pool_pair.first, (pool_pair.first % 10) / 10.0, pool_pair.second.get()});
        }

        for (size_t i = 0; i < size_efficiency.size(); ++i) {
            size_t buffer_size = size_efficiency[i].size;
            double efficiency = size_efficiency[i].efficiency;

            if (efficiency < 0.3) {
                if (size_efficiency[i].entry) {
                    total_evicted_optimized++;
                }
            }
        }
    }
    auto end_optimized = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end_optimized - start_optimized).count();
}

int main() {
    std::map<size_t, std::shared_ptr<PoolEntry>> m_pools;
    for (size_t i = 1; i <= 1000; ++i) {
        m_pools[i * 1024] = std::make_shared<PoolEntry>();
        m_pools[i * 1024]->available_buffers.resize(10, nullptr);
    }

    // Warmup
    run_baseline(m_pools);
    run_optimized(m_pools);

    double baseline_total = 0;
    double optimized_total = 0;

    for(int i=0; i<5; i++) {
        baseline_total += run_baseline(m_pools);
        optimized_total += run_optimized(m_pools);
    }

    std::cout << "Baseline (avg): " << (baseline_total / 5.0) << " us\n";
    std::cout << "Optimized (avg): " << (optimized_total / 5.0) << " us\n";
    std::cout << "Speedup: " << baseline_total / optimized_total << "x\n";

    return 0;
}
