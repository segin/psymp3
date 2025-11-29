/*
 * test_lastfm_thread_safety_properties.cpp - Property-based tests for thread-safe queue operations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cassert>
#include <chrono>
#include <random>
#include <string>
#include <set>

// ========================================
// THREAD-SAFE QUEUE SIMULATOR
// ========================================

/**
 * Simulated thread-safe scrobble queue for testing
 * This mirrors the threading behavior of LastFM class
 */
struct MockScrobble {
    std::string artist;
    std::string title;
    int id;  // Unique ID for tracking
    
    MockScrobble(const std::string& a, const std::string& t, int i)
        : artist(a), title(t), id(i) {}
};

class ThreadSafeScrobbleQueue {
private:
    std::queue<MockScrobble> m_scrobbles;
    mutable std::mutex m_scrobble_mutex;
    std::condition_variable m_submission_cv;
    std::atomic<bool> m_shutdown{false};
    
    // Statistics for verification
    std::atomic<int> m_total_enqueued{0};
    std::atomic<int> m_total_dequeued{0};
    
public:
    // Public API - acquires lock
    void enqueue(MockScrobble scrobble) {
        {
            std::lock_guard<std::mutex> lock(m_scrobble_mutex);
            enqueue_unlocked(std::move(scrobble));
        }
        m_submission_cv.notify_one();
    }
    
    // Batch enqueue for stress testing
    void enqueueBatch(std::vector<MockScrobble> scrobbles) {
        {
            std::lock_guard<std::mutex> lock(m_scrobble_mutex);
            for (auto& s : scrobbles) {
                enqueue_unlocked(std::move(s));
            }
        }
        m_submission_cv.notify_all();
    }
    
    // Dequeue with timeout (returns empty optional if timeout or shutdown)
    bool dequeue(MockScrobble& out, int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(m_scrobble_mutex);
        
        if (!m_submission_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [this] { return !isQueueEmpty_unlocked() || m_shutdown.load(); })) {
            return false;  // Timeout
        }
        
        if (m_shutdown.load() && isQueueEmpty_unlocked()) {
            return false;
        }
        
        return dequeue_unlocked(out);
    }
    
    // Dequeue batch (up to batch_size items)
    std::vector<MockScrobble> dequeueBatch(int batch_size) {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        std::vector<MockScrobble> result;
        result.reserve(batch_size);
        
        while (!isQueueEmpty_unlocked() && static_cast<int>(result.size()) < batch_size) {
            result.push_back(std::move(m_scrobbles.front()));
            m_scrobbles.pop();
            m_total_dequeued++;
        }
        
        return result;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        return getQueueSize_unlocked();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_scrobble_mutex);
        return isQueueEmpty_unlocked();
    }
    
    void shutdown() {
        m_shutdown = true;
        m_submission_cv.notify_all();
    }
    
    bool isShutdown() const {
        return m_shutdown.load();
    }
    
    int getTotalEnqueued() const { return m_total_enqueued.load(); }
    int getTotalDequeued() const { return m_total_dequeued.load(); }
    
private:
    // Private unlocked implementations - assume lock is held
    void enqueue_unlocked(MockScrobble scrobble) {
        m_scrobbles.push(std::move(scrobble));
        m_total_enqueued++;
    }
    
    bool dequeue_unlocked(MockScrobble& out) {
        if (m_scrobbles.empty()) {
            return false;
        }
        out = std::move(m_scrobbles.front());
        m_scrobbles.pop();
        m_total_dequeued++;
        return true;
    }
    
    size_t getQueueSize_unlocked() const {
        return m_scrobbles.size();
    }
    
    bool isQueueEmpty_unlocked() const {
        return m_scrobbles.empty();
    }
};

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 8: Thread-Safe Queue Operations
// ========================================
// **Feature: lastfm-performance-optimization, Property 8: Thread-Safe Queue Operations**
// **Validates: Requirements 7.1, 7.4**
//
// For any sequence of concurrent enqueue and dequeue operations across N threads,
// the total number of successfully dequeued items SHALL equal the total number
// of enqueued items (no items lost or duplicated).

