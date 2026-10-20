/*
 * MacNowPlaying.mm - macOS Now Playing and media keys (PsyMP3::Core)
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
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

/*
 * The MediaPlayer framework's MPNowPlayingInfoCenter carries what the
 * system shows as playing; MPRemoteCommandCenter delivers the media keys,
 * AirPods, Touch Bar and Control Centre buttons. Built with ARC, on macOS
 * only, and kept apart from every other translation unit (see
 * src/core/Makefile.am), like the file dialog.
 */

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

#include "core/MacNowPlaying.h"

using PsyMP3::Core::MacNowPlaying;

namespace {

/** @brief Where commands go; null when not started. */
MacNowPlaying::CommandHandler g_handler = nullptr;

/** @brief Each registered command with the token removeTarget: needs, as [command, token]. */
NSMutableArray* g_targets = nil;

/** @brief The cover last given, to build its artwork only when it changes. */
std::vector<uint8_t> g_last_artwork;

/** @brief The artwork built from g_last_artwork, or nil. */
MPMediaItemArtwork* g_artwork = nil;

/**
 * @brief A UTF-8 std::string as an NSString.
 * @param text The text.
 * @return It, or @"" if it is not valid UTF-8.
 */
NSString* toNSString(const std::string& text)
{
    NSString* s = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
    return s ? s : @"";
}

/**
 * @brief Enable @p command and pass it to the handler as @p which.
 * @param command The system's command.
 * @param which   What the player is told.
 */
void addCommand(MPRemoteCommand* command, MacNowPlaying::Command which)
{
    command.enabled = YES;
    id token = [command addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* /*event*/) {
        if (!g_handler) return MPRemoteCommandHandlerStatusCommandFailed;
        g_handler(which, 0);
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [g_targets addObject:@[command, token]];
}

/**
 * @brief The artwork for @p bytes, rebuilt only when they differ from the last.
 * @param bytes An image file's bytes, or none.
 * @return The artwork, or nil.
 */
MPMediaItemArtwork* artworkFor(const std::vector<uint8_t>& bytes)
{
    if (bytes == g_last_artwork) return g_artwork;
    g_last_artwork = bytes;
    g_artwork = nil;
    if (bytes.empty()) return nil;
    NSImage* image = [[NSImage alloc] initWithData:[NSData dataWithBytes:bytes.data() length:bytes.size()]];
    if (!image) return nil;
    g_artwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:image.size
                                                requestHandler:^NSImage*(CGSize /*size*/) { return image; }];
    return g_artwork;
}

} // namespace

namespace PsyMP3 {
namespace Core {

/**
 * @brief Register for the system's playback commands.
 * @param handler Where they go.
 */
void MacNowPlaying::start(CommandHandler handler)
{
    if (g_handler) stop();
    g_handler = handler;
    g_targets = [NSMutableArray array];
    MPRemoteCommandCenter* commands = [MPRemoteCommandCenter sharedCommandCenter];
    addCommand(commands.playCommand, Command::Play);
    addCommand(commands.pauseCommand, Command::Pause);
    addCommand(commands.togglePlayPauseCommand, Command::TogglePlayPause);
    addCommand(commands.stopCommand, Command::Stop);
    addCommand(commands.nextTrackCommand, Command::Next);
    addCommand(commands.previousTrackCommand, Command::Previous);

    // Dragging the position in Control Centre: a seek, to the time it gives.
    MPRemoteCommand* seek = commands.changePlaybackPositionCommand;
    seek.enabled = NO; // until a track that can be seeked plays (update())
    id token = [seek addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
        if (!g_handler || ![event isKindOfClass:[MPChangePlaybackPositionCommandEvent class]]) {
            return MPRemoteCommandHandlerStatusCommandFailed;
        }
        const NSTimeInterval seconds = static_cast<MPChangePlaybackPositionCommandEvent*>(event).positionTime;
        g_handler(Command::SeekTo, seconds > 0 ? static_cast<uint64_t>(seconds * 1000.0) : 0);
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [g_targets addObject:@[seek, token]];
}

/**
 * @brief Unregister the commands and clear the system's Now Playing.
 */
void MacNowPlaying::stop()
{
    for (NSArray* pair in g_targets) {
        MPRemoteCommand* command = pair[0];
        [command removeTarget:pair[1]];
        command.enabled = NO;
    }
    g_targets = nil;
    g_handler = nullptr;
    MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
    center.nowPlayingInfo = nil;
    center.playbackState = MPNowPlayingPlaybackStateStopped;
    g_last_artwork.clear();
    g_artwork = nil;
}

/**
 * @brief Tell the system what plays, where it is, and whether it plays.
 * @param artist      The artist.
 * @param title       The title.
 * @param album       The album.
 * @param length_ms   The length; 0 for a live stream.
 * @param position_ms The position.
 * @param state       Playing, paused or stopped.
 * @param artwork     The cover's bytes, or none.
 */
void MacNowPlaying::update(const std::string& artist, const std::string& title, const std::string& album,
                           uint64_t length_ms, uint64_t position_ms, State state,
                           const std::vector<uint8_t>& artwork)
{
    MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
    if (state == State::Stopped) {
        center.nowPlayingInfo = nil;
        center.playbackState = MPNowPlayingPlaybackStateStopped;
        return;
    }

    NSMutableDictionary* info = [NSMutableDictionary dictionary];
    info[MPNowPlayingInfoPropertyMediaType] = @(MPNowPlayingInfoMediaTypeAudio);
    if (!title.empty()) info[MPMediaItemPropertyTitle] = toNSString(title);
    if (!artist.empty()) info[MPMediaItemPropertyArtist] = toNSString(artist);
    if (!album.empty()) info[MPMediaItemPropertyAlbumTitle] = toNSString(album);
    const bool live = length_ms == 0;
    info[MPNowPlayingInfoPropertyIsLiveStream] = @(live);
    if (!live) info[MPMediaItemPropertyPlaybackDuration] = @(static_cast<double>(length_ms) / 1000.0);
    // With the rate, the system runs the clock on from the elapsed time itself.
    info[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(static_cast<double>(position_ms) / 1000.0);
    info[MPNowPlayingInfoPropertyPlaybackRate] = @(state == State::Playing ? 1.0 : 0.0);
    if (MPMediaItemArtwork* art = artworkFor(artwork)) info[MPMediaItemPropertyArtwork] = art;

    center.nowPlayingInfo = info;
    center.playbackState = state == State::Playing ? MPNowPlayingPlaybackStatePlaying
                                                   : MPNowPlayingPlaybackStatePaused;
    [MPRemoteCommandCenter sharedCommandCenter].changePlaybackPositionCommand.enabled = live ? NO : YES;
}

} // namespace Core
} // namespace PsyMP3
