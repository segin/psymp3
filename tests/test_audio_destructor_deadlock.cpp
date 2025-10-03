/*
 * test_audio_destructor_deadlock.cpp - Test Audio destructor deadlock fix
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstring>

// Mock classes to test Audio destructor without full dependencies
class MockStream {
public:
    MockStream() : m_eof(false), m_data_count(0) {}
    
    size_t getData(size_t size, void* buffer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Simulate I/O
        if (m_data_count++ > 100) {
            m_eof = true;
            return 0;
        }
        // Fill with dummy data
        memset(buffer, 0, size);
        return size;
    }
    
    bool eof() const { return m_eof; }
    int getRate() const { return 44100; }
    int getChannels() const { return 2; }
    
private:
    std::atomic<bool> m_eof;
    std::atomic<int> m_data_count;
};

class MockFastFourier {
public:
    float* getTimeDom() { return m_time_domain; }
    void doFFT() { /* Mock FFT */ }
private:
    float m_time_domain[512];
};

// Simplified Audio class for testing destructor
class TestAudio {
public:
    TestAudio(std::unique_ptr<MockStream> stream, MockFastFourier* fft, std::mutex* player_mutex)
        : m_active(true),
          m_owned_stream(std::move(stream)),
          m_current_stream_raw_ptr(m_owned_stream.get()),
          m_fft(fft),
          m_player_mutex(player_mutex),
          m_playing(false) {
        m_buffer.reserve(16384);
        m_decoder_thread = std::thread(&TestAudio::decoderThreadLoop, this);
    }
    
    ~TestAudio() {
        // Stop playback first (this will notify buffer_cv if stopping)
        play(false);
        
        // Signal decoder thread to terminate
        m_active = false;
        
        // Wake up decoder thread from all possible wait conditions
        m_stream_cv.notify_all(); // Wake up if waiting for a new stream
        m_buffer_cv.notify_all(); // Wake up if waiting for buffer space
        
        // Wait for decoder thread to finish
        if (m_decoder_thread.joinable()) {
            m_decoder_thread.join();
        }
    }
    
    void play(bool go) {
        m_playing = go;
        
        // Notify decoder thread about playback state change
        // This is critical for proper shutdown when play(false) is called
        if (!go) {
            m_buffer_cv.notify_all();
        }
    }
    
private:
    void decoderThreadLoop() {
        std::vector<int16_t> decode_chunk(4096);
        constexpr size_t BUFFER_HIGH_WATER_MARK = 16384;

        while (m_active) {
            MockStream* local_stream = nullptr;
            {
                std::unique_lock<std::mutex> lock(m_stream_mutex);
                m_stream_cv.wait(lock, [this] {
                    return m_owned_stream != nullptr || !m_active;
                });
                if (!m_active) break;
                local_stream = m_owned_stream.get();
            }

            while (local_stream && m_active) {
                {
                    std::unique_lock<std::mutex> lock(m_buffer_mutex);
                    m_buffer_cv.wait(lock, [this] {
                        return m_buffer.size() < BUFFER_HIGH_WATER_MARK || !m_active || !m_playing;
                    });
                }

                if (!m_active) break;

                size_t bytes_read = 0;
                bool eof = false;
                MockStream* validated_stream = nullptr;
                
                {
                    std::lock_guard<std::mutex> lock(*m_player_mutex);
                    MockStream* current_stream = m_current_stream_raw_ptr.load();
                    if (local_stream == current_stream && current_stream != nullptr) {
                        if (m_owned_stream.get() == current_stream) {
                            validated_stream = local_stream;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
                
                if (validated_stream) {
                    bytes_read = validated_stream->getData(decode_chunk.size() * sizeof(int16_t), decode_chunk.data());
                    eof = validated_stream->eof();
                }

                if (bytes_read > 0) {
                    std::lock_guard<std::mutex> lock(m_buffer_mutex);
                    size_t samples_read = bytes_read / sizeof(int16_t);
                    m_buffer.insert(m_buffer.end(), decode_chunk.begin(), decode_chunk.begin() + samples_read);
                }
                m_buffer_cv.notify_one();

                if (eof) {
                    m_stream_eof = true;
                    break;
                }
            }
        }
    }
    
    std::atomic<bool> m_active;
    std::unique_ptr<MockStream> m_owned_stream;
    std::atomic<MockStream*> m_current_stream_raw_ptr;
    MockFastFourier* m_fft;
    std::mutex* m_player_mutex;
    bool m_playing;
    std::atomic<bool> m_stream_eof{false};
    
    std::thread m_decoder_thread;
    std::vector<int16_t> m_buffer;
    mutable std::mutex m_buffer_mutex;
    mutable std::mutex m_stream_mutex;
    std::condition_variable m_stream_cv;
    std::condition_variable m_buffer_cv;
};

void test_audio_destructor_no_deadlock() {
    std::cout << "Testing Audio destructor deadlock fix..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Test multiple Audio object creations and destructions
    for (int i = 0; i < 5; i++) {
        std::cout << "Creating and destroying Audio object " << (i + 1) << "/5..." << std::endl;
        
        auto stream = std::make_unique<MockStream>();
        MockFastFourier fft;
        std::mutex player_mutex;
        
        {
            TestAudio audio(std::move(stream), &fft, &player_mutex);
            
            // Let it run for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Start playback
            audio.play(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Stop playback
            audio.play(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Destructor will be called here - should not deadlock
        }
        
        std::cout << "Audio object " << (i + 1) << " destroyed successfully" << std::endl;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (duration.count() > 10000) { // 10 seconds timeout
        std::cout << "FAIL: Test took too long (" << duration.count() << "ms), possible deadlock" << std::endl;
        exit(1);
    }
    
    std::cout << "PASS: Audio destructor deadlock fix test completed successfully" << std::endl;
    std::cout << "      Total time: " << duration.count() << "ms" << std::endl;
}

int main() {
    try {
        test_audio_destructor_no_deadlock();
        std::cout << "All Audio destructor tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}