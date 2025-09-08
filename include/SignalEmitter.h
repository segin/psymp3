#ifndef SIGNALEMITTER_H
#define SIGNALEMITTER_H

#include "MPRISTypes.h"
#include "DBusConnectionManager.h"
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>

// Forward declarations for D-Bus types
struct DBusConnection;
struct DBusMessage;

namespace MPRISTypes {

/**
 * @brief Sends MPRIS property change signals asynchronously
 * 
 * This class handles asynchronous emission of MPRIS D-Bus signals without blocking
 * the calling thread. It follows the project's threading safety guidelines with the
 * public/private lock pattern and provides batching support for efficient signal emission.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. SignalEmitter::m_mutex (this class only uses one mutex)
 * 
 * The SignalEmitter uses a worker thread to process signals asynchronously, ensuring
 * that signal emission never blocks the main application thread or MPRIS operations.
 */
class SignalEmitter {
public:
    /**
     * @brief Constructor - initializes signal emitter with connection manager
     * @param connection Pointer to DBusConnectionManager (must remain valid during lifetime)
     */
    explicit SignalEmitter(DBusConnectionManager* connection);
    
    /**
     * @brief Destructor - ensures clean shutdown of worker thread
     */
    ~SignalEmitter();
    
    // Non-copyable and non-movable for thread safety
    SignalEmitter(const SignalEmitter&) = delete;
    SignalEmitter& operator=(const SignalEmitter&) = delete;
    SignalEmitter(SignalEmitter&&) = delete;
    SignalEmitter& operator=(SignalEmitter&&) = delete;
    
    /**
     * @brief Emit PropertiesChanged signal for MPRIS interface
     * @param interface D-Bus interface name (e.g., "org.mpris.MediaPlayer2.Player")
     * @param changed_properties Map of property names to new values
     * @return Result indicating if signal was queued successfully
     */
    Result<void> emitPropertiesChanged(const std::string& interface, 
                                     const std::map<std::string, DBusVariant>& changed_properties);
    
    /**
     * @brief Emit Seeked signal for position changes
     * @param position_us New position in microseconds
     * @return Result indicating if signal was queued successfully
     */
    Result<void> emitSeeked(uint64_t position_us);
    
    /**
     * @brief Start the signal emission worker thread
     * @return Result indicating success or error message
     */
    Result<void> start();
    
    /**
     * @brief Stop the signal emission worker thread
     * @param wait_for_completion If true, wait for all queued signals to be processed
     */
    void stop(bool wait_for_completion = true);
    
    /**
     * @brief Check if the signal emitter is currently running
     * @return true if worker thread is active
     */
    bool isRunning() const;
    
    /**
     * @brief Get the current queue size
     * @return Number of signals waiting to be processed
     */
    size_t getQueueSize() const;
    
    /**
     * @brief Check if the signal queue is full
     * @return true if queue has reached maximum capacity
     */
    bool isQueueFull() const;
    
    /**
     * @brief Get statistics about signal emission
     * @return Structure containing emission statistics
     */
    struct Statistics {
        uint64_t signals_queued = 0;
        uint64_t signals_sent = 0;
        uint64_t signals_failed = 0;
        uint64_t signals_dropped = 0;
        uint64_t batches_sent = 0;
    };
    
    Statistics getStatistics() const;
    
    /**
     * @brief Reset emission statistics
     */
    void resetStatistics();

private:
    // Private implementations - assume locks are already held
    Result<void> emitPropertiesChanged_unlocked(const std::string& interface, 
                                              const std::map<std::string, DBusVariant>& changed_properties);
    Result<void> emitSeeked_unlocked(uint64_t position_us);
    Result<void> start_unlocked();
    void stop_unlocked(bool wait_for_completion);
    bool isRunning_unlocked() const;
    size_t getQueueSize_unlocked() const;
    bool isQueueFull_unlocked() const;
    Statistics getStatistics_unlocked() const;
    void resetStatistics_unlocked();
    
    // Internal signal processing
    void signalWorkerLoop();
    void processSignalQueue_unlocked();
    bool processNextSignal_unlocked();
    void processBatchedSignals_unlocked();
    
    // Signal creation helpers
    Result<DBusMessagePtr> createPropertiesChangedMessage_unlocked(
        const std::string& interface, 
        const std::map<std::string, DBusVariant>& changed_properties);
    Result<DBusMessagePtr> createSeekedMessage_unlocked(uint64_t position_us);
    
    // D-Bus message sending
    Result<void> sendSignalMessage_unlocked(DBusMessage* message);
    
    // Queue management
    bool enqueueSignal_unlocked(std::function<void()> signal_func);
    void dropOldestSignals_unlocked(size_t count);
    
    // Batching support
    struct BatchedPropertiesChanged {
        std::string interface;
        std::map<std::string, DBusVariant> properties;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    void addToBatch_unlocked(const std::string& interface, 
                           const std::map<std::string, DBusVariant>& properties);
    bool shouldFlushBatch_unlocked() const;
    void flushBatch_unlocked();
    
    // Threading safety
    mutable std::mutex m_mutex;
    
    // Connection management
    DBusConnectionManager* m_connection;
    
    // Worker thread management
    std::thread m_signal_thread;
    std::atomic<bool> m_signal_thread_active{false};
    std::atomic<bool> m_shutdown_requested{false};
    
    // Signal queue
    std::queue<std::function<void()>> m_signal_queue;
    std::condition_variable m_signal_cv;
    
    // Batching state
    std::map<std::string, BatchedPropertiesChanged> m_batched_properties;
    std::chrono::steady_clock::time_point m_last_batch_flush;
    
    // Statistics
    Statistics m_statistics;
    
    // Configuration constants
    static constexpr size_t MAX_QUEUE_SIZE = 100;
    static constexpr size_t QUEUE_DROP_COUNT = 10; // Drop this many when queue is full
    static constexpr std::chrono::milliseconds BATCH_TIMEOUT{50}; // Flush batch after this delay
    static constexpr std::chrono::milliseconds WORKER_TIMEOUT{100}; // Worker thread wait timeout
    static constexpr size_t MAX_BATCH_SIZE = 10; // Maximum properties in a single batch
    
    // D-Bus constants
    static constexpr const char* MPRIS_INTERFACE = "org.mpris.MediaPlayer2";
    static constexpr const char* MPRIS_PLAYER_INTERFACE = "org.mpris.MediaPlayer2.Player";
    static constexpr const char* DBUS_PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";
    static constexpr const char* DBUS_OBJECT_PATH = "/org/mpris/MediaPlayer2";
};

} // namespace MPRISTypes

#endif // SIGNALEMITTER_H