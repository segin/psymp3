#ifndef PROPERTYMANAGER_H
#define PROPERTYMANAGER_H

#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <atomic>

// Forward declarations
class Player;

namespace PsyMP3 {
namespace MPRIS {
    enum class PlaybackStatus;
    struct DBusVariant;
    struct MPRISMetadata;
}
}

/**
 * PropertyManager - Thread-safe caching and management of MPRIS properties
 * 
 * This class follows the public/private lock pattern established in the threading
 * safety guidelines. All public methods acquire locks and call private _unlocked
 * implementations. Internal method calls use the _unlocked versions.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. PropertyManager::m_mutex (this class)
 * 2. Player locks (when calling into Player methods)
 */

namespace PsyMP3 {
namespace MPRIS {

class PropertyManager {
public:
    /**
     * Constructor
     * @param player Pointer to Player instance for state queries (non-owning)
     */
    explicit PropertyManager(Player* player);
    
    /**
     * Destructor - ensures clean shutdown
     */
    ~PropertyManager();
    
    // Public API - acquires locks and calls private implementations
    
    /**
     * Update cached metadata with new track information
     * @param artist Artist name
     * @param title Track title  
     * @param album Album name
     * @param length_us Track length in microseconds (0 if unknown)
     */
    void updateMetadata(const std::string& artist, const std::string& title, const std::string& album, uint64_t length_us = 0);
    
    /**
     * Update cached playback status
     * @param status New playback status
     */
    void updatePlaybackStatus(PsyMP3::MPRIS::PlaybackStatus status);
    
    /**
     * Update cached position with current timestamp
     * @param position_us Position in microseconds
     */
    void updatePosition(uint64_t position_us);
    
    /**
     * Update cached loop status
     * @param status New loop status
     */
    void updateLoopStatus(PsyMP3::MPRIS::LoopStatus status);

    /**
     * Update shuffle state
     * @param shuffle New shuffle state
     */
    void updateShuffle(bool shuffle);

    /**
     * Update volume
     * @param volume New volume (0.0 - 1.0)
     * @return true if changed
     */
    bool updateVolume(double volume);

    /**
     * Get current playback status as string for D-Bus
     * @return Playback status string ("Playing", "Paused", "Stopped")
     */
    std::string getPlaybackStatus() const;
    
    /**
     * Get current metadata as D-Bus dictionary
     * @return Map of metadata properties for D-Bus response
     */
    std::map<std::string, PsyMP3::MPRIS::DBusVariant> getMetadata() const;
    
    /**
     * Get current position with timestamp-based interpolation
     * @return Current position in microseconds
     */
    uint64_t getPosition() const;

    /**
     * Get current loop status
     * @return Current loop status
     */
    PsyMP3::MPRIS::LoopStatus getLoopStatus() const;

    /**
     * Get current shuffle state
     * @return Shuffle state
     */
    bool getShuffle() const;

    /**
     * Get current volume
     * @return Volume (0.0 - 1.0)
     */
    double getVolume() const;
    
    /**
     * Get track length in microseconds
     * @return Track length or 0 if unknown
     */
    uint64_t getLength() const;
    
    /**
     * Check if we can go to next track
     * @return true if next track is available
     */
    bool canGoNext() const;
    
    /**
     * Check if we can go to previous track  
     * @return true if previous track is available
     */
    bool canGoPrevious() const;
    
    /**
     * Check if seeking is supported
     * @return true if seeking is supported
     */
    bool canSeek() const;
    
    /**
     * Check if playback control is available
     * @return true if playback can be controlled
     */
    bool canControl() const;
    
    /**
     * Clear all cached metadata
     */
    void clearMetadata();
    
    /**
     * Get all MPRIS properties as D-Bus dictionary
     * @return Complete property map for D-Bus GetAll response
     */
    std::map<std::string, PsyMP3::MPRIS::DBusVariant> getAllProperties() const;

private:
    // Private implementations - assume locks are already held
    
    void updateMetadata_unlocked(const std::string& artist, const std::string& title, const std::string& album, uint64_t length_us);
    void updatePlaybackStatus_unlocked(PsyMP3::MPRIS::PlaybackStatus status);
    void updatePosition_unlocked(uint64_t position_us);
    void updateLoopStatus_unlocked(PsyMP3::MPRIS::LoopStatus status);
    void updateShuffle_unlocked(bool shuffle);
    bool updateVolume_unlocked(double volume);
    
    std::string getPlaybackStatus_unlocked() const;
    std::map<std::string, PsyMP3::MPRIS::DBusVariant> getMetadata_unlocked() const;
    uint64_t getPosition_unlocked() const;
    PsyMP3::MPRIS::LoopStatus getLoopStatus_unlocked() const;
    bool getShuffle_unlocked() const;
    double getVolume_unlocked() const;
    uint64_t getLength_unlocked() const;
    
    bool canGoNext_unlocked() const;
    bool canGoPrevious_unlocked() const;
    bool canSeek_unlocked() const;
    bool canControl_unlocked() const;
    
    void clearMetadata_unlocked();
    std::map<std::string, PsyMP3::MPRIS::DBusVariant> getAllProperties_unlocked() const;
    
    // Helper methods for property conversion
    PsyMP3::MPRIS::MPRISMetadata buildMetadataStruct_unlocked() const;
    uint64_t interpolatePosition_unlocked() const;
    
    // Thread synchronization
    mutable std::mutex m_mutex;
    
    // Player reference (non-owning)
    Player* m_player;
    
    // Cached property state
    std::string m_artist;
    std::string m_title;
    std::string m_album;
    std::string m_track_id;
    uint64_t m_length_us;
    std::string m_art_url;
    
    // Playback state with atomic access for lock-free reads where safe
    std::atomic<PsyMP3::MPRIS::PlaybackStatus> m_status;
    
    // Loop status
    PsyMP3::MPRIS::LoopStatus m_loop_status;

    bool m_shuffle;
    double m_volume{1.0};

    // Position tracking with timestamp-based interpolation
    uint64_t m_position_us;
    std::chrono::steady_clock::time_point m_position_timestamp;
    
    // Control capabilities cache
    bool m_can_seek;
    bool m_can_control;
    
    // Track if metadata has been set
    bool m_metadata_valid;
};

} // namespace MPRIS
} // namespace PsyMP3
#endif // PROPERTYMANAGER_H