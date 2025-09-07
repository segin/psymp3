#include "psymp3.h"

PropertyManager::PropertyManager(Player* player)
    : m_player(player)
    , m_length_us(0)
    , m_status(MPRISTypes::PlaybackStatus::Stopped)
    , m_position_us(0)
    , m_position_timestamp(std::chrono::steady_clock::now())
    , m_can_go_next(false)
    , m_can_go_previous(false)
    , m_can_seek(false)
    , m_can_control(true)  // Generally true for media players
    , m_metadata_valid(false)
{
    // Initialize with empty metadata
    clearMetadata_unlocked();
}

PropertyManager::~PropertyManager() {
    // Clean shutdown - no special cleanup needed as we don't own the Player
}

void PropertyManager::updateMetadata(const std::string& artist, const std::string& title, const std::string& album) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updateMetadata_unlocked(artist, title, album);
}

void PropertyManager::updatePlaybackStatus(MPRISTypes::PlaybackStatus status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updatePlaybackStatus_unlocked(status);
}

void PropertyManager::updatePosition(uint64_t position_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updatePosition_unlocked(position_us);
}

std::string PropertyManager::getPlaybackStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getPlaybackStatus_unlocked();
}

std::map<std::string, MPRISTypes::DBusVariant> PropertyManager::getMetadata() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getMetadata_unlocked();
}

uint64_t PropertyManager::getPosition() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getPosition_unlocked();
}

uint64_t PropertyManager::getLength() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getLength_unlocked();
}

bool PropertyManager::canGoNext() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return canGoNext_unlocked();
}

bool PropertyManager::canGoPrevious() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return canGoPrevious_unlocked();
}

bool PropertyManager::canSeek() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return canSeek_unlocked();
}

bool PropertyManager::canControl() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return canControl_unlocked();
}

void PropertyManager::clearMetadata() {
    std::lock_guard<std::mutex> lock(m_mutex);
    clearMetadata_unlocked();
}

std::map<std::string, MPRISTypes::DBusVariant> PropertyManager::getAllProperties() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getAllProperties_unlocked();
}

// Private unlocked implementations

void PropertyManager::updateMetadata_unlocked(const std::string& artist, const std::string& title, const std::string& album) {
    m_artist = artist;
    m_title = title;
    m_album = album;
    m_metadata_valid = true;
    
    // Generate a simple track ID based on the metadata
    if (!title.empty()) {
        m_track_id = "/org/mpris/MediaPlayer2/Track/" + std::to_string(std::hash<std::string>{}(artist + title + album));
    } else {
        m_track_id.clear();
    }
    
    // TODO: In a real implementation, we might query the Player for track length
    // For now, we'll leave m_length_us as is or set it separately
}

void PropertyManager::updatePlaybackStatus_unlocked(MPRISTypes::PlaybackStatus status) {
    m_status.store(status, std::memory_order_relaxed);
    
    // Update position timestamp when status changes to maintain accurate interpolation
    m_position_timestamp = std::chrono::steady_clock::now();
}

void PropertyManager::updatePosition_unlocked(uint64_t position_us) {
    m_position_us = position_us;
    m_position_timestamp = std::chrono::steady_clock::now();
}

std::string PropertyManager::getPlaybackStatus_unlocked() const {
    return MPRISTypes::playbackStatusToString(m_status.load(std::memory_order_relaxed));
}

std::map<std::string, MPRISTypes::DBusVariant> PropertyManager::getMetadata_unlocked() const {
    MPRISTypes::MPRISMetadata metadata = buildMetadataStruct_unlocked();
    return metadata.toDBusDict();
}

uint64_t PropertyManager::getPosition_unlocked() const {
    return interpolatePosition_unlocked();
}

uint64_t PropertyManager::getLength_unlocked() const {
    return m_length_us;
}