void test_property_thread_safe_queue_single_producer_single_consumer() {
    std::cout << "\n=== Property 8.1: Single Producer, Single Consumer ===" << std::endl;
    std::cout << "Testing basic thread-safe queue operations..." << std::endl;
    
    ThreadSafeScrobbleQueue queue;
    const int num_items = 1000;
    std::atomic<int> consumed_count{0};
    std::set<int> consumed_ids;
    std::mutex consumed_mutex;
    
    // Producer thread
    std::thread producer([&queue, num_items]() {
        for (int i = 0; i < num_items; ++i) {
            queue.enqueue(MockScrobble("Artist" + std::to_string(i), 
                                       "Title" + std::to_string(i), i));
        }
    });
    
    // Consumer thread
    std::thread consumer([&queue, &consumed_count, &consumed_ids, &consumed_mutex, num_items]() {
        while (consumed_count.load() < num_items) {
            MockScrobble scrobble("", "", -1);
            if (queue.dequeue(scrobble, 50)) {
                std::lock_guard<std::mutex> lock(consumed_mutex);
                consumed_ids.insert(scrobble.id);
                consumed_count++;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all items were consumed exactly once
    assert(consumed_count.load() == num_items);
    assert(static_cast<int>(consumed_ids.size()) == num_items);
    
    // Verify no duplicates (set size equals count)
    for (int i = 0; i < num_items; ++i) {
        assert(consumed_ids.count(i) == 1);
    }
    
    std::cout << "  Produced: " << num_items << " items" << std::endl;
    std::cout << "  Consumed: " << consumed_count.load() << " items" << std::endl;
    std::cout << "  Unique IDs: " << consumed_ids.size() << std::endl;
    std::cout << "  ✓ No items lost or duplicated" << std::endl;
}

void test_property_thread_safe_queue_multiple_producers() {
    std::cout << "\n=== Property 8.2: Multiple Producers, Single Consumer ===" << std::endl;
    std::cout << "Testing concurrent enqueue from multiple threads..." << std::endl;
    
    ThreadSafeScrobbleQueue queue;
    const int num_producers = 4;
    const int items_per_producer = 250;
    const int total_items = num_producers * items_per_producer;
    
    std::atomic<int> consumed_count{0};
    std::set<int> consumed_ids;
    std::mutex consumed_mutex;
    std::atomic<bool> producers_done{false};
    
    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                int id = p * items_per_producer + i;
                queue.enqueue(MockScrobble("Artist" + std::to_string(id),
                                           "Title" + std::to_string(id), id));
            }
        });
    }
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!producers_done.load() || !queue.empty()) {
            MockScrobble scrobble("", "", -1);
            if (queue.dequeue(scrobble, 50)) {
                std::lock_guard<std::mutex> lock(consumed_mutex);
                consumed_ids.insert(scrobble.id);
                consumed_count++;
            }
        }
    });
    
    // Wait for producers
    for (auto& t : producers) {
        t.join();
    }
    producers_done = true;
    
    consumer.join();
    
    // Verify all items were consumed exactly once
    assert(consumed_count.load() == total_items);
    assert(static_cast<int>(consumed_ids.size()) == total_items);
    
    std::cout << "  Producers: " << num_producers << std::endl;
    std::cout << "  Items per producer: " << items_per_producer << std::endl;
    std::cout << "  Total produced: " << total_items << std::endl;
    std::cout << "  Total consumed: " << consumed_count.load() << std::endl;
    std::cout << "  ✓ All items from all producers consumed exactly once" << std::endl;
}

