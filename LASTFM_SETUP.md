# Last.fm Scrobbling Setup Guide

PsyMP3 includes built-in Last.fm scrobbling support to automatically track your music listening history.

## Quick Setup

### 1. Create Configuration File
Create the file `~/.local/share/psymp3/lastfm.conf` with your Last.fm credentials:

```
# Last.fm configuration
username=your_lastfm_username
password=your_lastfm_password
```

**Note**: Use your actual Last.fm username and password (not email address).

### 2. Start Playing Music
Once configured, PsyMP3 will automatically:
- Submit "Now Playing" status when you start a track
- Scrobble tracks after you've listened to >50% or >4 minutes (whichever comes first)
- Only scrobble tracks longer than 30 seconds

## Configuration Details

### Configuration File Location
- **Linux/Unix**: `~/.local/share/psymp3/lastfm.conf`
- **Windows**: `%LOCALAPPDATA%\psymp3\lastfm.conf`

### Configuration Format
```ini
# Last.fm scrobbler configuration
username=your_username
password=your_password
# session_key=auto_generated  # Don't manually edit this line
```

The `session_key` is automatically generated and managed by PsyMP3.

## Debug Output

### Enable Runtime Debugging
To see Last.fm scrobbling debug information:

```bash
# Command line option
./psymp3 --debug-runtime your_music_files.flac

# Or press 'r' key during playback to toggle runtime debug
```

### Debug Output Examples
When debug is enabled, you'll see:
```
LastFM: Initializing Last.fm scrobbler
LastFM: Config file: /home/user/.local/share/psymp3/lastfm.conf
LastFM: Username loaded: your_username
LastFM: Configuration complete - scrobbling enabled
LastFM: Setting now playing: Artist Name - Track Title
LastFM: Added scrobble to queue: Artist Name - Track Title
LastFM: Queue size: 1
LastFM: Successfully submitted 1 scrobbles
```

## Scrobbling Behavior

### When Tracks Are Scrobbled
- **Minimum length**: Track must be >30 seconds
- **Minimum listen time**: Either >50% of track OR >4 minutes played
- **Once per track**: Each track is only scrobbled once per play session

### Offline Support
- Scrobbles are cached locally if Last.fm is unavailable
- Cached scrobbles are automatically submitted when connection is restored
- Cache location: `~/.local/share/psymp3/scrobble_cache.xml`

### Network Settings
- Uses multiple Last.fm API endpoints for redundancy:
  - `post.audioscrobbler.com`
  - `post2.audioscrobbler.com` 
  - `submissions.last.fm`
- Automatic retry on network failures
- 10-second timeout per request

## Troubleshooting

### No Scrobbles Appearing
1. **Check configuration**: Enable debug output to verify credentials are loaded
2. **Check Last.fm website**: Verify username/password work on last.fm
3. **Check network**: Ensure internet connection is working
4. **Check track length**: Only tracks >30 seconds are scrobbled
5. **Check listen time**: Must listen to >50% of track or >4 minutes

### Common Issues
- **"LastFM: Config file not found"**: Create the configuration file
- **"LastFM: Missing username or password"**: Check config file format
- **"LastFM: Cannot scrobble - not configured"**: Credentials not loaded properly
- **"LastFM: Handshake failed"**: Network issue or invalid credentials

### Getting Help
Enable debug output and check the console for detailed error messages:
```bash
./psymp3 --debug-runtime /path/to/music
```

## Privacy Notes
- Your Last.fm password is stored in plain text in the config file
- Session keys are cached to avoid repeated authentication
- Only track metadata (artist, title, album, length) is sent to Last.fm
- No file paths or personal information is transmitted