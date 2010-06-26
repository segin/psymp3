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
#Ifdef __FB_WIN32__
#define WIN_INCLUDEALL
#include once "windows.bi" 
#else
#define SIGINT 3
#EndIf

/' Misc library headers '/
#Include Once "wshelper.bi"
#include Once "vbcompat.bi"
#Include Once "md5.bi"
#include once "fbgfx.bi"

' #Define USE_ASM 1 

/' Local headers '/
#Include Once "scrobble.bi"
#Include Once "lastfm.bi"
#Include Once "playlist.bi"
#Include Once "strings.bi"
#Include Once "freetype2.bi"
#Include Once "multiput.bi"
#Include Once "tagutil.bi"
#Include Once "cpuid.bi"
#Include Once "ui.bi"
#Ifdef __FB_WIN32__
#Include Once "msnmsgr.bi"
#Include Once "winamp-ipc.bi"
#EndIf /' __FB_WIN32__ '/

#If Defined(__FB_LINUX__) And Not Defined(I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX) 
#error PSYMP3 IS KNOWN TO BE BROKEN ON LINUX. I REFUSE TO HELP YOU IF IT BREAKS.
#error YOU HAVE BEEN WARNED. DANGER LIES AHEAD. TURN BACK NOW AND USE PSYMP3 ON WINDOWS.
#Error Define the symbol I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX to disable this checkpoint.
#EndIf

#If Not Defined(Boolean)
	#Define Boolean integer
#EndIf

Using FB

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

Declare Function plGetPosition Alias "plGetPosition" () As Integer
Declare Function plGetEntries Alias "plGetEntries" () As Integer

Common Shared As String mp3artist, mp3name, mp3album, mp3file
Common Shared As WString * 1024 mp3artistW, mp3nameW, mp3albumW, mp3fileW
Common Shared stream as FSOUND_STREAM Ptr
Common Shared IsPaused as Integer
Common Shared doRepeat As Integer
Common Shared doCommand As Integer
Common Shared As String lastfm_username, lastfm_password, lastfm_sessionkey
Common Shared songstart As Integer ' Song start time in UNIX format.
#ifdef __FB_WIN32__
Common Shared WAWindow As HWND
Common Shared MainWnd As HWND
Common Shared As HWND hWnd
#else 
Common Shared As Integer hWnd
#EndIf
Common Shared songlength As Integer

extern "c"
declare function getpid () as Integer
#ifndef __FB_WIN32__
Declare Sub _Exit Alias "_Exit" (ByVal errcode As Integer)
declare function kill_ alias "kill" (byval pid as pid_t, byval sig as integer) as Integer
#endif
#Ifdef __FB_WIN32__
Declare Function wsprintfW Alias "wsprintfW" (buf As WString ptr, fmt As WString Ptr, ...) As Integer 
#endif
Declare Function dirname Alias "dirname" (path As ZString Ptr) As ZString Ptr 
End Extern

#ifdef __FB_WIN32__
#Inclib "dir"
#EndIf

#EndIf /' __PSYMP3_BI__ '/