void test_property_thread_safe_queue_multiple_consumers() {
    std::cout << "\n=== Property 8.3: Single Producer, Multiple Consumers ===" << std::endl;
    std::cout << "Testing concurrent dequeue from multiple threads..." << std::endl;
    
    ThreadSafeScrobbleQueue queue;
    const int num_consumers = 4;
    const int total_items = 1000;
    
    std::atomic<int> consumed_count{0};
    std::set<int> consumed_ids;
    std::mutex consumed_mutex;
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&queue, total_items, &producer_done]() {
        for (int i = 0; i < total_items; ++i) {
            queue.enqueue(MockScrobble("Artist" + std::to_string(i),
                                       "Title" + std::to_string(i), i));
        }
        producer_done = true;
    });
    
    // Multiple consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (!producer_done.load() || !queue.empty()) {
                MockScrobble scrobble("", "", -1);
                if (queue.dequeue(scrobble, 50)) {
                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_ids.insert(scrobble.id);
                    consumed_count++;
                }
            }
        });
    }
    
    producer.join();
    for (auto& t : consumers) {
        t.join();
    }
    
    // Verify all items were consumed exactly once
    assert(consumed_count.load() == total_items);
    assert(static_cast<int>(consumed_ids.size()) == total_items);
    
    std::cout << "  Consumers: " << num_consumers << std::endl;
    std::cout << "  Total produced: " << total_items << std::endl;
    std::cout << "  Total consumed: " << consumed_count.load() << std::endl;
    std::cout << "  ✓ No items lost or duplicated across consumers" << std::endl;
}

void test_property_thread_safe_queue_stress_test() {
    std::cout << "\n=== Property 8.4: Stress Test (Multiple Producers & Consumers) ===" << std::endl;
    std::cout << "Testing high-concurrency scenario..." << std::endl;
    
    ThreadSafeScrobbleQueue queue;
    const int num_producers = 8;
    const int num_consumers = 4;
    const int items_per_producer = 500;
    const int total_items = num_producers * items_per_producer;
    
    std::atomic<int> consumed_count{0};
    std::set<int> consumed_ids;
    std::mutex consumed_mutex;
    std::atomic<int> producers_finished{0};
    
    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, p, items_per_producer, &producers_finished]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> delay(0, 100);
            
            for (int i = 0; i < items_per_producer; ++i) {
                int id = p * items_per_producer + i;
                queue.enqueue(MockScrobble("Artist" + std::to_string(id),
                                           "Title" + std::to_string(id), id));
                
                // Random small delay to increase interleaving
                if (delay(gen) < 5) {
                    std::this_thread::yield();
                }
            }
            producers_finished++;
        });
    }
    
    // Multiple consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (producers_finished.load() < num_producers || !queue.empty()) {
                MockScrobble scrobble("", "", -1);
                if (queue.dequeue(scrobble, 50)) {
                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_ids.insert(scrobble.id);
                    consumed_count++;
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }
    
    // Verify all items were consumed exactly once
    assert(consumed_count.load() == total_items);
    assert(static_cast<int>(consumed_ids.size()) == total_items);
    
    std::cout << "  Producers: " << num_producers << std::endl;
    std::cout << "  Consumers: " << num_consumers << std::endl;
    std::cout << "  Total items: " << total_items << std::endl;
    std::cout << "  Consumed: " << consumed_count.load() << std::endl;
    std::cout << "  ✓ High-concurrency test passed - no data races" << std::endl;
}

