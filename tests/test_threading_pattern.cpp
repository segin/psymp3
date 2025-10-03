/*
 * test_threading_pattern.cpp - Test public/private lock pattern implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <atomic>

/**
 * Example class demonstrating the public/private lock pattern
 */
class ThreadSafeCounter {
public:
    ThreadSafeCounter() : m_count(0) {}
    
    // Public methods acquire locks and call private unlocked versions
    void increment() {
        std::lock_guard<std::mutex> lock(m_mutex);
        increment_unlocked();
    }
    
    void decrement() {
        std::lock_guard<std::mutex> lock(m_mutex);
        decrement_unlocked();
    }
    
    int getValue() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return getValue_unlocked();
    }
    
    // This method demonstrates calling multiple operations atomically
    void incrementTwice() {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Call unlocked versions to avoid deadlock
        increment_unlocked();
        increment_unlocked();
    }
    
    // This method demonstrates reading and modifying atomically
    int getAndReset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        int current = getValue_unlocked();
        reset_unlocked();
        return current;
    }
    
private:
    // Private unlocked versions - assume locks are already held
    void increment_unlocked() {
        m_count++;
    }
    
    void decrement_unlocked() {
        m_count--;
    }
    
    int getValue_unlocked() const {
        return m_count;
    }
    
    void reset_unlocked() {
        m_count = 0;
    }
    
    mutable std::mutex m_mutex;
    int m_count;
};

/**
 * Test that the threading pattern works correctly under concurrent access
 */
void test_threading_pattern() {
    std::cout << "Testing public/private lock pattern..." << std::endl;
    
    ThreadSafeCounter counter;
    std::atomic<bool> test_running{true};
    std::atomic<int> operations_completed{0};
    
    // Thread 1: Increment operations
    std::thread incrementer([&]() {
        while (test_running) {
            counter.increment();
            counter.incrementTwice(); // Tests calling unlocked methods
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Thread 2: Decrement operations
    std::thread decrementer([&]() {
        while (test_running) {
            counter.decrement();
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Thread 3: Read operations
    std::thread reader([&]() {
        while (test_running) {
            counter.getValue();
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Thread 4: Complex operations
    std::thread complex([&]() {
        while (test_running) {
            counter.getAndReset(); // Tests multiple unlocked calls
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });
    
    // Run test for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
    test_running = false;
    
    // Wait for threads to complete
    incrementer.join();
    decrementer.join();
    reader.join();
    complex.join();
    
    std::cout << "PASS: Threading pattern test completed successfully" << std::endl;
    std::cout << "      Operations completed: " << operations_completed << std::endl;
    std::cout << "      Final counter value: " << counter.getValue() << std::endl;
}

/**
 * Demonstrate the anti-pattern that would cause deadlocks
 */
class BadThreadSafeCounter {
public:
    BadThreadSafeCounter() : m_count(0) {}
    
    void increment() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_count++;
    }
    
    int getValue() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count;
    }
    
    // BAD: This would deadlock with regular mutex
    // (works with recursive_mutex but violates the pattern)
    void incrementTwice() {
        std::lock_guard<std::mutex> lock(m_mutex);
        // BAD: Calling public method that tries to acquire same lock
        // increment(); // This would deadlock!
        
        // Instead, we should call unlocked version or duplicate code
        m_count++; // Duplicated code - not ideal
        m_count++; // Duplicated code - not ideal
    }
    
private:
    mutable std::mutex m_mutex; // Note: NOT recursive_mutex
    int m_count;
};

void demonstrate_anti_pattern() {
    std::cout << "Demonstrating why the public/private pattern is needed..." << std::endl;
    
    BadThreadSafeCounter counter;
    
    // This works fine
    counter.increment();
    std::cout << "After increment: " << counter.getValue() << std::endl;
    
    // This also works (doesn't call other public methods)
    counter.incrementTwice();
    std::cout << "After incrementTwice: " << counter.getValue() << std::endl;
    
    std::cout << "PASS: Anti-pattern demonstration completed" << std::endl;
    std::cout << "      (Note: Real deadlock would occur if incrementTwice called increment())" << std::endl;
}

int main() {
    try {
        test_threading_pattern();
        demonstrate_anti_pattern();
        
        std::cout << std::endl;
        std::cout << "=== Threading Pattern Guidelines ===" << std::endl;
        std::cout << "1. Public methods acquire locks and call private _unlocked versions" << std::endl;
        std::cout << "2. Private _unlocked methods assume locks are already held" << std::endl;
        std::cout << "3. Internal method calls use _unlocked versions to avoid deadlocks" << std::endl;
        std::cout << "4. Use RAII lock guards for exception safety" << std::endl;
        std::cout << "5. Document lock acquisition order to prevent deadlocks" << std::endl;
        std::cout << std::endl;
        
        std::cout << "All threading pattern tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}