bool PropertyManager::canGoNext_unlocked() const {
    // TODO: In a real implementation, query the Player/Playlist for next track availability
    return m_can_go_next;
}

bool PropertyManager::canGoPrevious_unlocked() const {
    // TODO: In a real implementation, query the Player/Playlist for previous track availability  
    return m_can_go_previous;
}

bool PropertyManager::canSeek_unlocked() const {
    // TODO: In a real implementation, check if current stream supports seeking
    return m_can_seek;
}

bool PropertyManager::canControl_unlocked() const {
    return m_can_control;
}

void PropertyManager::clearMetadata_unlocked() {
    m_artist.clear();
    m_title.clear();
    m_album.clear();
    m_track_id.clear();
    m_length_us = 0;
    m_art_url.clear();
    m_metadata_valid = false;
}

std::map<std::string, MPRISTypes::DBusVariant> PropertyManager::getAllProperties_unlocked() const {
    std::map<std::string, MPRISTypes::DBusVariant> properties;
    
    // Playback status
    properties["PlaybackStatus"] = MPRISTypes::DBusVariant(getPlaybackStatus_unlocked());
    
    // Loop status (for now, always None)
    properties["LoopStatus"] = MPRISTypes::DBusVariant(MPRISTypes::loopStatusToString(MPRISTypes::LoopStatus::None));
    
    // Rate (playback rate, always 1.0 for now)
    properties["Rate"] = MPRISTypes::DBusVariant(1.0);
    
    // Shuffle (not implemented, always false)
    properties["Shuffle"] = MPRISTypes::DBusVariant(false);
    
    // Metadata
    auto metadata_dict = getMetadata_unlocked();
    properties["Metadata"] = MPRISTypes::DBusVariant(std::string("metadata_dict")); // TODO: Handle dict-in-dict properly
    
    // Volume (not implemented, use 1.0)
    properties["Volume"] = MPRISTypes::DBusVariant(1.0);
    
    // Position
    properties["Position"] = MPRISTypes::DBusVariant(static_cast<int64_t>(getPosition_unlocked()));
    
    // Minimum and maximum rates
    properties["MinimumRate"] = MPRISTypes::DBusVariant(1.0);
    properties["MaximumRate"] = MPRISTypes::DBusVariant(1.0);
    
    // Control capabilities
    properties["CanGoNext"] = MPRISTypes::DBusVariant(canGoNext_unlocked());
    properties["CanGoPrevious"] = MPRISTypes::DBusVariant(canGoPrevious_unlocked());
    properties["CanPlay"] = MPRISTypes::DBusVariant(canControl_unlocked());
    properties["CanPause"] = MPRISTypes::DBusVariant(canControl_unlocked());
    properties["CanSeek"] = MPRISTypes::DBusVariant(canSeek_unlocked());
    properties["CanControl"] = MPRISTypes::DBusVariant(canControl_unlocked());
    
    return properties;
}

MPRISTypes::MPRISMetadata PropertyManager::buildMetadataStruct_unlocked() const {
    MPRISTypes::MPRISMetadata metadata;
    
    if (m_metadata_valid) {
        metadata.artist = m_artist;
        metadata.title = m_title;
        metadata.album = m_album;
        metadata.track_id = m_track_id;
        metadata.length_us = m_length_us;
        metadata.art_url = m_art_url;
    }
    
    return metadata;
}

uint64_t PropertyManager::interpolatePosition_unlocked() const {
    // If we're playing, interpolate position based on elapsed time since last update
    if (m_status.load(std::memory_order_relaxed) == MPRISTypes::PlaybackStatus::Playing) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - m_position_timestamp);
        
        uint64_t interpolated_position = m_position_us + elapsed.count();
        
        // Clamp to track length if we have it
        if (m_length_us > 0 && interpolated_position > m_length_us) {
            return m_length_us;
        }
        
        return interpolated_position;
    }
    
    // If paused or stopped, return the last known position
    return m_position_us;
}