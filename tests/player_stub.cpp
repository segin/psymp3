/*
 * player_stub.cpp - Stub implementations for Player methods needed by MethodHandler
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

// Stub implementations for Player methods used by MethodHandler
// These are only used for testing and don't need to do anything meaningful

bool Player::play() {
    return true; // Always succeed for testing
}

bool Player::pause() {
    return true; // Always succeed for testing
}

bool Player::stop() {
    return true; // Always succeed for testing
}

bool Player::playPause() {
    return true; // Always succeed for testing
}

void Player::nextTrack(unsigned long count) {
    (void)count; // Suppress unused parameter warning
    // Do nothing for testing
}

void Player::prevTrack() {
    // Do nothing for testing
}

void Player::seekTo(unsigned long position_ms) {
    (void)position_ms; // Suppress unused parameter warning
    // Do nothing for testing
}

void Player::synthesizeUserEvent(int event_type, void* data1, void* data2) {
    (void)event_type; (void)data1; (void)data2; // Suppress unused parameter warnings
    // Do nothing for testing
}