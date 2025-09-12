# MPRIS Troubleshooting Guide

## Overview

This guide helps diagnose and resolve common issues with MPRIS (Media Player Remote Interfacing Specification) integration in PsyMP3.

## Common Issues

### 1. MPRIS Not Working

**Symptoms:**
- Desktop media controls don't respond to PsyMP3
- No PsyMP3 entry in media control panels
- Third-party applications can't control PsyMP3

**Diagnosis:**
1. Check if MPRIS was compiled in:
   ```bash
   ldd psymp3 | grep dbus
   ```
   If no D-Bus libraries are listed, MPRIS support wasn't compiled in.

2. Check debug output:
   ```bash
   ./psymp3 --debug 2>&1 | grep mpris
   ```

**Solutions:**

**If MPRIS wasn't compiled in:**
- Ensure D-Bus development libraries are installed:
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libdbus-1-dev
  
  # Fedora/RHEL
  sudo dnf install dbus-devel
  
  # Arch Linux
  sudo pacman -S dbus
  ```
- Rebuild PsyMP3:
  ```bash
  ./generate-configure.sh
  ./configure
  make clean && make
  ```

**If MPRIS was compiled but not working:**
- Check if D-Bus session bus is running:
  ```bash
  echo $DBUS_SESSION_BUS_ADDRESS
  ```
  Should return a valid D-Bus address.

- Test D-Bus connectivity:
  ```bash
  dbus-send --session --dest=org.freedesktop.DBus --type=method_call --print-reply /org/freedesktop/DBus org.freedesktop.DBus.ListNames
  ```

### 2. Connection Errors

**Symptoms:**
- "MPRIS initialization failed" in debug output
- "D-Bus connection failed" errors
- MPRIS works intermittently

**Common Error Messages:**
- `Failed to connect to D-Bus session bus`
- `D-Bus connection lost`
- `Unable to register MPRIS service`

**Solutions:**

**For session bus connection issues:**
1. Ensure you're running in a desktop session with D-Bus:
   ```bash
   ps aux | grep dbus-daemon
   ```

2. Check D-Bus session environment:
   ```bash
   env | grep DBUS
   ```

3. Try starting D-Bus session manually:
   ```bash
   eval `dbus-launch --sh-syntax`
   ./psymp3 --debug
   ```

**For service registration issues:**
1. Check if another MPRIS service is using the same name:
   ```bash
   dbus-send --session --dest=org.freedesktop.DBus --type=method_call --print-reply /org/freedesktop/DBus org.freedesktop.DBus.ListNames | grep mpris
   ```

2. Kill conflicting processes if found:
   ```bash
   pkill -f "org.mpris.MediaPlayer2.psymp3"
   ```

### 3. Partial Functionality

**Symptoms:**
- Some media controls work, others don't
- Metadata not updating
- Position tracking not working

**Diagnosis:**
1. Enable detailed MPRIS logging:
   ```bash
   ./psymp3 --debug 2>&1 | grep -E "(mpris|dbus)"
   ```

2. Test specific MPRIS methods:
   ```bash
   # Test play/pause
   dbus-send --session --dest=org.mpris.MediaPlayer2.psymp3 --type=method_call /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.PlayPause
   
   # Test metadata retrieval
   dbus-send --session --dest=org.mpris.MediaPlayer2.psymp3 --type=method_call --print-reply /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Get string:org.mpris.MediaPlayer2.Player string:Metadata
   ```

**Solutions:**

**For metadata issues:**
- Ensure audio files have proper tags
- Check if TagLib is working correctly:
  ```bash
  ./psymp3 --debug /path/to/audio/file 2>&1 | grep -E "(artist|title|album)"
  ```

**For position tracking issues:**
- Verify seeking works in PsyMP3 directly
- Check if position updates are being sent:
  ```bash
  dbus-monitor --session "type='signal',interface='org.freedesktop.DBus.Properties'"
  ```

### 4. Performance Issues

**Symptoms:**
- PsyMP3 becomes sluggish when MPRIS is active
- High CPU usage
- Audio stuttering

**Diagnosis:**
1. Monitor MPRIS-related threads:
   ```bash
   top -H -p $(pgrep psymp3)
   ```

2. Check for excessive D-Bus traffic:
   ```bash
   dbus-monitor --session | grep -E "(psymp3|mpris)"
   ```

**Solutions:**

**For high CPU usage:**
- Reduce signal emission frequency by modifying position update interval
- Check for signal emission loops in debug output
- Ensure no external applications are polling MPRIS properties excessively

**For audio stuttering:**
- Verify MPRIS operations are not blocking audio thread
- Check debug output for lock contention warnings
- Consider disabling MPRIS temporarily to isolate the issue

### 5. Desktop Environment Specific Issues

#### GNOME

**Common Issues:**
- Media controls in top bar not responding
- Notifications not showing track changes

**Solutions:**
1. Restart GNOME Shell:
   ```bash
   killall -3 gnome-shell
   ```

2. Check GNOME media controls extension:
   ```bash
   gnome-extensions list | grep -i media
   ```

#### KDE Plasma

**Common Issues:**
- Media player widget not detecting PsyMP3
- Keyboard media keys not working

**Solutions:**
1. Restart Plasma:
   ```bash
   kquitapp5 plasmashell && kstart5 plasmashell
   ```

2. Check media player widget configuration in system settings

#### XFCE

**Common Issues:**
- Panel media controls not working
- Keyboard shortcuts not responding

**Solutions:**
1. Check if xfce4-pulseaudio-plugin is installed
2. Verify keyboard shortcuts in Settings → Keyboard → Application Shortcuts

## Debug Information Collection

### Enabling Debug Output

Run PsyMP3 with debug output to collect diagnostic information:

```bash
./psymp3 --debug --logfile mpris_debug.log /path/to/audio/file
```

### Useful Debug Commands

1. **Check D-Bus service registration:**
   ```bash
   dbus-send --session --dest=org.freedesktop.DBus --type=method_call --print-reply /org/freedesktop/DBus org.freedesktop.DBus.ListNames | grep psymp3
   ```

2. **Monitor MPRIS signals:**
   ```bash
   dbus-monitor --session "type='signal',sender='org.mpris.MediaPlayer2.psymp3'"
   ```

3. **Test MPRIS methods manually:**
   ```bash
   # Play
   dbus-send --session --dest=org.mpris.MediaPlayer2.psymp3 --type=method_call /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Play
   
   # Get current status
   dbus-send --session --dest=org.mpris.MediaPlayer2.psymp3 --type=method_call --print-reply /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Get string:org.mpris.MediaPlayer2.Player string:PlaybackStatus
   ```

4. **Check for MPRIS-related processes:**
   ```bash
   ps aux | grep -E "(psymp3|dbus|mpris)"
   ```

### Log Analysis

Look for these patterns in debug output:

**Successful initialization:**
```
mpris: MPRIS initialized successfully
mpris: D-Bus connection established
mpris: Service registered: org.mpris.MediaPlayer2.psymp3
```

**Connection issues:**
```
mpris: MPRIS initialization failed: <error message>
mpris: D-Bus connection failed: <error details>
mpris: Failed to register service: <registration error>
```

**Runtime errors:**
```
mpris: Connection lost, attempting reconnection
mpris: Method call failed: <method> - <error>
mpris: Signal emission failed: <signal> - <error>
```

## Advanced Troubleshooting

### Memory Issues

If you suspect memory leaks or corruption:

1. **Run with Valgrind:**
   ```bash
   valgrind --leak-check=full --track-origins=yes ./psymp3 /path/to/audio/file
   ```

2. **Monitor memory usage:**
   ```bash
   watch -n 1 'ps -o pid,vsz,rss,comm -p $(pgrep psymp3)'
   ```

### Threading Issues

If you suspect deadlocks or race conditions:

1. **Enable threading debug output:**
   ```bash
   ./psymp3 --debug 2>&1 | grep -E "(lock|mutex|thread)"
   ```

2. **Check for deadlocks with GDB:**
   ```bash
   gdb -p $(pgrep psymp3)
   (gdb) thread apply all bt
   ```

### D-Bus Protocol Analysis

For deep D-Bus protocol issues:

1. **Capture D-Bus traffic:**
   ```bash
   dbus-monitor --session > dbus_traffic.log &
   ./psymp3 /path/to/audio/file
   # Perform problematic operations
   kill %1  # Stop dbus-monitor
   ```

2. **Analyze message patterns:**
   ```bash
   grep -E "(method_call|method_return|error|signal)" dbus_traffic.log
   ```

## Getting Help

### Information to Include in Bug Reports

When reporting MPRIS issues, include:

1. **System Information:**
   - Operating system and version
   - Desktop environment and version
   - D-Bus version: `dbus-daemon --version`

2. **Build Information:**
   - Configure options used
   - Compiler version
   - Library versions (especially libdbus)

3. **Debug Output:**
   - Complete debug log with `--debug` flag
   - D-Bus monitor output during the issue
   - Any error messages from system logs

4. **Reproduction Steps:**
   - Exact steps to reproduce the issue
   - Audio file formats that trigger the problem
   - Desktop environment specific actions

### Useful System Commands

```bash
# System information
uname -a
lsb_release -a  # On Linux
echo $DESKTOP_SESSION
echo $XDG_CURRENT_DESKTOP

# D-Bus information
dbus-daemon --version
echo $DBUS_SESSION_BUS_ADDRESS
systemctl --user status dbus

# PsyMP3 build information
ldd psymp3
./psymp3 --version  # If version flag exists
```

## Known Limitations

1. **Single Instance**: Only one PsyMP3 instance can register MPRIS service
2. **Desktop Environment**: Some features may not work in minimal window managers
3. **D-Bus Version**: Requires D-Bus 1.0 or later
4. **Threading**: MPRIS operations may add slight latency to Player operations

## See Also

- [MPRIS Architecture Documentation](mpris-architecture.md)
- [Threading Safety Guidelines](threading-safety-patterns.md)
- [PsyMP3 Testing Guide](../TESTING.md)
- [MPRIS Specification](https://specifications.freedesktop.org/mpris-spec/latest/)