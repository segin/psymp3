/*
 * PropertyManager.cpp - MPRIS D-Bus property management
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace MPRIS {

PropertyManager::PropertyManager(Player *player)
    : m_player(player), m_length_us(0),
      m_status(PsyMP3::MPRIS::PlaybackStatus::Stopped),
      m_loop_status(PsyMP3::MPRIS::LoopStatus::None),
      m_position_us(0),
      m_position_timestamp(std::chrono::steady_clock::now()),
      m_can_go_next(false), m_can_go_previous(false),
      m_can_control(true) // Generally true for media players
      ,
      m_metadata_valid(false) {
  // Initialize with empty metadata
  clearMetadata_unlocked();
}

PropertyManager::~PropertyManager() {
  // Clean shutdown - no special cleanup needed as we don't own the Player
}

void PropertyManager::updateMetadata(const std::string &artist,
                                     const std::string &title,
                                     const std::string &album) {
  std::lock_guard<std::mutex> lock(m_mutex);
  updateMetadata_unlocked(artist, title, album);
}

void PropertyManager::updatePlaybackStatus(
    PsyMP3::MPRIS::PlaybackStatus status) {
  std::lock_guard<std::mutex> lock(m_mutex);
  updatePlaybackStatus_unlocked(status);
}

void PropertyManager::updatePosition(uint64_t position_us) {
  std::lock_guard<std::mutex> lock(m_mutex);
  updatePosition_unlocked(position_us);
}

void PropertyManager::updateLoopStatus(PsyMP3::MPRIS::LoopStatus status) {
  std::lock_guard<std::mutex> lock(m_mutex);
  updateLoopStatus_unlocked(status);
}

std::string PropertyManager::getPlaybackStatus() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getPlaybackStatus_unlocked();
}

std::map<std::string, PsyMP3::MPRIS::DBusVariant>
PropertyManager::getMetadata() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getMetadata_unlocked();
}

uint64_t PropertyManager::getPosition() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getPosition_unlocked();
}

PsyMP3::MPRIS::LoopStatus PropertyManager::getLoopStatus() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getLoopStatus_unlocked();
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

std::map<std::string, PsyMP3::MPRIS::DBusVariant>
PropertyManager::getAllProperties() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getAllProperties_unlocked();
}

// Private unlocked implementations

void PropertyManager::updateMetadata_unlocked(const std::string &artist,
                                              const std::string &title,
                                              const std::string &album) {
  m_artist = artist;
  m_title = title;
  m_album = album;
  m_metadata_valid = true;

  // Generate a simple track ID based on the metadata
  if (!title.empty()) {
    m_track_id =
        "/org/mpris/MediaPlayer2/Track/" +
        std::to_string(std::hash<std::string>{}(artist + title + album));
  } else {
    m_track_id.clear();
  }

  // TODO: In a real implementation, we might query the Player for track length
  // For now, we'll leave m_length_us as is or set it separately
}

void PropertyManager::updatePlaybackStatus_unlocked(
    PsyMP3::MPRIS::PlaybackStatus status) {
  m_status.store(status, std::memory_order_relaxed);

  // Update position timestamp when status changes to maintain accurate
  // interpolation
  m_position_timestamp = std::chrono::steady_clock::now();
}

void PropertyManager::updatePosition_unlocked(uint64_t position_us) {
  m_position_us = position_us;
  m_position_timestamp = std::chrono::steady_clock::now();
}

void PropertyManager::updateLoopStatus_unlocked(PsyMP3::MPRIS::LoopStatus status) {
  m_loop_status = status;
}

std::string PropertyManager::getPlaybackStatus_unlocked() const {
  return PsyMP3::MPRIS::playbackStatusToString(
      m_status.load(std::memory_order_relaxed));
}

std::map<std::string, PsyMP3::MPRIS::DBusVariant>
PropertyManager::getMetadata_unlocked() const {
  PsyMP3::MPRIS::MPRISMetadata metadata = buildMetadataStruct_unlocked();
  return metadata.toDBusDict();
}

uint64_t PropertyManager::getPosition_unlocked() const {
  return interpolatePosition_unlocked();
}

PsyMP3::MPRIS::LoopStatus PropertyManager::getLoopStatus_unlocked() const {
  return m_loop_status;
}

uint64_t PropertyManager::getLength_unlocked() const { return m_length_us; }

bool PropertyManager::canGoNext_unlocked() const {
  // TODO: In a real implementation, query the Player/Playlist for next track
  // availability
  return m_can_go_next;
}

bool PropertyManager::canGoPrevious_unlocked() const {
  // TODO: In a real implementation, query the Player/Playlist for previous
  // track availability
  return m_can_go_previous;
}

bool PropertyManager::canSeek_unlocked() const {
  // Check if current stream supports seeking via Player
  return m_player && m_player->canSeek();
}

bool PropertyManager::canControl_unlocked() const { return m_can_control; }

void PropertyManager::clearMetadata_unlocked() {
  m_artist.clear();
  m_title.clear();
  m_album.clear();
  m_track_id.clear();
  m_length_us = 0;
  m_art_url.clear();
  m_metadata_valid = false;
}

std::map<std::string, PsyMP3::MPRIS::DBusVariant>
PropertyManager::getAllProperties_unlocked() const {
  std::map<std::string, PsyMP3::MPRIS::DBusVariant> properties;

  // Use insert() instead of operator[] to avoid ARM ABI warning about
  // parameter passing changes in GCC 7.1 for std::map with complex value types

  // Playback status
  properties.insert(
      std::make_pair(std::string("PlaybackStatus"),
                     PsyMP3::MPRIS::DBusVariant(getPlaybackStatus_unlocked())));

  // Loop status
  properties.insert(std::make_pair(
      std::string("LoopStatus"),
      PsyMP3::MPRIS::DBusVariant(
          PsyMP3::MPRIS::loopStatusToString(getLoopStatus_unlocked()))));

  // Rate (playback rate, always 1.0 for now)
  properties.insert(
      std::make_pair(std::string("Rate"), PsyMP3::MPRIS::DBusVariant(1.0)));

  // Shuffle (not implemented, always false)
  properties.insert(std::make_pair(std::string("Shuffle"),
                                   PsyMP3::MPRIS::DBusVariant(false)));

  // Metadata
  auto metadata_dict = getMetadata_unlocked();
  properties.insert(std::make_pair(std::string("Metadata"),
                                   PsyMP3::MPRIS::DBusVariant(metadata_dict)));

  // Volume (not implemented, use 1.0)
  properties.insert(
      std::make_pair(std::string("Volume"), PsyMP3::MPRIS::DBusVariant(1.0)));

  // Position
  properties.insert(std::make_pair(
      std::string("Position"), PsyMP3::MPRIS::DBusVariant(static_cast<int64_t>(
                                   getPosition_unlocked()))));

  // Minimum and maximum rates
  properties.insert(std::make_pair(std::string("MinimumRate"),
                                   PsyMP3::MPRIS::DBusVariant(1.0)));
  properties.insert(std::make_pair(std::string("MaximumRate"),
                                   PsyMP3::MPRIS::DBusVariant(1.0)));

  // Control capabilities
  properties.insert(
      std::make_pair(std::string("CanGoNext"),
                     PsyMP3::MPRIS::DBusVariant(canGoNext_unlocked())));
  properties.insert(
      std::make_pair(std::string("CanGoPrevious"),
                     PsyMP3::MPRIS::DBusVariant(canGoPrevious_unlocked())));
  properties.insert(
      std::make_pair(std::string("CanPlay"),
                     PsyMP3::MPRIS::DBusVariant(canControl_unlocked())));
  properties.insert(
      std::make_pair(std::string("CanPause"),
                     PsyMP3::MPRIS::DBusVariant(canControl_unlocked())));
  properties.insert(std::make_pair(
      std::string("CanSeek"), PsyMP3::MPRIS::DBusVariant(canSeek_unlocked())));
  properties.insert(
      std::make_pair(std::string("CanControl"),
                     PsyMP3::MPRIS::DBusVariant(canControl_unlocked())));

  return properties;
}

PsyMP3::MPRIS::MPRISMetadata
PropertyManager::buildMetadataStruct_unlocked() const {
  PsyMP3::MPRIS::MPRISMetadata metadata;

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
  // If we're playing, interpolate position based on elapsed time since last
  // update
  if (m_status.load(std::memory_order_relaxed) ==
      PsyMP3::MPRIS::PlaybackStatus::Playing) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - m_position_timestamp);

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

} // namespace MPRIS
} // namespace PsyMP3