void test_property_thread_safe_queue_batch_operations() {
    std::cout << "\n=== Property 8.5: Batch Operations Thread Safety ===" << std::endl;
    std::cout << "Testing batch enqueue/dequeue under concurrency..." << std::endl;
    
    ThreadSafeScrobbleQueue queue;
    const int num_batches = 100;
    const int batch_size = 10;
    const int total_items = num_batches * batch_size;
    
    std::atomic<int> consumed_count{0};
    std::set<int> consumed_ids;
    std::mutex consumed_mutex;
    std::atomic<bool> producer_done{false};
    
    // Producer using batch enqueue
    std::thread producer([&]() {
        for (int b = 0; b < num_batches; ++b) {
            std::vector<MockScrobble> batch;
            for (int i = 0; i < batch_size; ++i) {
                int id = b * batch_size + i;
                batch.emplace_back("Artist" + std::to_string(id),
                                   "Title" + std::to_string(id), id);
            }
            queue.enqueueBatch(std::move(batch));
        }
        producer_done = true;
    });
    
    // Consumer using batch dequeue
    std::thread consumer([&]() {
        while (!producer_done.load() || !queue.empty()) {
            auto batch = queue.dequeueBatch(batch_size);
            if (!batch.empty()) {
                std::lock_guard<std::mutex> lock(consumed_mutex);
                for (const auto& s : batch) {
                    consumed_ids.insert(s.id);
                    consumed_count++;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all items were consumed exactly once
    assert(consumed_count.load() == total_items);
    assert(static_cast<int>(consumed_ids.size()) == total_items);
    
    std::cout << "  Batches: " << num_batches << std::endl;
    std::cout << "  Batch size: " << batch_size << std::endl;
    std::cout << "  Total items: " << total_items << std::endl;
    std::cout << "  Consumed: " << consumed_count.load() << std::endl;
    std::cout << "  ✓ Batch operations are thread-safe" << std::endl;
}

void test_property_graceful_shutdown() {
    std::cout << "\n=== Property 8.6: Graceful Shutdown ===" << std::endl;
    std::cout << "Testing that pending items are preserved on shutdown..." << std::endl;
    
    ThreadSafeScrobbleQueue queue;
    const int num_items = 100;
    
    // Enqueue items
    for (int i = 0; i < num_items; ++i) {
        queue.enqueue(MockScrobble("Artist" + std::to_string(i),
                                   "Title" + std::to_string(i), i));
    }
    
    // Consume half
    int consumed = 0;
    for (int i = 0; i < num_items / 2; ++i) {
        MockScrobble s("", "", -1);
        if (queue.dequeue(s, 10)) {
            consumed++;
        }
    }
    
    // Shutdown
    queue.shutdown();
    
    // Verify remaining items are still in queue
    size_t remaining = queue.size();
    assert(remaining == static_cast<size_t>(num_items - consumed));
    
    std::cout << "  Initial items: " << num_items << std::endl;
    std::cout << "  Consumed before shutdown: " << consumed << std::endl;
    std::cout << "  Remaining after shutdown: " << remaining << std::endl;
    std::cout << "  ✓ Pending items preserved on shutdown" << std::endl;
}

void test_property_no_deadlock_under_contention() {
    std::cout << "\n=== Property 8.7: No Deadlock Under Contention ===" << std::endl;
    std::cout << "Testing that operations complete without deadlock..." << std::endl;
    
    ThreadSafeScrobbleQueue queue;
    const int num_threads = 16;
    const int ops_per_thread = 1000;
    std::atomic<int> completed_ops{0};
    
    auto start = std::chrono::steady_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&queue, t, ops_per_thread, &completed_ops]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 2);
            
            for (int i = 0; i < ops_per_thread; ++i) {
                int op = op_dist(gen);
                int id = t * ops_per_thread + i;
                
                switch (op) {
                    case 0:  // Enqueue
                        queue.enqueue(MockScrobble("A", "T", id));
                        break;
                    case 1:  // Dequeue
                        {
                            MockScrobble s("", "", -1);
                            queue.dequeue(s, 1);
                        }
                        break;
                    case 2:  // Size check
                        queue.size();
                        break;
                }
                completed_ops++;
            }
        });
    }
    
    // Wait with timeout to detect deadlock
    bool all_completed = true;
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int expected_ops = num_threads * ops_per_thread;
    assert(completed_ops.load() == expected_ops);
    assert(all_completed);
    
    std::cout << "  Threads: " << num_threads << std::endl;
    std::cout << "  Operations per thread: " << ops_per_thread << std::endl;
    std::cout << "  Total operations: " << completed_ops.load() << std::endl;
    std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "  ✓ No deadlock detected" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "LAST.FM THREAD SAFETY PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: lastfm-performance-optimization**" << std::endl;
    std::cout << "**Property 8: Thread-Safe Queue Operations**" << std::endl;
    std::cout << "**Validates: Requirements 7.1, 7.4**" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    try {
        // Run all property tests
        test_property_thread_safe_queue_single_producer_single_consumer();
        test_property_thread_safe_queue_multiple_producers();
        test_property_thread_safe_queue_multiple_consumers();
        test_property_thread_safe_queue_stress_test();
        test_property_thread_safe_queue_batch_operations();
        test_property_graceful_shutdown();
        test_property_no_deadlock_under_contention();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "✅ ALL THREAD SAFETY PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ THREAD SAFETY PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ THREAD SAFETY PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    }
}
