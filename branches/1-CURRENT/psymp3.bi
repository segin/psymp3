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

#define PSYMP3_VERSION "1-CURRENT"

#Include once "crt.bi"
#include once "crt/stdlib.bi"
#include once "crt/sys/types.bi"
#include once "crt/stddef.bi"
#include Once "libxml/xmlreader.bi"
#include Once "libxml/xmlwriter.bi"
#ifdef __FB_WIN32__
#define WIN_INCLUDEALL
#include once "windows.bi" 
#else
#define SIGINT 3
#EndIf
#include once "fmod.bi"

#Include Once "wshelper.bi"

#Include Once "scrobble.bi"
#Include Once "lastfm.bi"
#Include Once "playlist.bi"

' #Define USE_ASM 1 

#If Defined(__FB_LINUX__) And Not Defined(I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX) 
#error PSYMP3 IS KNOWN TO BE BROKEN ON LINUX. I REFUSE TO HELP YOU IF IT BREAKS.
#error YOU HAVE BEEN WARNED. DANGER LIES AHEAD. TURN BACK NOW AND USE PSYMP3 ON WINDOWS.
#Error Define the symbol I_UNDERSTAND_PSYMP3_IS_BROKEN_ON_LINUX to disable this checkpoint.
#EndIf

Declare Function getmp3artist Alias "getmp3artist" (stream As FSOUND_STREAM Ptr) As String
Declare Function getmp3name Alias "getmp3name" (stream As FSOUND_STREAM Ptr) As String
Declare Function getmp3artistW Alias "getmp3artistW" (stream As FSOUND_STREAM Ptr) As WString Ptr
Declare Function getmp3nameW Alias "getmp3nameW" (stream As FSOUND_STREAM Ptr) As WString Ptr
Declare Function MD5str Alias "MD5str" (chars As String) As String
Declare Function percent_encode Alias "percent_encode" (message As String) As String
Declare Function percent_encodeW Alias "percent_encodeW" (messageW As WString Ptr) As String

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

#EndIf