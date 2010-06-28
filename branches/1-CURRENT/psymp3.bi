/' psymp3.bi: #define switches for PsyMP3 '/
/'
    $Id $

    This file is part of PsyMP3.

    PsyMP3 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    PsyMP3 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PsyMP3; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
'/

#Ifndef __PSYMP3_BI__
#Define __PSYMP3_BI__

#Include Once "psymp3-release.bi"

#Include Once "fmod.bi"
#Include Once "crt.bi"
#Include Once "crt/stdlib.bi"
#Include Once "crt/sys/types.bi"
#Include Once "crt/stddef.bi"
'#Include Once "ext/containers/queue.bi"
#Include Once "libxml/xmlreader.bi"
#Include Once "libxml/xmlwriter.bi"
#Include Once "freetype2/freetype.bi"
#Include Once "vorbis/vorbisfile.bi"
#Include Once "mpg123.bi"
#Include Once "SDL/SDL.bi"
'#Include Once "id3tag.bi"
'#include once "jpeg.bi"
#Ifdef __FB_WIN32__
#Define WIN_INCLUDEALL
#Include Once "windows.bi"
#Include Once "win/winsock2.bi"
#Else
#Include Once "crt/netinet/in.bi"
#Include Once "crt/arpa/inet.bi"
#Include Once "crt/netdb.bi"
#Include Once "crt/sys/socket.bi"
#Include Once "crt/errno.bi"
#Define TRUE	1
#Define FALSE	0
#Define SIGINT 3
#EndIf


/' Misc library headers '/
#Include Once "wshelper.bi"
#include Once "vbcompat.bi"
#Include Once "md5.bi"
#Include Once "fbgfx.bi"

Using FB

#Define USE_ASM 1 

/' Local headers '/

#Ifdef LIBSEVEN
#Include Once "libseven.bi"
#EndIf

#If Defined(__FB_LINUX__) And Not Defined(I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX) 
#Error PSYMP3 IS KNOWN TO BE BROKEN ON LINUX. I REFUSE TO HELP YOU IF IT BREAKS.
#Error YOU HAVE BEEN WARNED. DANGER LIES AHEAD. TURN BACK NOW AND USE PSYMP3 ON WINDOWS.
#Error Define the symbol I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX to disable this checkpoint.
#EndIf

#If Not Defined(Boolean)
	#Define Boolean integer
#EndIf

Using FB

Type complex_t
	re As Double ' real
	im As Double ' imaginary
End Type

Enum PSYMP3_COMMANDS
	PSYMP3_PLAY_NEXT
	PSYMP3_PLAY_PREV
	PSYMP3_PAUSE
	PSYMP3_PLAY
	PSYMP3_STOP ' alias for kill
End Enum

Union tagText
	As Byte Ptr ascii
	As Short Ptr utf16
End Union

Type extendedFileInfoStruct
	As ZString Ptr filename
	As ZString Ptr metadata
	As ZString Ptr ret
	As Integer retlen
End Type

Type extendedFileInfoStructW
	As WString Ptr filename
	As WString Ptr metadata
	As WString Ptr ret
	As Integer retlen
End Type

Type CopyData
	dwData As Integer
	cbData As Integer
	lpData As Any Ptr
End Type

/'
Type yetiplay_t
	screen_width  As Integer = 1024    '
	screen_height As Integer = 768     '
	buffer_count  As Integer = 2048*2  ' samples in a buffer
	fourier_size  As Integer = 2048*1  ' number of samples fourier will use
End Type
'/

Type yetiplay_t
	screen_width  As Integer   '
	screen_height As Integer   '
	buffer_count  As Integer   ' samples in a buffer
	fourier_size  As Integer   ' number of samples fourier will use
End Type

Declare Function plGetPosition Alias "plGetPosition" () As Integer
Declare Function plGetEntries Alias "plGetEntries" () As Integer

Common Shared As String mp3artist, mp3name, mp3album, mp3file
Common Shared As WString * 1024 mp3artistW, mp3nameW, mp3albumW, mp3fileW
Common Shared stream As FSOUND_STREAM Ptr
Common Shared IsPaused As Integer
Common Shared doRepeat As Integer
Common Shared doCommand As Integer
Common Shared As String lastfm_username, lastfm_password, lastfm_sessionkey
Common Shared songstart As Integer ' Song start time in UNIX format.
#Ifdef __FB_WIN32__
Common Shared WAWindow As HWND
Common Shared MainWnd As HWND
Common Shared As HWND hWnd
#Else
Common Shared As Integer hWnd
#EndIf
Common Shared songlength As Integer

Common Shared As yetiplay_t yetiplay

Extern "c"
declare Function getpid () As Integer
#Ifndef __FB_WIN32__
Declare Sub _Exit Alias "_Exit" (ByVal errcode As Integer)
declare function kill_ alias "kill" (ByVal pid As pid_t, ByVal sig As Integer) As Integer
#EndIf
#Ifdef __FB_WIN32__
Declare Function wsprintfW Alias "wsprintfW" (buf As WString Ptr, fmt As WString Ptr, ...) As Integer 
#EndIf
Declare Function dirname Alias "dirname" (path As ZString Ptr) As ZString Ptr 
End Extern

#Ifdef __FB_WIN32__
#Inclib "dir"
#EndIf

#Include Once "quadfft.bi"
#Include Once "winamp-ipc.bi"
#Include Once "ui.bi"
#Include Once "quadfft.bi"
#Include Once "scrobble.bi"
#Include Once "lastfm.bi"
#Include Once "playlist.bi"
#Include Once "strings.bi"
#Include Once "freetype2.bi"
#Include Once "multiput.bi"
#Include Once "tagutil.bi"
#Include Once "cpuid.bi"
#Include Once "msnmsgr.bi"
#Include Once "yetiplay.bi"

#EndIf /' __PSYMP3_BI__ '/