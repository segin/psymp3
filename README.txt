PsyMP3 Version 1.1-CURRENT README
=========================================

1. Overview
2. Usage
3. Contacting the author

=========================================

1. OVERVIEW

PsyMP3 is a general, all-round, simple media player. PsyMP3 was named
for the bar spectrum visualization, which is a simple psychovisual display.

System Requirements:
	* WINDOWS AND REACTOS:
	- Microsoft Windows 95, 98, NT 4.0, 2000, XP, Server 2003/2008, 
	  Windows Vista, Windows 7
	- ReactOS 0.3 or later
	- Intel Pentium III or better, Core Solo/Atom or better recommended.
	- 800x600 16-bit SVGA or better (a ultra high-end 512MB PCI-E 16x card
	  WILL NOT make this run any better)
	- An additional 32MB of system RAM over your OS's requirement 
	  (128MB is good)
	- 1MB of disk space
	- 16-bit sound card (just about every sound card made in the last 15
	  years will work)
	- FMOD 3.7x (included with PsyMP3)
	- FreeType 2 (included with PsyMP3)
	
	[ Note: The Linux port is dead for now ]
	* GNU/LINUX:
	- Linux 2.4.16 or later (Older kernels will work, but why bother?)
	- FMOD 3.7x (a copy of libfmod.so is included with PsyMP3)
	- ALSA 0.9x or Open Sound System drivers (If the OSS on FreeBSD
	  when running a Linux binary of PsyMP3 under FreeBSD doesn't work,
	  this is a BUG)
	- GTK+-2.4 or later (you can probably get away with older)
	- a working X11 installation OR
	- A working Linux framebuffer with at least 24-bit color 
	- In either case, you still need Xlib and other base X libraries
	- 1GB of RAM (2GB is better, see README-Linux for why)
	- an i386 machine with a 400MHz CPU would be nice, 200MHz minimum

2. USAGE

PsyMP3 is a simple media player; in fact, it has very few features.
When started, you will be presented a series of file choosers.
You build an initial playlist by selecting files, one at a time.
Cancel out a chooser at any time to finish the playlist.

When playing, PsyMP3 is controlled entirely by keyboard.

Your keys are:
	SPACE	  - This pauses and resumes playback
	ESC or Q  - These close PsyMP3
	LEFT	  - This rewinds playback, 1.5 seconds at a time
	RIGHT	  - This fastforwards playback, 1.5 seconds at a time
	R	  - Restarts the playing track
	L	  - Loads a new track into this playlist slot temporarly
	I	  - Appends a new track to the end of the playlist
	B	  - Turns on "Repeat Track"
	Z	  - Displays framerate counter

If you want your played tracks to be scrobbled to Last.fm, create lastfm_config.txt
in the PsyMP3 directory. Enter your username on the first like and your password 
on the second, like so:

------------- file begins below -------------
username
password
------------- file ended above --------------

If you use various Winamp front-end software, such as the FoxyTunes toolbar for
Firefox, or the Winamp Toolbar, or even RndSkin, these can all be used to 
control PsyMP3, as if it were Winamp

If you use MSN Messenger or Pidgin, you can have Now Playing sent to your messenger.
See the PsyMP3 website at http://code.google.com/p/psymp3/ for more details.

3. CONTACTING THE AUTHOR

You may reach me at segin2005@gmail.com.

4. NOTES

<To be written.>