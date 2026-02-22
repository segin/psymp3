/*
 * player_stub.cpp - Stub implementations for Player methods needed by MethodHandler
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

// Stub implementations for Player methods used by MethodHandler
// These are only used for testing and don't need to do anything meaningful

// Global variable to track loop mode for testing
LoopMode g_last_loop_mode = LoopMode::None;

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

void Player::nextTrack(size_t count) {
    (void)count; // Suppress unused parameter warning
    // Do nothing for testing
}

void Player::prevTrack() {
    // Do nothing for testing
}

bool Player::canGoNext() const {
    return true; // Always return true for testing
}

bool Player::canGoPrevious() const {
    return true; // Always return true for testing
}

void Player::seekTo(unsigned long position_ms) {
    (void)position_ms; // Suppress unused parameter warning
    // Do nothing for testing
}

void Player::synthesizeUserEvent(int event_type, void* data1, void* data2) {
    (void)event_type; (void)data1; (void)data2; // Suppress unused parameter warnings
    // Do nothing for testing
}

void Player::setVolume(double volume) {
    (void)volume; // Suppress unused parameter warning
    // Do nothing for testing
}

double Player::getVolume() const {
    return 1.0; // Return dummy volume (1.0 = 100%)
}

void Player::showMPRISError(const std::string& error) {
    (void)error;
}

void Player::setLoopMode(LoopMode mode) {
    g_last_loop_mode = mode;
}

LoopMode Player::getLoopMode() const {
    return g_last_loop_mode;
}
